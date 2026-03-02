// ─── Helper functions ──────────────────────────────────
// Pure logic: no hardware deps besides Serial for logging

#include "lookup_tables.h"

bool wifiOk() { return WiFi.status() == WL_CONNECTED; }

void logTs(const char* tag, const char* fmt, ...) {
  char buf[160];
  unsigned long ms = millis();
  int n = snprintf(buf, sizeof(buf), "[%lu.%03lu][%s] ", ms / 1000, ms % 1000, tag);
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf + n, sizeof(buf) - n, fmt, args);
  va_end(args);
  Serial.println(buf);
}

bool alreadyLogged(const char* cs) {
  for (int i = 0; i < loggedCount; i++)
    if (strcmp(loggedCallsigns[i], cs) == 0) return true;
  return false;
}

const Airline* getAirline(const char* cs) {
  for (int i = 0; i < AIRLINE_COUNT; i++)
    if (strncmp(cs, AIRLINES[i].prefix, 3) == 0) return &AIRLINES[i];
  return nullptr;
}

const char* getAircraftTypeName(const char* code) {
  if (!code || !code[0]) return "---";
  for (int i = 0; i < AIRCRAFT_TYPE_COUNT; i++)
    if (strcmp(AIRCRAFT_TYPES[i].code, code) == 0) return AIRCRAFT_TYPES[i].name;
  return code;
}

FlightStatus deriveStatus(int alt, int vs, float dist) {
  if (alt <= 0) return STATUS_UNKNOWN;
  if (dist < 2.0f && alt < 8000) return STATUS_OVERHEAD;
  if (alt < 3000) {
    if (vs < -200) return STATUS_LANDING;
    if (vs >  200) return STATUS_TAKING_OFF;
    if (vs <  -50) return STATUS_APPROACH;
  }
  if (vs < -100) return STATUS_DESCENDING;
  if (vs >  100) return STATUS_CLIMBING;
  return STATUS_CRUISING;
}

const char* statusLabel(FlightStatus s) {
  switch (s) {
    case STATUS_TAKING_OFF:  return "TAKEOFF";
    case STATUS_CLIMBING:    return "CLIMBING";
    case STATUS_CRUISING:    return "CRUISING";
    case STATUS_DESCENDING:  return "DESCEND";
    case STATUS_APPROACH:    return "APPROACH";
    case STATUS_LANDING:     return "LANDING";
    case STATUS_OVERHEAD:    return "OVERHEAD";
    default:                 return "UNKNOWN";
  }
}

uint16_t statusColor(FlightStatus s) {
  switch (s) {
    case STATUS_TAKING_OFF:  return C_GREEN;
    case STATUS_CLIMBING:    return C_CYAN;
    case STATUS_CRUISING:    return C_AMBER;
    case STATUS_DESCENDING:  return C_ORANGE;
    case STATUS_APPROACH:    return C_GOLD;
    case STATUS_LANDING:     return C_RED;
    case STATUS_OVERHEAD:    return C_YELLOW;
    default:                 return C_DIM;
  }
}

float haversineKm(float lat1, float lon1, float lat2, float lon2) {
  const float R = 6371.0f;
  float dLat = (lat2 - lat1) * M_PI / 180.0f;
  float dLon = (lon2 - lon1) * M_PI / 180.0f;
  float a = sinf(dLat/2)*sinf(dLat/2) +
            cosf(lat1*M_PI/180)*cosf(lat2*M_PI/180)*sinf(dLon/2)*sinf(dLon/2);
  return R * 2.0f * atan2f(sqrtf(a), sqrtf(1-a));
}

int apiRadiusNm() {
  return (int)ceilf((GEOFENCE_KM / 1.852f) * 4.0f);
}

void formatAlt(int alt, char* buf, int len) {
  if (alt <= 0)          snprintf(buf, len, "---");
  else if (alt >= 10000) snprintf(buf, len, "FL%03d", alt / 100);
  else                   snprintf(buf, len, "%d FT", alt);
}

// ─── Diagnostics: single-line JSON status ─────────────
void diagReport() {
  const char* src = dataSource == 2 ? "cache" : dataSource == 1 ? "direct" : "proxy";
  Serial.printf("{\"heap\":%d,\"maxblk\":%d,\"wifi\":%d,\"rssi\":%d,\"src\":\"%s\","
                "\"flights\":%d,\"uptime\":%lu,\"frag\":%d}\n",
    ESP.getFreeHeap(),
    ESP.getMaxAllocHeap(),
    wifiOk() ? 1 : 0,
    wifiOk() ? WiFi.RSSI() : 0,
    src,
    flightCount,
    millis() / 1000,
    (int)(100 - (ESP.getMaxAllocHeap() * 100 / max((uint32_t)1, (uint32_t)ESP.getFreeHeap())))
  );
}
