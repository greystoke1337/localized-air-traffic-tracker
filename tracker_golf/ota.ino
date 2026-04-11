// ota.ino — WiFi OTA firmware update (HTTPS download + NVMCTRL flash write from SRAM)
// Local OTA: set OTA_LOCAL_HOST to your machine's LAN IP in secrets.h (HTTP, no SSL).
//            Falls back to remote if host is empty or unreachable.
// Remote OTA: api.overheadtracker.com (HTTPS). Checked every OTA_CHECK_INTERVAL_MS.

// Download buffer (BSS — zero-initialised, no flash cost). 110 KB covers any foreseeable build.
static uint8_t otaBuf[110000];

// Forward declaration (defined below checkOTA so it can be placed in .data section)
static void __attribute__((section(".data"), noinline, used))
applyOTA(const uint8_t *buf, uint32_t len);

// ── Shared helpers ────────────────────────────────────────────────────────────

// Skip HTTP response headers until a blank line or deadline expires.
static bool skipOtaHeaders(WiFiClient &client, unsigned long deadline) {
  while (client.connected() && millis() < deadline) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line == "") return true;
  }
  return false;
}

// Read HTTP body into otaBuf. Returns true if at least 4 KB was received.
static bool readOtaBinary(WiFiClient &client, uint32_t &received) {
  received = 0;
  unsigned long deadline = millis() + 30000UL;
  while (client.connected() && received < sizeof(otaBuf) && millis() < deadline) {
    int n = client.read(otaBuf + received, sizeof(otaBuf) - received);
    if (n > 0) { received += n; deadline = millis() + 5000UL; }
  }
  return received >= 4096;
}

// ── Local OTA (HTTP) ──────────────────────────────────────────────────────────
// Tries OTA_LOCAL_HOST:OTA_LOCAL_PORT over plain HTTP.
// Returns true if handled (up-to-date or flashed), false if host is empty or unreachable.
static bool checkLocalOTA() {
  if (!OTA_LOCAL_HOST[0]) return false;

  WiFiClient client;
  client.setTimeout(5000);
  if (!client.connect(OTA_LOCAL_HOST, OTA_LOCAL_PORT)) {
    Serial.println("[OTA] Local server unreachable — trying remote");
    return false;
  }

  if (!client.print(
    "GET /firmware/golf/version HTTP/1.1\r\n"
    "Host: " OTA_LOCAL_HOST "\r\n"
    "Connection: close\r\n"
    "\r\n")) {
    client.stop(); return false;
  }

  if (!skipOtaHeaders(client, millis() + 5000UL)) { client.stop(); return false; }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, client);
  client.stop();
  if (err) return false;

  int remoteVer = doc["version"] | 0;
  Serial.printf("[OTA] Local version %d, device %d\n", remoteVer, FIRMWARE_VERSION);
  if (remoteVer <= FIRMWARE_VERSION) return true;  // up-to-date; skip remote check

  Serial.println("[OTA] Downloading from local server");
  drawOTAStatus("OTA...");

  if (!client.connect(OTA_LOCAL_HOST, OTA_LOCAL_PORT)) return false;
  if (!client.print(
    "GET /firmware/golf/binary HTTP/1.1\r\n"
    "Host: " OTA_LOCAL_HOST "\r\n"
    "Connection: close\r\n"
    "\r\n")) {
    client.stop(); return false;
  }

  if (!skipOtaHeaders(client, millis() + 15000UL)) { client.stop(); return false; }

  uint32_t received = 0;
  if (!readOtaBinary(client, received)) {
    client.stop();
    Serial.printf("[OTA] Local download too small (%u bytes) — trying remote\n", received);
    return false;
  }
  client.stop();

  Serial.printf("[OTA] Local: downloaded %u bytes — flashing\n", received);
  drawOTAStatus("FLASH");
  delay(400);

  applyOTA(otaBuf, received);  // never returns
  return true;
}

