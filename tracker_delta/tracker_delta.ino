#include "user_config.h"
#include "config.h"
#include "lvgl_port.h"
#include "esp_err.h"
#include "i2c_bsp.h"
#include "src/lcd_bl_bsp/lcd_bl_pwm_bsp.h"
#include <WiFi.h>
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>

char      WIFI_SSID[64]    = "";
char      WIFI_PASS[64]    = "";
char      boot_status[64]  = "INITIALISING...";

void setup()
{
    i2c_master_Init();
    Serial.begin(115200);
    lvgl_port_init();                   /* spawns LVGL task → shows boot screen */
    lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255);

    if (!loadWiFiConfig()) {
        strlcpy(boot_status, "SETUP MODE  Connect to DELTA-SETUP", sizeof(boot_status));
        startCaptivePortal();           /* blocks until form saved + restart */
    }

    if (!connectWiFi()) {
        strlcpy(boot_status, "WIFI FAILED  CHECK CREDENTIALS", sizeof(boot_status));
        delay(3000);
    } else {
        strlcpy(boot_status, "WIFI CONNECTED", sizeof(boot_status));
        delay(600);
    }

    lvgl_switch_to_dashboard();
    fetchWeather();
    fetchReceiver();
    fetchServer();
    fetchNearest();
}

void loop()
{
    static unsigned long last_weather_ms  = 0;
    static unsigned long last_recv_ms     = 0;
    static unsigned long last_server_ms   = 0;
    static unsigned long last_nearest_ms  = 0;
    unsigned long now = millis();
    if (now - last_weather_ms >= WEATHER_REFRESH_MS) {
        last_weather_ms = now;
        fetchWeather();
    }
    if (now - last_recv_ms >= RECEIVER_REFRESH_MS) {
        last_recv_ms = now;
        fetchReceiver();
    }
    if (now - last_server_ms >= SERVER_REFRESH_MS) {
        last_server_ms = now;
        fetchServer();
    }
    if (now - last_nearest_ms >= NEAREST_REFRESH_MS) {
        last_nearest_ms = now;
        fetchNearest();
    }

#if (Backlight_Testing == 1)
    setUpduty(LCD_PWM_MODE_255);
    delay(1500);
    setUpduty(LCD_PWM_MODE_175);
    delay(1500);
    setUpduty(LCD_PWM_MODE_125);
    delay(1500);
    setUpduty(LCD_PWM_MODE_0);
    delay(1500);
#endif
}
