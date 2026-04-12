// ─── Network: weather + receiver fetch ───────────────────────────────────────
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

void fetchWeather(void) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    char url[128];
    snprintf(url, sizeof(url),
        "https://%s/weather?lat=%.4f&lon=%.4f",
        PROXY_HOST, HOME_LAT_DEFAULT, HOME_LON_DEFAULT);

    if (!http.begin(client, url)) return;
    http.addHeader("User-Agent", "OverheadTracker-Delta/1.0");
    int code = http.GET();
    if (code != 200) { http.end(); return; }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok) return;

    float       temp  = doc["temp"]          | 0.0f;
    const char *cond  = doc["condition"]     | "Unknown";
    float       wind  = doc["wind_speed"]    | 0.0f;
    const char *wdir  = doc["wind_cardinal"] | "--";
    int         hum   = doc["humidity"]      | 0;
    float       uv    = doc["uv_index"]      | 0.0f;
    const char *sr    = doc["sunrise"]       | "--:--";
    const char *ss    = doc["sunset"]        | "--:--";

    char wx[256];
    snprintf(wx, sizeof(wx),
        "%s  %.0f\xc2\xb0""C  %s  |  Wind: %.0f km/h %s  |  Humidity: %d%%  |  UV: %.0f  |  Sunrise: %s  |  Sunset: %s",
        LOCATION_NAME, temp, cond, wind, wdir, hum, uv, sr, ss);

    lvgl_update_weather(wx);
}

static float haversine_km(float lat1, float lon1, float lat2, float lon2) {
    const float R = 6371.0f;
    float dlat = (lat2 - lat1) * (float)M_PI / 180.0f;
    float dlon = (lon2 - lon1) * (float)M_PI / 180.0f;
    float a = sinf(dlat / 2) * sinf(dlat / 2) +
              cosf(lat1 * (float)M_PI / 180.0f) * cosf(lat2 * (float)M_PI / 180.0f) *
              sinf(dlon / 2) * sinf(dlon / 2);
    return R * 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
}

static const char *flightPhase(int alt, float gs, int baro_rate) {
    if (gs < 30)                        return "GROUND";
    if (alt < 1500 && baro_rate > 300)  return "TAKING OFF";
    if (alt < 3000 && baro_rate < -300) return "LANDING";
    if (baro_rate > 256)                return "CLIMBING";
    if (baro_rate < -256)               return "DESCENDING";
    return "CRUISING";
}

