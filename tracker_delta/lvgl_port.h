#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#define RECV_ROWS 5
#define SERV_ROWS 6
#define NEAR_ROWS 6

void lvgl_port_init(void);
void lvgl_switch_to_dashboard(void);
void lvgl_update_receiver(const char *vals[RECV_ROWS]);
void lvgl_update_server(const char *vals[SERV_ROWS]);
void lvgl_update_nearest(const char *vals[NEAR_ROWS]);
void lvgl_update_weather(const char *text);

#ifdef __cplusplus
}
#endif

#endif
