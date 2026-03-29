// ─── Helper functions ─────────────────────────────

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
  [STATUS_CLIMBING]   = { "CLIMBING", C_GREEN  },
  [STATUS_CRUISING]   = { "CRUISING", C_AMBER  },
  [STATUS_DESCENDING] = { "DESCEND",  C_ORANGE },
  [STATUS_APPROACH]   = { "APPROACH", C_GOLD   },
  [STATUS_LANDING]    = { "LANDING",  C_RED    },
  [STATUS_OVERHEAD]   = { "OVERHEAD", C_ACCENT },
};

const char* statusLabel(FlightStatus s) { return STATUS_TABLE[s].label; }
uint16_t    statusColor(FlightStatus s) { return STATUS_TABLE[s].color; }

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
  else                   snprintf(buf, len, "%dft", alt);
}

void toUpperStr(char* s) {
  for (int i = 0; s[i]; i++) s[i] = toupper((unsigned char)s[i]);
}

void sortFlightsByDist(Flight* f, int count) {
  for (int i = 0; i < count-1; i++)
    for (int j = 0; j < count-i-1; j++)
      if (f[j].dist > f[j+1].dist)
        { Flight tmp = f[j]; f[j] = f[j+1]; f[j+1] = tmp; }
}

bool alreadyLogged(const char* cs) {
  if (!cs || !cs[0]) return true;
  for (int i = 0; i < loggedCount; i++)
    if (strcmp(loggedCallsigns[i], cs) == 0) return true;
  return false;
}