void fetchNearest(void) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    char url[128];
    snprintf(url, sizeof(url),
        "https://%s/flights?lat=%.4f&lon=%.4f&radius=20",
        PROXY_HOST, HOME_LAT_DEFAULT, HOME_LON_DEFAULT);
    if (!http.begin(client, url)) return;
    http.addHeader("User-Agent", "OverheadTracker-Delta/1.0");
    int code = http.GET();
    if (code != 200) { http.end(); return; }

    String body = http.getString();
    http.end();

    JsonDocument filter;
    filter["ac"][0]["flight"]    = true;
    filter["ac"][0]["t"]         = true;
    filter["ac"][0]["r"]         = true;
    filter["ac"][0]["lat"]       = true;
    filter["ac"][0]["lon"]       = true;
    filter["ac"][0]["alt_baro"]  = true;
    filter["ac"][0]["gs"]        = true;
    filter["ac"][0]["baro_rate"] = true;
    filter["ac"][0]["dep"]       = true;
    filter["ac"][0]["arr"]       = true;

    JsonDocument doc;
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter)) != DeserializationError::Ok) return;

    JsonArray ac = doc["ac"];
    const char *none[NEAR_ROWS] = { "--", "--", "--", "--", "--", "--" };
    if (ac.isNull() || ac.size() == 0) { lvgl_update_nearest(none); return; }

    float best_dist = 1e9f;
    JsonObject best;
    for (JsonObject a : ac) {
        if (a["lat"].isNull() || a["lon"].isNull()) continue;
        float d = haversine_km(HOME_LAT_DEFAULT, HOME_LON_DEFAULT,
                               a["lat"].as<float>(), a["lon"].as<float>());
        if (d < best_dist) { best_dist = d; best = a; }
    }

    if (best.isNull()) { lvgl_update_nearest(none); return; }

    const char *call = best["flight"] | "--";
    const char *type = best["t"]      | "--";
    const char *reg  = best["r"]      | "--";
    const char *dep  = best["dep"]    | (const char *)NULL;
    const char *arr  = best["arr"]    | (const char *)NULL;
    int   alt        = best["alt_baro"]  | 0;
    float gs         = best["gs"]        | 0.0f;
    int   baro_rate  = best["baro_rate"] | 0;

    char s_call[12], s_type[8], s_reg[12], s_route[16], s_dist[12], s_phase[14];
    strlcpy(s_call, call, sizeof(s_call));
    /* trim trailing spaces from callsign */
    for (int i = strlen(s_call) - 1; i >= 0 && s_call[i] == ' '; i--) s_call[i] = '\0';
    strlcpy(s_type, type, sizeof(s_type));
    strlcpy(s_reg,  reg,  sizeof(s_reg));
    if (dep && arr) snprintf(s_route, sizeof(s_route), "%s-%s", dep, arr);
    else strlcpy(s_route, "--", sizeof(s_route));
    snprintf(s_dist,  sizeof(s_dist),  "%.1f km", best_dist);
    strlcpy(s_phase, flightPhase(alt, gs, baro_rate), sizeof(s_phase));

    const char *vals[NEAR_ROWS] = { s_call, s_type, s_reg, s_route, s_dist, s_phase };
    lvgl_update_nearest(vals);
}

void fetchServer(void) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    char url[64];
    snprintf(url, sizeof(url), "https://%s/stats", PROXY_HOST);
    if (!http.begin(client, url)) return;
    int code = http.GET();
    if (code != 200) { http.end(); return; }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok) return;

    char s_up[16], s_reqs[16], s_cache[16], s_routes[16], s_errs[16];
    strlcpy(s_up,     doc["uptime"]       | "--",   sizeof(s_up));
    snprintf(s_reqs,  sizeof(s_reqs),   "%d", (int)(doc["totalRequests"] | 0));
    strlcpy(s_cache,  doc["cacheHitRate"] | "--",   sizeof(s_cache));
    snprintf(s_routes, sizeof(s_routes), "%d", (int)(doc["knownRoutes"]   | 0));
    snprintf(s_errs,  sizeof(s_errs),   "%d", (int)(doc["errors"]         | 0));

    const char *vals[SERV_ROWS] = { "OK", s_up, s_reqs, s_cache, s_routes, s_errs };
    lvgl_update_server(vals);
}

void fetchReceiver(void) {
    WiFiClient client;
    HTTPClient http;
    if (!http.begin(client, "http://airplanes.local/tar1090/data/stats.json")) return;
    int code = http.GET();
    if (code != 200) { http.end(); return; }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok) return;

    JsonObject lm  = doc["last1min"];
    JsonObject loc = lm["local"];

    int   msgs   = lm["messages_valid"]    | 0;
    float signal = loc["signal"]           | 0.0f;
    float noise  = loc["noise"]            | 0.0f;
    int   strong = loc["strong_signals"]   | 0;
    int   tracks = lm["tracks"]["all"]     | 0;

    char s_msgs[16], s_sig[16], s_noise[16], s_strong[16], s_tracks[16];
    snprintf(s_msgs,   sizeof(s_msgs),   "%d",        msgs);
    snprintf(s_sig,    sizeof(s_sig),    "%.1f dBFS", signal);
    snprintf(s_noise,  sizeof(s_noise),  "%.1f dBFS", noise);
    snprintf(s_strong, sizeof(s_strong), "%d",        strong);
    snprintf(s_tracks, sizeof(s_tracks), "%d",        tracks);

    const char *vals[RECV_ROWS] = { s_msgs, s_sig, s_noise, s_strong, s_tracks };
    lvgl_update_receiver(vals);
}
