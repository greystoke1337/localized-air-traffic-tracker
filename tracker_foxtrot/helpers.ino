// ─── Helper functions ──────────────────────────────────
// Pure logic: no hardware deps besides Serial for logging

#include "lookup_tables.h"

bool wifiOk() { return WiFi.status() == WL_CONNECTED; }

void wlog(const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print(buf);
  if (logClient && logClient.connected()) logClient.print(buf);
}

void logTs(const char* tag, const char* fmt, ...) {
  char buf[160];
  unsigned long ms = millis();
  int n = snprintf(buf, sizeof(buf), "[%lu.%03lu][%s] ", ms / 1000, ms % 1000, tag);
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf + n, sizeof(buf) - n, fmt, args);
  va_end(args);
  wlog("%s\n", buf);
}

bool alreadyLogged(const char* cs) {
  if (!cs || !cs[0]) return true;
  for (int i = 0; i < loggedCount; i++)
    if (strcmp(loggedCallsigns[i], cs) == 0) return true;
  return false;
}

const Airline* getAirline(const char* cs) {
  if (!cs || !cs[0]) return nullptr;
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

const char* getAircraftCategory(const char* code) {
  if (!code || !code[0]) return nullptr;
  for (int i = 0; i < AIRCRAFT_TYPE_COUNT; i++)
    if (strcmp(AIRCRAFT_TYPES[i].code, code) == 0) return AIRCRAFT_TYPES[i].cat;
  return nullptr;
}

FlightStatus deriveStatus(int alt, int vs, float dist) {
  if (alt <= 0) return STATUS_UNKNOWN;
  if (dist < 1.2f && alt < 8000) return STATUS_OVERHEAD;
  if (alt < 3000) {
    if (vs < -200) return STATUS_LANDING;
    if (vs >  200) return STATUS_TAKING_OFF;
    if (vs <  -50) return STATUS_APPROACH;
  }
  if (vs < -100) return STATUS_DESCENDING;
  if (vs >  100) return STATUS_CLIMBING;
  return STATUS_CRUISING;
}

struct StatusInfo { const char* label; uint16_t color; };
static const StatusInfo STATUS_TABLE[] = {
  [STATUS_UNKNOWN]    = { "UNKNOWN",  C_DIM    },
  [STATUS_TAKING_OFF] = { "TAKEOFF",  C_GREEN  },
  [STATUS_CLIMBING]   = { "CLIMBING", C_CYAN   },
  [STATUS_CRUISING]   = { "CRUISING", C_AMBER  },
  [STATUS_DESCENDING] = { "DESCEND",  C_ORANGE },
  [STATUS_APPROACH]   = { "APPROACH", C_GOLD   },
  [STATUS_LANDING]    = { "LANDING",  C_RED    },
  [STATUS_OVERHEAD]   = { "OVERHEAD", C_YELLOW },
};

const char* statusLabel(FlightStatus s) { return STATUS_TABLE[s].label; }
uint16_t    statusColor(FlightStatus s) { return STATUS_TABLE[s].color; }

uint16_t distanceColor(float dist_mi, float max_mi) {
  float t = dist_mi / max_mi;
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  uint8_t r = (uint8_t)(t * 100.0f);
  uint8_t g = (uint8_t)(255.0f - t * 55.0f);
  uint8_t b = (uint8_t)(t * 255.0f);
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

float haversineMi(float lat1, float lon1, float lat2, float lon2) {
  const float R = 3958.8f;
  float dLat = (lat2 - lat1) * M_PI / 180.0f;
  float dLon = (lon2 - lon1) * M_PI / 180.0f;
  float a = sinf(dLat/2)*sinf(dLat/2) +
            cosf(lat1*M_PI/180)*cosf(lat2*M_PI/180)*sinf(dLon/2)*sinf(dLon/2);
  return R * 2.0f * atan2f(sqrtf(a), sqrtf(1-a));
}

int apiRadiusNm() {
  return (int)ceilf((GEOFENCE_MI / 1.15078f) * 4.0f);
}

void formatAlt(int alt, char* buf, int len) {
  if (alt <= 0)          snprintf(buf, len, "---");
  else if (alt >= 10000) snprintf(buf, len, "FL%03d", alt / 100);
  else                   snprintf(buf, len, "%d FT", alt);
}

void toUpperStr(char* s) {
  for (int i = 0; s[i]; i++) s[i] = toupper(s[i]);
}

const char* dataSourceLabel() {
  return dataSource == 2 ? "cache" : dataSource == 1 ? "direct" : "proxy";
}

void sortFlightsByDist(Flight* f, int count) {
  for (int i = 0; i < count-1; i++)
    for (int j = 0; j < count-i-1; j++)
      if (f[j].dist > f[j+1].dist)
        { Flight tmp = f[j]; f[j] = f[j+1]; f[j+1] = tmp; }
}

// ─── Diagnostics: single-line JSON status ─────────────
void diagReport() {
  const char* src = dataSourceLabel();
  wlog("{\"heap\":%d,\"maxblk\":%d,\"wifi\":%d,\"rssi\":%d,\"src\":\"%s\","
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
