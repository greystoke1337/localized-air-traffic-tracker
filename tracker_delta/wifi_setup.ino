// ─── WiFi config, captive portal ─────────────────────────────────────────────
#include <assert.h>

bool loadWiFiConfig() {
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, true)) return false;
    if (!p.isKey("wifi_ssid")) { p.end(); return false; }
    strlcpy(WIFI_SSID, p.getString("wifi_ssid", "").c_str(), sizeof(WIFI_SSID));
    strlcpy(WIFI_PASS, p.getString("wifi_pass", "").c_str(), sizeof(WIFI_PASS));
    p.end();
    return strlen(WIFI_SSID) > 0;
}

void saveWiFiConfig(const char *ssid, const char *pass) {
    assert(ssid != NULL && strlen(ssid) > 0);
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, false)) return;
    p.putString("wifi_ssid", ssid);
    p.putString("wifi_pass", pass);
    p.end();
}

bool connectWiFi() {
    assert(strlen(WIFI_SSID) > 0);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int attempts = WIFI_CONNECT_TIMEOUT_S * 2;
    for (int i = 0; i < attempts; i++) {
        if (WiFi.status() == WL_CONNECTED) return true;
        snprintf(boot_status, sizeof(boot_status), "CONNECTING TO WIFI... %ds", i / 2);
        delay(500);
    }
    return WiFi.status() == WL_CONNECTED;
}

void startCaptivePortal() {
    /* Static so constructors run once; lambdas below access these as
       static-storage variables without needing an explicit capture. */
    static WebServer setupServer(80);
    static DNSServer dnsServer;

    bool ap_ok = WiFi.softAP(WIFI_AP_NAME);
    assert(ap_ok);
    dnsServer.start(53, "*", WiFi.softAPIP());

    setupServer.on("/", HTTP_GET, []() {
        setupServer.send(200, "text/html",
            "<!DOCTYPE html><html><head>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<style>"
            "body{background:#08080f;color:#c8dcff;font-family:monospace;padding:24px;max-width:420px;margin:auto}"
            "h2{color:#00c8ff;letter-spacing:2px;margin-bottom:20px}label{display:block;margin-top:12px;color:#00d250}"
            "input{width:100%;box-sizing:border-box;background:#0e1223;color:#fff;border:1px solid #1e50a0;"
            "padding:8px;font-family:monospace;font-size:15px;margin-top:4px}"
            "button{margin-top:20px;width:100%;background:#00c8ff;color:#000;border:none;"
            "padding:10px;font-family:monospace;font-size:15px;font-weight:bold;cursor:pointer}"
            "</style></head><body>"
            "<h2>DELTA SETUP</h2>"
            "<form method='POST' action='/save'>"
            "<label>WIFI SSID<input name='ssid' autocomplete='off'></label>"
            "<label>PASSWORD<input name='pass' type='password'></label>"
            "<button type='submit'>SAVE &amp; CONNECT</button>"
            "</form></body></html>"
        );
    });

    setupServer.on("/save", HTTP_POST, []() {
        String ssid = setupServer.arg("ssid");
        String pass = setupServer.arg("pass");
        if (ssid.length() > 0) {
            saveWiFiConfig(ssid.c_str(), pass.c_str());
            setupServer.send(200, "text/html",
                "<!DOCTYPE html><html><head>"
                "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<style>body{background:#08080f;color:#00d250;font-family:monospace;"
                "padding:24px;text-align:center}</style></head><body>"
                "<h2>SAVED</h2><p>DELTA is restarting...</p></body></html>"
            );
            delay(1500);
            ESP.restart();
        } else {
            setupServer.sendHeader("Location", "/");
            setupServer.send(302, "text/plain", "");
        }
    });

    setupServer.onNotFound([]() {
        setupServer.sendHeader("Location", "http://192.168.4.1/");
        setupServer.send(302, "text/plain", "");
    });

    setupServer.begin();

    /* Bounded portal loop: restart after PORTAL_TIMEOUT_MS if no config saved */
    unsigned long start = millis();
    for (unsigned long elapsed = 0; elapsed < PORTAL_TIMEOUT_MS; elapsed = millis() - start) {
        dnsServer.processNextRequest();
        setupServer.handleClient();
        delay(5);
    }
    ESP.restart();
}
