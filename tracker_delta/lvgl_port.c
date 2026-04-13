#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl_port.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "user_config.h"
#include "driver/spi_master.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "src/axs15231b/esp_lcd_axs15231b.h"
#include "i2c_bsp.h"

static const char *TAG = "lvgl_port";
static SemaphoreHandle_t lvgl_mux = NULL;

/* ── Boot status string — written by .ino WiFi code, read by LVGL timer ───── */
extern char boot_status[64];

/* ── Screen handles ──────────────────────────────────────────────────────────  */
static lv_obj_t *g_boot_scr        = NULL;
static lv_obj_t *g_dash_scr        = NULL;
static lv_obj_t *g_boot_status_lbl = NULL;
static lv_obj_t *g_wx_lbl          = NULL;

/* ── Live-update handles for RECEIVER column value labels ────────────────────  */
#define RECV_ROWS 5
static lv_obj_t *g_recv_val[RECV_ROWS];   /* [0]=MSGS/MIN [1]=SIGNAL [2]=NOISE [3]=STRONG [4]=TRACKS */

/* ── Live-update handles for SERVER column value labels ──────────────────────  */
#define SERV_ROWS 6
static lv_obj_t *g_serv_val[SERV_ROWS];   /* [0]=STATUS [1]=UP [2]=REQS [3]=CACHE [4]=ROUTES [5]=ERRS */

/* ── Live-update handles for NEAREST column value labels ─────────────────────  */
#define NEAR_ROWS 6
static lv_obj_t *g_near_val[NEAR_ROWS];   /* [0]=CALL [1]=TYPE [2]=REG [3]=ROUTE [4]=DIST [5]=PHASE */

/* ── Placeholder data (replaced with live data once network is wired) ──────── */
static const char *adsb_fields[][2] = {
    {"MSGS/MIN", "1284"},        /* last1min.messages_valid       */
    {"SIGNAL",   "-18.3 dBFS"},  /* last1min.local.signal         */
    {"NOISE",    "-28.1 dBFS"},  /* last1min.local.noise          */
    {"STRONG",   "3"},           /* last1min.local.strong_signals */
    {"TRACKS",   "47"},          /* last1min.tracks.all           */
};
static const char *server_fields[][2] = {
    {"STATUS", "OK"},            /* reachable = OK               */
    {"UP",     "5d 3h"},         /* stats.uptime                 */
    {"REQS",   "12453"},         /* stats.totalRequests          */
    {"CACHE",  "87.3%"},         /* stats.cacheHitRate           */
    {"ROUTES", "1247"},          /* stats.knownRoutes            */
    {"ERRS",   "0"},             /* stats.errors                 */
};
static const char *nearest_fields[][2] = {
    {"CALL",   "QFA123"},        /* ac.flight                    */
    {"TYPE",   "B738"},          /* ac.t                         */
    {"REG",    "VH-VZR"},        /* ac.r                         */
    {"ROUTE",  "SYD-MEL"},       /* enriched route               */
    {"DIST",   "4.2 km"},        /* haversine                    */
    {"PHASE",  "CRUISING"},      /* derived                      */
};
static const char *weather_text =
    "SYDNEY  22\xc2\xb0C  Partly Cloudy  |  Wind: 12 km/h NE  |"
    "  Humidity: 68%  |  UV: 5 Moderate  |  Sunrise: 06:41  |  Sunset: 18:05";

static uint16_t *trans_buf_1 = NULL;
uint8_t *lvgl_dest = NULL;
static SemaphoreHandle_t flush_done_semaphore;

#define LCD_BIT_PER_PIXEL 16
#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))
#define BUFF_SIZE (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * BYTES_PER_PIXEL)

static const axs15231b_lcd_init_cmd_t lcd_init_cmds[] =
{
    {0x11, (uint8_t []){0x00}, 0, 100},
    {0x29, (uint8_t []){0x00}, 0, 100},
};

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io,
                                             esp_lcd_panel_io_event_data_t *edata,
                                             void *user_ctx)
{
    BaseType_t TaskWoken;
    xSemaphoreGiveFromISR(flush_done_semaphore, &TaskWoken);
    return false;
}

