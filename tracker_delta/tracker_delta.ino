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
    delay(2000);                        /* wait for host to open port */
    Serial.println("\n[BOOT] step 1 serial ok");     lvgl_port_init();                   /* spawns LVGL task → shows boot screen */
    Serial.println("[BOOT] step 2 lvgl ok");     lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255);

    Serial.println("[BOOT] step 3 bl ok");     if (!loadWiFiConfig()) {
        strlcpy(boot_status, "SETUP MODE  Connect to DELTA-SETUP", sizeof(boot_status));
        Serial.println("[WIFI] no config — captive portal");         startCaptivePortal();           /* blocks until form saved + restart */
    }

    Serial.println("[BOOT] step 4 wifi config loaded");     if (!connectWiFi()) {
        strlcpy(boot_status, "WIFI FAILED  CHECK CREDENTIALS", sizeof(boot_status));
        Serial.printf("[WIFI] failed ssid=%s\n", WIFI_SSID);         delay(3000);
    } else {
        strlcpy(boot_status, "WIFI CONNECTED", sizeof(boot_status));
        Serial.printf("[WIFI] ok ssid=%s rssi=%d ip=%s\n",
                      WIFI_SSID, WiFi.RSSI(), WiFi.localIP().toString().c_str());
                delay(600);
    }

    Serial.println("[BOOT] step 5 switching to dashboard");     lvgl_switch_to_dashboard();
    Serial.println("[BOOT] step 6 fetching weather");     fetchWeather();
    Serial.println("[BOOT] step 7 fetching receiver");     fetchReceiver();
    Serial.println("[BOOT] step 8 fetching server");     fetchServer();
    Serial.println("[BOOT] step 9 fetching nearest");     fetchNearest();
    Serial.println("[BOOT] step 10 setup complete"); }

void loop()
{
    static unsigned long last_weather_ms  = 0;
    static unsigned long last_recv_ms     = 0;
    static unsigned long last_server_ms   = 0;
    static unsigned long last_nearest_ms  = 0;
    static unsigned long last_heap_ms     = 0;
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
    if (now - last_heap_ms >= 30000UL) {
        last_heap_ms = now;
        logTs("SYS", "heap=%u maxblk=%u uptime=%lus",
              ESP.getFreeHeap(), ESP.getMaxAllocHeap(), now / 1000);
    }
    checkSerialCmd();

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