// ── Remote OTA (HTTPS) ────────────────────────────────────────────────────────
// Check proxy for a newer firmware version; download and self-flash if one is found.
// Safe to call from setup() and periodically from loop(). Returns immediately if up-to-date.
void checkOTA() {
  if (OTA_LOCAL_HOST[0] && checkLocalOTA()) return;

  WiFiSSLClient client;
  client.setTimeout(10000);

  // ── Step 1: fetch version number ──────────────────────────────────────────
  if (!client.connect("api.overheadtracker.com", 443)) {
    Serial.println("[OTA] Connect failed (version check)");
    return;
  }

  if (!client.print(
    "GET /firmware/golf/version HTTP/1.1\r\n"
    "Host: api.overheadtracker.com\r\n"
    "Connection: close\r\n"
    "\r\n")) {
    client.stop();
    Serial.println("[OTA] Write failed (version check)");
    return;
  }

  if (!skipOtaHeaders(client, millis() + 10000UL)) {
    client.stop();
    Serial.println("[OTA] Timeout reading version headers");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, client);
  client.stop();

  if (err) {
    Serial.print("[OTA] JSON parse error: ");
    Serial.println(err.c_str());
    return;
  }

  int remoteVer = doc["version"] | 0;
  Serial.printf("[OTA] Remote version %d, local %d\n", remoteVer, FIRMWARE_VERSION);
  if (remoteVer <= FIRMWARE_VERSION) return;

  // ── Step 2: download binary ────────────────────────────────────────────────
  Serial.println("[OTA] Update available — downloading");
  drawOTAStatus("OTA...");

  if (!client.connect("api.overheadtracker.com", 443)) {
    Serial.println("[OTA] Connect failed (binary download)");
    return;
  }

  if (!client.print(
    "GET /firmware/golf/binary HTTP/1.1\r\n"
    "Host: api.overheadtracker.com\r\n"
    "Connection: close\r\n"
    "\r\n")) {
    client.stop();
    Serial.println("[OTA] Write failed (binary download)");
    return;
  }

  if (!skipOtaHeaders(client, millis() + 15000UL)) {
    client.stop();
    Serial.println("[OTA] Timeout reading binary headers");
    return;
  }

  uint32_t received = 0;
  if (!readOtaBinary(client, received)) {
    client.stop();
    Serial.printf("[OTA] Download too small (%u bytes) — aborting\n", received);
    return;
  }
  client.stop();

  Serial.printf("[OTA] Downloaded %u bytes — flashing\n", received);
  drawOTAStatus("FLASH");
  delay(400);

  applyOTA(otaBuf, received);
  // applyOTA() never returns — device resets into new firmware
}

// Erases + rewrites the sketch area from SRAM, then resets.
// Must run from SRAM (.data) — NVMCTRL cannot read flash while writing it.
// SAMD51 NVMCTRL: block erase = 8 KB, quad-word write = 16 bytes.
static void __attribute__((section(".data"), noinline, used))
applyOTA(const uint8_t *buf, uint32_t len) {
  if (!buf || len < 4096) { NVIC_SystemReset(); return; }

  const uint32_t SKETCH_START = 0x4000UL;
  const uint32_t BLOCK_SIZE   = 8192UL;
  const uint32_t SPIN_MAX     = 100000UL;

  __disable_irq();

  uint32_t spin = 0;
  while (!NVMCTRL->INTFLAG.bit.DONE && ++spin < SPIN_MAX) {}
  NVMCTRL->INTFLAG.bit.DONE = 1;

  // Erase all 8 KB blocks covering the new binary
  uint32_t eraseLen = (len + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1);
  for (uint32_t off = 0; off < eraseLen; off += BLOCK_SIZE) {
    NVMCTRL->ADDR.reg  = SKETCH_START + off;
    NVMCTRL->CTRLA.reg = NVMCTRL_CTRLB_CMD_EB | NVMCTRL_CTRLB_CMDEX_KEY;
    spin = 0;
    while (!NVMCTRL->INTFLAG.bit.DONE && ++spin < SPIN_MAX) {}
    NVMCTRL->INTFLAG.bit.DONE = 1;
  }

  // Write quad-words (16 bytes, 4 × uint32_t each)
  uint32_t writeLen = (len + 15) & ~15UL;
  for (uint32_t off = 0; off < writeLen; off += 16) {
    volatile uint32_t *dst = (volatile uint32_t *)(SKETCH_START + off);
    const uint32_t    *src = (const uint32_t *)(buf + off);
    dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3];
    NVMCTRL->CTRLA.reg = NVMCTRL_CTRLB_CMD_WQW | NVMCTRL_CTRLB_CMDEX_KEY;
    spin = 0;
    while (!NVMCTRL->INTFLAG.bit.DONE && ++spin < SPIN_MAX) {}
    NVMCTRL->INTFLAG.bit.DONE = 1;
  }

  NVIC_SystemReset();
}