static void example_increase_lvgl_tick(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    lv_draw_sw_rgb565_swap(color_p, lv_area_get_width(area) * lv_area_get_height(area));
#if (Rotated == USER_DISP_ROT_90)
    lv_display_rotation_t rotation = lv_display_get_rotation(disp);
    lv_area_t rotated_area;
    if (rotation != LV_DISPLAY_ROTATION_0) {
        lv_color_format_t cf = lv_display_get_color_format(disp);
        rotated_area = *area;
        lv_display_rotate_area(disp, &rotated_area);
        uint32_t src_stride  = lv_draw_buf_width_to_stride(lv_area_get_width(area), cf);
        uint32_t dest_stride = lv_draw_buf_width_to_stride(lv_area_get_width(&rotated_area), cf);
        int32_t src_w = lv_area_get_width(area);
        int32_t src_h = lv_area_get_height(area);
        lv_draw_sw_rotate(color_p, lvgl_dest, src_w, src_h, src_stride, dest_stride, rotation, cf);
        area = &rotated_area;
    }

    const int flush_coun = (LVGL_SPIRAM_BUFF_LEN / LVGL_DMA_BUFF_LEN);
    const int offgap     = (EXAMPLE_LCD_V_RES / flush_coun);
    const int dmalen     = (LVGL_DMA_BUFF_LEN / 2);
    int offsetx1 = 0, offsety1 = 0;
    int offsetx2 = EXAMPLE_LCD_H_RES, offsety2 = offgap;
    uint16_t *map = (uint16_t *)lvgl_dest;
    xSemaphoreGive(flush_done_semaphore);
    for (int i = 0; i < flush_coun; i++) {
        xSemaphoreTake(flush_done_semaphore, portMAX_DELAY);
        memcpy(trans_buf_1, map, LVGL_DMA_BUFF_LEN);
        esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2, offsety2, trans_buf_1);
        offsety1 += offgap;
        offsety2 += offgap;
        map += dmalen;
    }
    xSemaphoreTake(flush_done_semaphore, portMAX_DELAY);
    lv_disp_flush_ready(disp);
#else
    const int flush_coun = (LVGL_SPIRAM_BUFF_LEN / LVGL_DMA_BUFF_LEN);
    const int offgap     = (EXAMPLE_LCD_V_RES / flush_coun);
    const int dmalen     = (LVGL_DMA_BUFF_LEN / 2);
    int offsetx1 = 0, offsety1 = 0;
    int offsetx2 = EXAMPLE_LCD_H_RES, offsety2 = offgap;
    uint16_t *map = (uint16_t *)color_p;
    xSemaphoreGive(flush_done_semaphore);
    for (int i = 0; i < flush_coun; i++) {
        xSemaphoreTake(flush_done_semaphore, portMAX_DELAY);
        memcpy(trans_buf_1, map, LVGL_DMA_BUFF_LEN);
        esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2, offsety2, trans_buf_1);
        offsety1 += offgap;
        offsety2 += offgap;
        map += dmalen;
    }
    xSemaphoreTake(flush_done_semaphore, portMAX_DELAY);
    lv_disp_flush_ready(disp);
#endif
}

static void TouchInputReadCallback(lv_indev_t *indev, lv_indev_data_t *indevData)
{
    uint8_t read_touchpad_cmd[11] = {0xb5, 0xab, 0xa5, 0x5a, 0x0, 0x0, 0x0, 0x0e, 0x0, 0x0, 0x0};
    uint8_t buff[32] = {0};
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_read_dev(disp_touch_dev_handle, read_touchpad_cmd, 11, buff, 32));
    uint16_t pointX = (((uint16_t)buff[2] & 0x0f) << 8) | (uint16_t)buff[3];
    uint16_t pointY = (((uint16_t)buff[4] & 0x0f) << 8) | (uint16_t)buff[5];
    if (buff[1] > 0 && buff[1] < 5) {
        if (pointX > EXAMPLE_LCD_V_RES) pointX = EXAMPLE_LCD_V_RES;
        if (pointY > EXAMPLE_LCD_H_RES) pointY = EXAMPLE_LCD_H_RES;
        indevData->state   = LV_INDEV_STATE_PRESSED;
        indevData->point.x = pointY;
        indevData->point.y = (EXAMPLE_LCD_V_RES - pointX);
    } else {
        indevData->state = LV_INDEV_STATE_RELEASED;
    }
}

