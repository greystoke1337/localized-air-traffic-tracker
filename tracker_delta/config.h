#ifndef CONFIG_H
#define CONFIG_H

#define PROXY_HOST              "api.overheadtracker.com"
#define PROXY_PORT              443
#define WIFI_AP_NAME            "DELTA-SETUP"
#define NVS_NAMESPACE           "tracker"
#define WIFI_CONNECT_TIMEOUT_S  20
#define HOME_LAT_DEFAULT        -33.8614f
#define HOME_LON_DEFAULT        151.1397f
#define LOCATION_NAME           "RUSSELL LEA"
#define WEATHER_REFRESH_MS      300000UL  /* 5 minutes */
#define RECEIVER_REFRESH_MS      30000UL  /* 30 seconds */
#define SERVER_REFRESH_MS        60000UL  /* 60 seconds */
#define NEAREST_REFRESH_MS       10000UL  /* 10 seconds */
#define PORTAL_TIMEOUT_MS       300000UL  /* 5-minute captive portal timeout */
#define MAX_AC_COUNT               100    /* max aircraft to scan in fetchNearest */

#endif
