/*
  Desktop test for JSON parsing logic.
  Compile: g++ -std=c++17 -Wall -Wextra -Itests/../tracker_live_fnk0103s/libraries/ArduinoJson/src -o test_parsing test_parsing.cpp
  Run:     ./test_parsing
  Or:      ./build.sh test

  Uses the vendored ArduinoJson (header-only) to test extractFlights logic
  against saved JSON fixtures — the same code path the firmware uses for
  proxy/cache responses.
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cassert>
#include <string>
#include <fstream>
#include <sstream>

#include <ArduinoJson.h>

// ─── Types from firmware ──────────────────────────────
enum FlightStatus {
  STATUS_UNKNOWN, STATUS_TAKING_OFF, STATUS_CLIMBING, STATUS_CRUISING,
  STATUS_DESCENDING, STATUS_APPROACH, STATUS_LANDING, STATUS_OVERHEAD,
};

struct Flight {
  char callsign[12];
  char reg[12];
  char type[8];
  char route[40];
  float lat, lon;
  int alt, speed, vs, track;
  float dist;
  char squawk[6];
  FlightStatus status;
};

// ─── Re-implement firmware logic for desktop ──────────
float haversineKm(float lat1, float lon1, float lat2, float lon2) {
  const float R = 6371.0f;
  float dLat = (lat2 - lat1) * (float)M_PI / 180.0f;
  float dLon = (lon2 - lon1) * (float)M_PI / 180.0f;
  float a = sinf(dLat/2)*sinf(dLat/2) +
            cosf(lat1*(float)M_PI/180)*cosf(lat2*(float)M_PI/180)*sinf(dLon/2)*sinf(dLon/2);
  return R * 2.0f * atan2f(sqrtf(a), sqrtf(1-a));
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

// Test geofence
static float HOME_LAT = -33.8688f;
static float HOME_LON = 151.2093f;
static float GEOFENCE_KM = 20.0f;
static int   ALT_FLOOR_FT = 500;

// ─── extractFlights (mirror of firmware) ──────────────
static Flight newFlights[20];

int extractFlights(DynamicJsonDocument& doc) {
  JsonArray ac = doc["ac"].as<JsonArray>();
  if (ac.isNull()) return 0;
  int count = 0;
  for (JsonObject a : ac) {
    if (count >= 20) break;
    float lat = a["lat"] | 0.0f;
    float lon = a["lon"] | 0.0f;
    int alt = a["alt_baro"] | 0;
    if (alt < ALT_FLOOR_FT || lat == 0.0f) continue;
    float dist = haversineKm(HOME_LAT, HOME_LON, lat, lon);
    if (dist > GEOFENCE_KM) continue;

    Flight& f = newFlights[count];
    const char* cs = a["flight"] | "";
    strlcpy(f.callsign, cs, sizeof(f.callsign));
    for (int i = (int)strlen(f.callsign)-1; i >= 0 && f.callsign[i] == ' '; i--) f.callsign[i] = 0;
    strlcpy(f.reg,    a["r"]      | "",     sizeof(f.reg));
    strlcpy(f.type,   a["t"]      | "",     sizeof(f.type));
    strlcpy(f.squawk, a["squawk"] | "----", sizeof(f.squawk));
    strlcpy(f.route,  a["route"]  | "",     sizeof(f.route));
    f.lat = lat; f.lon = lon; f.alt = alt;
    f.speed = (int)(a["gs"] | 0.0f);
    f.vs = a["baro_rate"] | 0;
    f.track = (int)(a["track"] | -1.0f);
    f.dist = dist;
    f.status = deriveStatus(alt, f.vs, dist);

    for (int i = 0; f.callsign[i]; i++) f.callsign[i] = toupper(f.callsign[i]);
    for (int i = 0; f.reg[i]; i++) f.reg[i] = toupper(f.reg[i]);
    for (int i = 0; f.type[i]; i++) f.type[i] = toupper(f.type[i]);
    count++;
  }

  // Sort by distance
  for (int i = 0; i < count-1; i++)
    for (int j = 0; j < count-i-1; j++)
      if (newFlights[j].dist > newFlights[j+1].dist) {
        Flight tmp = newFlights[j]; newFlights[j] = newFlights[j+1]; newFlights[j+1] = tmp;
      }
  return count;
}

// ─── strlcpy for non-BSD systems ──────────────────────
#ifndef __APPLE__
#ifndef strlcpy
size_t strlcpy(char* dst, const char* src, size_t size) {
  size_t len = strlen(src);
  if (size > 0) {
    size_t n = (len >= size) ? size - 1 : len;
    memcpy(dst, src, n);
    dst[n] = '\0';
  }
  return len;
}
#endif
#endif

// ─── Test helpers ─────────────────────────────────────
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  %-50s ", name);
#define EXPECT(cond) if (cond) { tests_passed++; printf("[OK]\n"); } else { printf("[FAIL] line %d\n", __LINE__); } } while(0)

std::string readFixture(const char* filename) {
  std::string path = std::string("tests/fixtures/") + filename;
  std::ifstream f(path);
  if (!f.is_open()) {
    // Try from parent directory (if running from tests/)
    path = std::string("fixtures/") + filename;
    f.open(path);
  }
  if (!f.is_open()) {
    fprintf(stderr, "Cannot open fixture: %s\n", filename);
    return "";
  }
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

int parseFixture(const char* filename) {
  std::string json = readFixture(filename);
  if (json.empty()) return -1;

  DynamicJsonDocument doc(16384);
  StaticJsonDocument<512> filter;
  JsonObject af = filter["ac"].createNestedObject();
  af["flight"] = af["r"] = af["t"] = af["lat"] = af["lon"] =
  af["alt_baro"] = af["gs"] = af["baro_rate"] = af["track"] =
  af["squawk"] = af["route"] = true;

  DeserializationError err = deserializeJson(doc, json.c_str(), json.length(),
    DeserializationOption::Filter(filter));
  if (err) {
    printf("JSON error: %s  ", err.c_str());
    return -1;
  }
  return extractFlights(doc);
}

// ─── Tests ────────────────────────────────────────────

void test_normal_response() {
  printf("\n── normal_response.json ──\n");
  int n = parseFixture("normal_response.json");

  TEST("parses successfully");
  EXPECT(n >= 0);

  TEST("finds expected number of flights");
  EXPECT(n == 4);

  TEST("first flight (closest) has valid callsign");
  EXPECT(n > 0 && newFlights[0].callsign[0] != '\0');

  TEST("flights sorted by distance (ascending)");
  bool sorted = true;
  for (int i = 0; i < n-1; i++)
    if (newFlights[i].dist > newFlights[i+1].dist) sorted = false;
  EXPECT(sorted);

  TEST("all flights within geofence");
  bool inFence = true;
  for (int i = 0; i < n; i++)
    if (newFlights[i].dist > GEOFENCE_KM) inFence = false;
  EXPECT(inFence);
}

void test_empty_response() {
  printf("\n── empty_response.json ──\n");
  int n = parseFixture("empty_response.json");

  TEST("parses successfully");
  EXPECT(n >= 0);

  TEST("returns 0 flights");
  EXPECT(n == 0);
}

void test_emergency_squawk() {
  printf("\n── emergency_squawk.json ──\n");
  int n = parseFixture("emergency_squawk.json");

  TEST("parses successfully");
  EXPECT(n >= 0);

  TEST("finds emergency flight");
  bool found7700 = false;
  for (int i = 0; i < n; i++)
    if (strcmp(newFlights[i].squawk, "7700") == 0) found7700 = true;
  EXPECT(found7700);
}

void test_malformed_json() {
  printf("\n── malformed.json ──\n");
  int n = parseFixture("malformed.json");

  TEST("returns -1 for corrupt JSON");
  EXPECT(n == -1);
}

void test_no_altitude() {
  printf("\n── no_altitude.json ──\n");
  int n = parseFixture("no_altitude.json");

  TEST("parses successfully");
  EXPECT(n >= 0);

  TEST("filters out aircraft with 0 altitude");
  EXPECT(n == 0);
}

void test_boundary_geofence() {
  printf("\n── boundary_geofence.json ──\n");
  int n = parseFixture("boundary_geofence.json");

  TEST("parses successfully");
  EXPECT(n >= 0);

  TEST("includes flight inside fence, excludes outside");
  EXPECT(n == 1);
}

int main() {
  printf("═══════════════════════════════════════\n");
  printf("  Overhead Tracker — JSON Parsing Tests\n");
  printf("═══════════════════════════════════════\n");

  test_normal_response();
  test_empty_response();
  test_emergency_squawk();
  test_malformed_json();
  test_no_altitude();
  test_boundary_geofence();

  printf("\n═══════════════════════════════════════\n");
  printf("  Results: %d / %d passed\n", tests_passed, tests_run);
  printf("═══════════════════════════════════════\n");

  return (tests_passed == tests_run) ? 0 : 1;
}