static bool example_lvgl_lock(int timeout_ms)
{
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

static void example_lvgl_unlock(void)
{
    assert(lvgl_mux && "bsp_display_start must be called first");
    xSemaphoreGive(lvgl_mux);
}

void example_lvgl_port_task(void *arg)
{
    uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    for (;;) {
        if (example_lvgl_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            example_lvgl_unlock();
        }
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS)      task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

/* ── Boot status timer — polls boot_status[] every 200 ms ──────────────────── */
static void boot_status_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (g_boot_status_lbl)
        lv_label_set_text(g_boot_status_lbl, boot_status);
}

/* ── Animation helper: animate lv_obj opacity ───────────────────────────────  */
static void anim_opa_cb(void *obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

/* ── Animation helper: animate lv_obj width ─────────────────────────────────  */
static void anim_width_cb(void *obj, int32_t v)
{
    lv_obj_set_width((lv_obj_t *)obj, v);
}

/* ── Boot screen ─────────────────────────────────────────────────────────────  */
static void create_boot_screen(void)
{
    g_boot_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_boot_scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_boot_scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(g_boot_scr, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Phase 1: horizontal cyan scan bar sweeps left → right (0–400 ms) ── */
    lv_obj_t *scan_bar = lv_obj_create(g_boot_scr);
    lv_obj_set_pos(scan_bar, 0, 85);
    lv_obj_set_size(scan_bar, 0, 2);
    lv_obj_set_style_bg_color(scan_bar, lv_color_make(0, 200, 255), 0);
    lv_obj_set_style_bg_opa(scan_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scan_bar, 0, 0);
    lv_obj_set_style_radius(scan_bar, 0, 0);
    lv_obj_set_style_pad_all(scan_bar, 0, 0);

    lv_anim_t a_scan;
    lv_anim_init(&a_scan);
    lv_anim_set_var(&a_scan, scan_bar);
    lv_anim_set_exec_cb(&a_scan, anim_width_cb);
    lv_anim_set_values(&a_scan, 0, 640);
    lv_anim_set_duration(&a_scan, 400);
    lv_anim_set_path_cb(&a_scan, lv_anim_path_ease_out);
    lv_anim_start(&a_scan);

    /* Fade scan bar out after it completes (start_delay = 420ms) */
    lv_anim_t a_scan_fade;
    lv_anim_init(&a_scan_fade);
    lv_anim_set_var(&a_scan_fade, scan_bar);
    lv_anim_set_exec_cb(&a_scan_fade, anim_opa_cb);
    lv_anim_set_values(&a_scan_fade, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&a_scan_fade, 150);
    lv_anim_set_delay(&a_scan_fade, 420);
    lv_anim_start(&a_scan_fade);

    /* ── Phase 2: "OVERHEAD" fades in (delay 400 ms) ────────────────────── */
    lv_obj_t *lbl_oh = lv_label_create(g_boot_scr);
    lv_label_set_text(lbl_oh, "OVERHEAD");
    lv_obj_set_style_text_font(lbl_oh, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_oh, lv_color_make(253, 160, 0), 0);
    lv_obj_set_style_opa(lbl_oh, LV_OPA_TRANSP, 0);
    lv_obj_align(lbl_oh, LV_ALIGN_CENTER, 0, -28);

    lv_anim_t a_oh;
    lv_anim_init(&a_oh);
    lv_anim_set_var(&a_oh, lbl_oh);
    lv_anim_set_exec_cb(&a_oh, anim_opa_cb);
    lv_anim_set_values(&a_oh, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&a_oh, 300);
    lv_anim_set_delay(&a_oh, 400);
    lv_anim_start(&a_oh);

    /* "TRACKER" fades in (delay 520 ms) */
    lv_obj_t *lbl_tr = lv_label_create(g_boot_scr);
    lv_label_set_text(lbl_tr, "TRACKER");
    lv_obj_set_style_text_font(lbl_tr, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_tr, lv_color_make(0, 200, 255), 0);
    lv_obj_set_style_opa(lbl_tr, LV_OPA_TRANSP, 0);
    lv_obj_align(lbl_tr, LV_ALIGN_CENTER, 0, 0);

    lv_anim_t a_tr;
    lv_anim_init(&a_tr);
    lv_anim_set_var(&a_tr, lbl_tr);
    lv_anim_set_exec_cb(&a_tr, anim_opa_cb);
    lv_anim_set_values(&a_tr, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&a_tr, 300);
    lv_anim_set_delay(&a_tr, 520);
    lv_anim_start(&a_tr);

    /* "DELTA" subtitle fades in (delay 660 ms) */
    lv_obj_t *lbl_delta = lv_label_create(g_boot_scr);
    lv_label_set_text(lbl_delta, "DELTA");
    lv_obj_set_style_text_font(lbl_delta, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_delta, lv_color_make(100, 180, 255), 0);
    lv_obj_set_style_opa(lbl_delta, LV_OPA_TRANSP, 0);
    lv_obj_align(lbl_delta, LV_ALIGN_CENTER, 0, 22);

    lv_anim_t a_delta;
    lv_anim_init(&a_delta);
    lv_anim_set_var(&a_delta, lbl_delta);
    lv_anim_set_exec_cb(&a_delta, anim_opa_cb);
    lv_anim_set_values(&a_delta, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&a_delta, 250);
    lv_anim_set_delay(&a_delta, 660);
    lv_anim_start(&a_delta);

    /* ── Phase 3: spinner (left side) ───────────────────────────────────── */
    lv_obj_t *spinner = lv_spinner_create(g_boot_scr);
    lv_obj_set_size(spinner, 40, 40);
    lv_obj_align(spinner, LV_ALIGN_LEFT_MID, 16, 0);
    lv_obj_set_style_arc_color(spinner, lv_color_make(0, 200, 255), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, lv_color_make(20, 40, 80), LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(spinner, 3, LV_PART_MAIN);
    lv_obj_set_style_opa(spinner, LV_OPA_TRANSP, 0);

    lv_anim_t a_sp;
    lv_anim_init(&a_sp);
    lv_anim_set_var(&a_sp, spinner);
    lv_anim_set_exec_cb(&a_sp, anim_opa_cb);
    lv_anim_set_values(&a_sp, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&a_sp, 200);
    lv_anim_set_delay(&a_sp, 700);
    lv_anim_start(&a_sp);

    /* ── Status label (bottom centre) ───────────────────────────────────── */
    g_boot_status_lbl = lv_label_create(g_boot_scr);
    lv_label_set_text(g_boot_status_lbl, boot_status);
    lv_obj_set_style_text_font(g_boot_status_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(g_boot_status_lbl, lv_color_make(180, 220, 255), 0);
    lv_obj_set_style_opa(g_boot_status_lbl, LV_OPA_TRANSP, 0);
    lv_obj_align(g_boot_status_lbl, LV_ALIGN_BOTTOM_MID, 0, -6);

    lv_anim_t a_st;
    lv_anim_init(&a_st);
    lv_anim_set_var(&a_st, g_boot_status_lbl);
    lv_anim_set_exec_cb(&a_st, anim_opa_cb);
    lv_anim_set_values(&a_st, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&a_st, 200);
    lv_anim_set_delay(&a_st, 700);
    lv_anim_start(&a_st);

    /* Timer polls boot_status[] every 200 ms */
    lv_timer_create(boot_status_timer_cb, 200, NULL);

    lv_scr_load(g_boot_scr);
}

/* ── Dashboard ───────────────────────────────────────────────────────────────  */
static void build_dashboard(lv_obj_t *scr)
{
    lv_color_t c_bg    = lv_color_black();
    lv_color_t c_panel = lv_color_make( 14,  18,  35);
    lv_color_t c_cyan  = lv_color_make(  0, 200, 255);
    lv_color_t c_green = lv_color_make(  0, 210,  80);
    lv_color_t c_white = lv_color_white();
    lv_color_t c_div   = lv_color_make( 30,  80, 160);
    lv_color_t c_hdiv  = lv_color_make(  0, 140, 220);

    lv_obj_set_style_bg_color(scr, c_bg, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    const int COL_W = 212;
    const int COL_H = 151;
    const int GAP   = 2;
    const int ROW_H = 19;
    const int HDR_H = 21;
    const int PAD_X = 5;

    const char *titles[3] = {"RECEIVER", "SERVER", "NEAREST"};
    const char *(*cols[3])[2] = {adsb_fields, server_fields, nearest_fields};
    int col_rows[3] = {
        sizeof(adsb_fields)    / sizeof(adsb_fields[0]),
        sizeof(server_fields)  / sizeof(server_fields[0]),
        sizeof(nearest_fields) / sizeof(nearest_fields[0]),
    };

    for (int c = 0; c < 3; c++) {
        int x = c * (COL_W + GAP);

        lv_obj_t *panel = lv_obj_create(scr);
        lv_obj_set_pos(panel, x, 0);
        lv_obj_set_size(panel, COL_W, COL_H);
        lv_obj_set_style_bg_color(panel, c_panel, 0);
        lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(panel, 0, 0);
        lv_obj_set_style_radius(panel, 0, 0);
        lv_obj_set_style_pad_all(panel, 0, 0);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *hdr = lv_label_create(panel);
        lv_label_set_text(hdr, titles[c]);
        lv_obj_set_style_text_color(hdr, c_cyan, 0);
        lv_obj_set_style_text_font(hdr, &lv_font_montserrat_12, 0);
        lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 4);

        lv_obj_t *hdiv = lv_obj_create(panel);
        lv_obj_set_pos(hdiv, 0, HDR_H);
        lv_obj_set_size(hdiv, COL_W, 1);
        lv_obj_set_style_bg_color(hdiv, c_hdiv, 0);
        lv_obj_set_style_bg_opa(hdiv, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(hdiv, 0, 0);
        lv_obj_set_style_radius(hdiv, 0, 0);
        lv_obj_set_style_pad_all(hdiv, 0, 0);

        for (int r = 0; r < col_rows[c]; r++) {
            int y = HDR_H + 2 + r * ROW_H;

            lv_obj_t *key = lv_label_create(panel);
            lv_label_set_text(key, cols[c][r][0]);
            lv_obj_set_style_text_color(key, c_green, 0);
            lv_obj_set_style_text_font(key, &lv_font_montserrat_12, 0);
            lv_obj_set_pos(key, PAD_X, y);

            lv_obj_t *val = lv_label_create(panel);
            lv_label_set_text(val, cols[c][r][1]);
            lv_obj_set_style_text_color(val, c_white, 0);
            lv_obj_set_style_text_font(val, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_align(val, LV_ALIGN_TOP_RIGHT, -PAD_X, y);
            if (c == 0 && r < RECV_ROWS) g_recv_val[r] = val;
            if (c == 1 && r < SERV_ROWS) g_serv_val[r] = val;
            if (c == 2 && r < NEAR_ROWS) g_near_val[r] = val;

            if (r < col_rows[c] - 1) {
                lv_obj_t *sep = lv_obj_create(panel);
                lv_obj_set_pos(sep, PAD_X, y + ROW_H - 2);
                lv_obj_set_size(sep, COL_W - PAD_X * 2, 1);
                lv_obj_set_style_bg_color(sep, c_div, 0);
                lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
                lv_obj_set_style_border_width(sep, 0, 0);
                lv_obj_set_style_radius(sep, 0, 0);
                lv_obj_set_style_pad_all(sep, 0, 0);
            }
        }
    }

    /* Weather bar */
    lv_color_t c_wx_bg  = lv_color_make( 10,  24,  48);
    lv_color_t c_wx_txt = lv_color_make(180, 220, 255);

    lv_obj_t *wx_bar = lv_obj_create(scr);
    lv_obj_set_pos(wx_bar, 0, COL_H + 1);
    lv_obj_set_size(wx_bar, 640, 20);
    lv_obj_set_style_bg_color(wx_bar, c_wx_bg, 0);
    lv_obj_set_style_bg_opa(wx_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(wx_bar, 0, 0);
    lv_obj_set_style_radius(wx_bar, 0, 0);
    lv_obj_set_style_pad_all(wx_bar, 0, 0);
    lv_obj_clear_flag(wx_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *wx_sep = lv_obj_create(scr);
    lv_obj_set_pos(wx_sep, 0, COL_H);
    lv_obj_set_size(wx_sep, 640, 1);
    lv_obj_set_style_bg_color(wx_sep, c_hdiv, 0);
    lv_obj_set_style_bg_opa(wx_sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(wx_sep, 0, 0);
    lv_obj_set_style_radius(wx_sep, 0, 0);
    lv_obj_set_style_pad_all(wx_sep, 0, 0);

    g_wx_lbl = lv_label_create(wx_bar);
    lv_label_set_text(g_wx_lbl, weather_text);
    lv_obj_set_style_text_color(g_wx_lbl, c_wx_txt, 0);
    lv_obj_set_style_text_font(g_wx_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(g_wx_lbl, LV_ALIGN_LEFT_MID, 6, 0);
}

/* ── Called from .ino after WiFi is ready — switches to dashboard screen ────── */
void lvgl_switch_to_dashboard(void)
{
    if (!example_lvgl_lock(2000)) return;
    g_dash_scr = lv_obj_create(NULL);
    build_dashboard(g_dash_scr);
    lv_scr_load_anim(g_dash_scr, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
    example_lvgl_unlock();
}

/* ── Update RECEIVER column values (safe to call from any task) ──────────────  */
/* vals[5]: {msgs_min, signal, noise, strong, tracks} as formatted strings */
void lvgl_update_receiver(const char *vals[RECV_ROWS])
{
    if (!g_recv_val[0]) return;
    if (!example_lvgl_lock(500)) return;
    for (int i = 0; i < RECV_ROWS; i++) {
        if (vals[i]) lv_label_set_text(g_recv_val[i], vals[i]);
    }
    example_lvgl_unlock();
}

/* ── Update NEAREST column values (safe to call from any task) ──────────────  */
void lvgl_update_nearest(const char *vals[NEAR_ROWS])
{
    if (!g_near_val[0]) return;
    if (!example_lvgl_lock(500)) return;
    for (int i = 0; i < NEAR_ROWS; i++) {
        if (vals[i]) lv_label_set_text(g_near_val[i], vals[i]);
    }
    example_lvgl_unlock();
}

/* ── Update SERVER column values (safe to call from any task) ───────────────  */
void lvgl_update_server(const char *vals[SERV_ROWS])
{
    if (!g_serv_val[0]) return;
    if (!example_lvgl_lock(500)) return;
    for (int i = 0; i < SERV_ROWS; i++) {
        if (vals[i]) lv_label_set_text(g_serv_val[i], vals[i]);
    }
    example_lvgl_unlock();
}

/* ── Update the weather bar label (safe to call from any task) ───────────────  */
void lvgl_update_weather(const char *text)
{
    if (!g_wx_lbl) return;
    if (!example_lvgl_lock(500)) return;
    lv_label_set_text(g_wx_lbl, text);
    example_lvgl_unlock();
}

/* ── Hardware + LVGL init. Spawns LVGL task and shows boot screen. ───────────  */
void lvgl_port_init(void)
{
    flush_done_semaphore = xSemaphoreCreateBinary();
    assert(flush_done_semaphore);
    ESP_LOGI(TAG, "Initialize LCD RESET GPIO");

    gpio_config_t gpio_conf = {};
    gpio_conf.intr_type     = GPIO_INTR_DISABLE;
    gpio_conf.mode          = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask  = ((uint64_t)0X01 << EXAMPLE_PIN_NUM_LCD_RST);
    gpio_conf.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en    = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

    ESP_LOGI(TAG, "Initialize QSPI bus");
    spi_bus_config_t buscfg = {};
    buscfg.data0_io_num     = EXAMPLE_PIN_NUM_LCD_DATA0;
    buscfg.data1_io_num     = EXAMPLE_PIN_NUM_LCD_DATA1;
    buscfg.sclk_io_num      = EXAMPLE_PIN_NUM_LCD_PCLK;
    buscfg.data2_io_num     = EXAMPLE_PIN_NUM_LCD_DATA2;
    buscfg.data3_io_num     = EXAMPLE_PIN_NUM_LCD_DATA3;
    buscfg.max_transfer_sz  = LVGL_DMA_BUFF_LEN;
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t panel_io = NULL;
    esp_lcd_panel_handle_t    panel    = NULL;

    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num          = EXAMPLE_PIN_NUM_LCD_CS;
    io_config.dc_gpio_num          = -1;
    io_config.spi_mode             = 3;
    io_config.pclk_hz              = 40 * 1000 * 1000;
    io_config.trans_queue_depth    = 10;
    io_config.on_color_trans_done  = example_notify_lvgl_flush_ready;
    io_config.lcd_cmd_bits         = 32;
    io_config.lcd_param_bits       = 8;
    io_config.flags.quad_mode      = true;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_HOST, &io_config, &panel_io));

    axs15231b_vendor_config_t vendor_config = {};
    vendor_config.flags.use_qspi_interface = 1;
    vendor_config.init_cmds      = lcd_init_cmds;
    vendor_config.init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]);

    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num  = -1;
    panel_config.rgb_ele_order   = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel  = LCD_BIT_PER_PIXEL;
    panel_config.vendor_config   = &vendor_config;

    ESP_LOGI(TAG, "Install panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_axs15231b(panel_io, &panel_config, &panel));

    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_LCD_RST, 1));
    vTaskDelay(pdMS_TO_TICKS(30));
    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_LCD_RST, 0));
    vTaskDelay(pdMS_TO_TICKS(250));
    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_LCD_RST, 1));
    vTaskDelay(pdMS_TO_TICKS(30));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

    lv_init();

    lv_display_t *disp = lv_display_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
    lv_display_set_flush_cb(disp, example_lvgl_flush_cb);

    uint8_t *buffer_1 = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
    uint8_t *buffer_2 = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
    assert(buffer_1);
    assert(buffer_2);
    trans_buf_1 = (uint16_t *)heap_caps_malloc(LVGL_DMA_BUFF_LEN, MALLOC_CAP_DMA);
    assert(trans_buf_1);
    lv_display_set_buffers(disp, buffer_1, buffer_2, BUFF_SIZE, LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_user_data(disp, panel);
#if (Rotated == USER_DISP_ROT_90)
    lvgl_dest = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
#endif

    lv_indev_t *touch_indev = lv_indev_create();
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, TouchInputReadCallback);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    esp_timer_create_args_t lvgl_tick_timer_args = {};
    lvgl_tick_timer_args.callback = &example_increase_lvgl_tick;
    lvgl_tick_timer_args.name     = "lvgl_tick";
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    lvgl_mux = xSemaphoreCreateMutex();
    assert(lvgl_mux);
    xTaskCreatePinnedToCore(example_lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL, 0);

    if (example_lvgl_lock(-1)) {
        create_boot_screen();
        example_lvgl_unlock();
    }
}
