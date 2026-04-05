#include <lvgl.h>
#include "lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static SemaphoreHandle_t lvgl_mux = NULL;

static void lvgl_task(void *arg) {
  while (1) {
    xSemaphoreTake(lvgl_mux, portMAX_DELAY);
    lv_timer_handler();
    xSemaphoreGive(lvgl_mux);
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void setup() {
  lvgl_port_init();

  // Backlight: GPIO 8, full brightness
  ledcAttach(8, 5000, 8);
  ledcWrite(8, 255);

  lvgl_mux = xSemaphoreCreateMutex();

  // Hello World label, centered on the 640×172 display
  xSemaphoreTake(lvgl_mux, portMAX_DELAY);
  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, "Hello World");
  lv_obj_center(label);
  xSemaphoreGive(lvgl_mux);

  xTaskCreate(lvgl_task, "lvgl", 8192, NULL, 2, NULL);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
