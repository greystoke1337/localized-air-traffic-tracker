// ─── Network: weather + receiver fetch ───────────────────────────────────────
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <assert.h>

/* Static docs: each allocated once on first deserialise, then reused.
   Eliminates the per-call String + JsonDocument heap alloc/free cycle. */
static JsonDocument s_weather_doc;
static JsonDocument s_nearest_doc;
static JsonDocument s_server_doc;
static JsonDocument s_recv_doc;

void fetchWeather(void) {
    assert(strlen(PROXY_HOST) > 0);
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
    if (code != 200) { logTs("WX", "HTTP %d", code); http.end(); return; }
    String wx_body = http.getString();
    http.end();
    if (deserializeJson(s_weather_doc, wx_body) != DeserializationError::Ok) {
        logTs("WX", "parse error"); return;
    }

    float       temp  = s_weather_doc["temp"]          | 0.0f;
    const char *cond  = s_weather_doc["condition"]     | "Unknown";
    float       wind  = s_weather_doc["wind_speed"]    | 0.0f;
    const char *wdir  = s_weather_doc["wind_cardinal"] | "--";
    int         hum   = s_weather_doc["humidity"]      | 0;
    float       uv    = s_weather_doc["uv_index"]      | 0.0f;
    const char *sr    = s_weather_doc["sunrise"]       | "--:--";
    const char *ss    = s_weather_doc["sunset"]        | "--:--";

    char wx[256];
    snprintf(wx, sizeof(wx),
        "%s  %.0f\xc2\xb0""C  %s  |  Wind: %.0f km/h %s  |  Humidity: %d%%  |  UV: %.0f  |  Sunrise: %s  |  Sunset: %s",
        LOCATION_NAME, temp, cond, wind, wdir, hum, uv, sr, ss);
    logTs("WX", "ok temp=%.1f cond=%s", temp, cond);
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

/* Scans ac array (capped at MAX_AC_COUNT) and returns the nearest aircraft.
   Writes the distance in km to *out_dist. Returns a null JsonObject if none. */
static JsonObject findNearest(JsonArray ac, float *out_dist) {
    assert(out_dist != NULL);
    *out_dist = 1e9f;
    JsonObject best;
    int count = 0;
    for (JsonObject a : ac) {
        if (++count > MAX_AC_COUNT) break;
        if (a["lat"].isNull() || a["lon"].isNull()) continue;
        const char *fl = a["flight"] | "";
        if (strncmp(fl, "SSM1", 4) == 0 || strncmp(fl, "SSM2", 4) == 0) continue;
        float d = haversine_km(HOME_LAT_DEFAULT, HOME_LON_DEFAULT,
                               a["lat"].as<float>(), a["lon"].as<float>());
        if (d < *out_dist) { *out_dist = d; best = a; }
    }
    return best;
}

void fetchNearest(void) {
    assert(strlen(PROXY_HOST) > 0);
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
    if (code != 200) { logTs("NEAR", "HTTP %d", code); http.end(); return; }

    /* WiFiClientSecure::available() returns 0 between SSL records (4KB boundaries),
       which ArduinoJson misreads as EOF. Read the full body first, then parse. */
    String body = http.getString();
    http.end();
    if (body.isEmpty()) { logTs("NEAR", "empty body"); return; }

    /* Build the field filter once and reuse it on every call */
    static JsonDocument s_filter_doc;
    static bool s_filter_ready = false;
    if (!s_filter_ready) {
        s_filter_doc["ac"][0]["flight"]    = true;
        s_filter_doc["ac"][0]["t"]         = true;
        s_filter_doc["ac"][0]["r"]         = true;
        s_filter_doc["ac"][0]["lat"]       = true;
        s_filter_doc["ac"][0]["lon"]       = true;
        s_filter_doc["ac"][0]["alt_baro"]  = true;
        s_filter_doc["ac"][0]["gs"]        = true;
        s_filter_doc["ac"][0]["baro_rate"] = true;
        s_filter_doc["ac"][0]["dep"]       = true;
        s_filter_doc["ac"][0]["arr"]       = true;
        s_filter_ready = true;
    }
    assert(s_filter_ready);

    DeserializationError near_err = deserializeJson(s_nearest_doc, body,
            DeserializationOption::Filter(s_filter_doc));
    if (near_err != DeserializationError::Ok) {
        logTs("NEAR", "parse error: %s", near_err.c_str()); return;
    }

    JsonArray ac = s_nearest_doc["ac"];
    const char *none[NEAR_ROWS] = { "--", "--", "--", "--", "--", "--" };
    if (ac.isNull() || ac.size() == 0) { lvgl_update_nearest(none); return; }

    float best_dist;
    JsonObject best = findNearest(ac, &best_dist);
    if (best.isNull()) { lvgl_update_nearest(none); return; }

    const char *call = best["flight"]    | "--";
    const char *type = best["t"]         | "--";
    const char *reg  = best["r"]         | "--";
    const char *dep  = best["dep"]       | (const char *)NULL;
    const char *arr  = best["arr"]       | (const char *)NULL;
    int   alt        = best["alt_baro"]  | 0;
    float gs         = best["gs"]        | 0.0f;
    int   baro_rate  = best["baro_rate"] | 0;

    char s_call[12], s_type[8], s_reg[12], s_route[16], s_dist[12], s_phase[14];
    strlcpy(s_call, call, sizeof(s_call));
    for (int i = strlen(s_call) - 1; i >= 0 && s_call[i] == ' '; i--) s_call[i] = '\0';
    strlcpy(s_type, type, sizeof(s_type));
    strlcpy(s_reg,  reg,  sizeof(s_reg));
    if (dep && arr) snprintf(s_route, sizeof(s_route), "%s-%s", dep, arr);
    else            strlcpy(s_route, "--", sizeof(s_route));
    snprintf(s_dist,  sizeof(s_dist),  "%.1f km", best_dist);
    strlcpy(s_phase, flightPhase(alt, gs, baro_rate), sizeof(s_phase));

    logTs("NEAR", "ok call=%s dist=%s phase=%s", s_call, s_dist, s_phase);
    const char *vals[NEAR_ROWS] = { s_call, s_type, s_reg, s_route, s_dist, s_phase };
    lvgl_update_nearest(vals);
}

void fetchServer(void) {
    assert(strlen(PROXY_HOST) > 0);
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    char url[64];
    snprintf(url, sizeof(url), "https://%s/stats", PROXY_HOST);
    if (!http.begin(client, url)) return;
    int code = http.GET();
    if (code != 200) { logTs("SRV", "HTTP %d", code); http.end(); return; }
    String srv_body = http.getString();
    http.end();
    DeserializationError srv_err = deserializeJson(s_server_doc, srv_body);
    if (srv_err != DeserializationError::Ok) {
        logTs("SRV", "parse error: %s", srv_err.c_str()); return;
    }

    char s_up[16], s_reqs[16], s_cache[16], s_routes[16], s_errs[16];
    strlcpy(s_up,      s_server_doc["uptime"]       | "--", sizeof(s_up));
    snprintf(s_reqs,   sizeof(s_reqs),   "%d", (int)(s_server_doc["totalRequests"] | 0));
    strlcpy(s_cache,   s_server_doc["cacheHitRate"] | "--", sizeof(s_cache));
    snprintf(s_routes, sizeof(s_routes), "%d", (int)(s_server_doc["knownRoutes"]   | 0));
    snprintf(s_errs,   sizeof(s_errs),   "%d", (int)(s_server_doc["errors"]        | 0));

    logTs("SRV", "ok up=%s reqs=%s routes=%s", s_up, s_reqs, s_routes);
    const char *vals[SERV_ROWS] = { "OK", s_up, s_reqs, s_cache, s_routes, s_errs };
    lvgl_update_server(vals);
}

void fetchReceiver(void) {
    WiFiClient client;
    HTTPClient http;
    if (!http.begin(client, "http://airplanes.local/tar1090/data/stats.json")) return;
    http.setConnectTimeout(3000);
    int code = http.GET();
    if (code != 200) { logTs("RECV", "HTTP %d", code); http.end(); return; }
    String recv_body = http.getString();
    http.end();
    if (deserializeJson(s_recv_doc, recv_body) != DeserializationError::Ok) {
        logTs("RECV", "parse error"); return;
    }

    JsonObject lm  = s_recv_doc["last1min"];
    JsonObject loc = lm["local"];

    int   msgs   = lm["messages_valid"]  | 0;
    float signal = loc["signal"]         | 0.0f;
    float noise  = loc["noise"]          | 0.0f;
    int   strong = loc["strong_signals"] | 0;
    int   tracks = lm["tracks"]["all"]   | 0;

    char s_msgs[16], s_sig[16], s_noise[16], s_strong[16], s_tracks[16];
    snprintf(s_msgs,   sizeof(s_msgs),   "%d",        msgs);
    snprintf(s_sig,    sizeof(s_sig),    "%.1f dBFS", signal);
    snprintf(s_noise,  sizeof(s_noise),  "%.1f dBFS", noise);
    snprintf(s_strong, sizeof(s_strong), "%d",        strong);
    snprintf(s_tracks, sizeof(s_tracks), "%d",        tracks);

    logTs("RECV", "ok msgs=%d tracks=%d", msgs, tracks);
    const char *vals[RECV_ROWS] = { s_msgs, s_sig, s_noise, s_strong, s_tracks };
    lvgl_update_receiver(vals);
}
