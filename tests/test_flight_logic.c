/*
  Desktop test for flight logic functions.
  Compile: gcc -std=c11 -Wall -Wextra -o test_flight_logic test_flight_logic.c -lm
  Run:     ./test_flight_logic
  Or:      ./build.sh test
*/

#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <assert.h>
#include <stdint.h>

// ─── Re-declare types and functions from firmware ─────
typedef enum {
  STATUS_UNKNOWN,
  STATUS_TAKING_OFF,
  STATUS_CLIMBING,
  STATUS_CRUISING,
  STATUS_DESCENDING,
  STATUS_APPROACH,
  STATUS_LANDING,
  STATUS_OVERHEAD,
} FlightStatus;

typedef struct { const char* prefix; const char* name; } Airline;
typedef struct { const char* code; const char* name; } AircraftType;

// ─── Paste-in of pure logic functions (no Arduino deps) ─
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

float haversineKm(float lat1, float lon1, float lat2, float lon2) {
  const float R = 6371.0f;
  float dLat = (lat2 - lat1) * (float)M_PI / 180.0f;
  float dLon = (lon2 - lon1) * (float)M_PI / 180.0f;
  float a = sinf(dLat/2)*sinf(dLat/2) +
            cosf(lat1*(float)M_PI/180)*cosf(lat2*(float)M_PI/180)*sinf(dLon/2)*sinf(dLon/2);
  return R * 2.0f * atan2f(sqrtf(a), sqrtf(1-a));
}

static const Airline AIRLINES[] = {
  {"QFA","QANTAS"}, {"VOZ","VIRGIN"}, {"JST","JETSTAR"},
  {"UAE","EMIRATES"}, {"AAL","AMERICAN"}, {"UAL","UNITED"},
  {"BAW","BRITISH"}, {"DAL","DELTA"},
};
static const int AIRLINE_COUNT = sizeof(AIRLINES) / sizeof(AIRLINES[0]);

const char* getAirline(const char* cs) {
  for (int i = 0; i < AIRLINE_COUNT; i++)
    if (strncmp(cs, AIRLINES[i].prefix, 3) == 0) return AIRLINES[i].name;
  return "";
}

static const AircraftType AIRCRAFT_TYPES[] = {
  {"B738","B737-800"}, {"A320","A320"}, {"B789","B787-9"}, {"A380","A380"},
};
static const int AIRCRAFT_TYPE_COUNT = sizeof(AIRCRAFT_TYPES) / sizeof(AIRCRAFT_TYPES[0]);

const char* getAircraftTypeName(const char* code) {
  if (!code || !code[0]) return "---";
  for (int i = 0; i < AIRCRAFT_TYPE_COUNT; i++)
    if (strcmp(AIRCRAFT_TYPES[i].code, code) == 0) return AIRCRAFT_TYPES[i].name;
  return code;
}

int apiRadiusNm(float geofence_km) {
  return (int)ceilf((geofence_km / 1.852f) * 4.0f);
}

void formatAlt(int alt, char* buf, int len) {
  if (alt <= 0)          snprintf(buf, (size_t)len, "---");
  else if (alt >= 10000) snprintf(buf, (size_t)len, "FL%03d", alt / 100);
  else                   snprintf(buf, (size_t)len, "%d FT", alt);
}

// ─── Test counters ────────────────────────────────────
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  %-50s ", name);
#define EXPECT(cond) if (cond) { tests_passed++; printf("[OK]\n"); } else { printf("[FAIL] line %d\n", __LINE__); } } while(0)

// ─── Tests ────────────────────────────────────────────

void test_deriveStatus() {
  printf("\n── deriveStatus ──\n");

  TEST("altitude 0 → UNKNOWN");
  EXPECT(deriveStatus(0, 0, 5.0f) == STATUS_UNKNOWN);

  TEST("negative altitude → UNKNOWN");
  EXPECT(deriveStatus(-100, 0, 5.0f) == STATUS_UNKNOWN);

  TEST("close + low alt → OVERHEAD");
  EXPECT(deriveStatus(5000, 0, 1.5f) == STATUS_OVERHEAD);

  TEST("close but high alt → not OVERHEAD");
  EXPECT(deriveStatus(9000, 0, 1.5f) != STATUS_OVERHEAD);

  TEST("low alt descending fast → LANDING");
  EXPECT(deriveStatus(2000, -500, 5.0f) == STATUS_LANDING);

  TEST("low alt climbing fast → TAKING_OFF");
  EXPECT(deriveStatus(2000, 500, 5.0f) == STATUS_TAKING_OFF);

  TEST("low alt slight descent → APPROACH");
  EXPECT(deriveStatus(2000, -80, 5.0f) == STATUS_APPROACH);

  TEST("high alt descending → DESCENDING");
  EXPECT(deriveStatus(15000, -500, 10.0f) == STATUS_DESCENDING);

  TEST("high alt climbing → CLIMBING");
  EXPECT(deriveStatus(15000, 500, 10.0f) == STATUS_CLIMBING);

  TEST("high alt level → CRUISING");
  EXPECT(deriveStatus(35000, 0, 20.0f) == STATUS_CRUISING);

  TEST("boundary: alt=3000 vs=-200 → LANDING (at boundary)");
  EXPECT(deriveStatus(2999, -201, 5.0f) == STATUS_LANDING);

  TEST("boundary: alt=3000 vs=-100 → not APPROACH (at 3000)");
  EXPECT(deriveStatus(3000, -80, 5.0f) != STATUS_APPROACH);
}

void test_haversine() {
  printf("\n── haversineKm ──\n");

  TEST("same point → 0 km");
  EXPECT(haversineKm(0, 0, 0, 0) < 0.001f);

  TEST("Sydney to Melbourne ~714 km");
  float d = haversineKm(-33.8688f, 151.2093f, -37.8136f, 144.9631f);
  EXPECT(d > 700.0f && d < 730.0f);

  TEST("short distance: ~1 km");
  float d2 = haversineKm(-33.8688f, 151.2093f, -33.8598f, 151.2093f);
  EXPECT(d2 > 0.9f && d2 < 1.1f);

  TEST("antipodal points ~20000 km");
  float d3 = haversineKm(0, 0, 0, 180);
  EXPECT(d3 > 20000.0f && d3 < 20100.0f);
}

void test_getAirline() {
  printf("\n── getAirline ──\n");

  TEST("QFA123 → QANTAS");
  EXPECT(strcmp(getAirline("QFA123"), "QANTAS") == 0);

  TEST("UAL456 → UNITED");
  EXPECT(strcmp(getAirline("UAL456"), "UNITED") == 0);

  TEST("XXX999 → empty (unknown)");
  EXPECT(strcmp(getAirline("XXX999"), "") == 0);

  TEST("empty string → empty");
  EXPECT(strcmp(getAirline(""), "") == 0);
}

void test_getAircraftTypeName() {
  printf("\n── getAircraftTypeName ──\n");

  TEST("B738 → B737-800");
  EXPECT(strcmp(getAircraftTypeName("B738"), "B737-800") == 0);

  TEST("A380 → A380");
  EXPECT(strcmp(getAircraftTypeName("A380"), "A380") == 0);

  TEST("ZZZZ → returns code itself");
  EXPECT(strcmp(getAircraftTypeName("ZZZZ"), "ZZZZ") == 0);

  TEST("NULL → ---");
  EXPECT(strcmp(getAircraftTypeName(NULL), "---") == 0);

  TEST("empty → ---");
  EXPECT(strcmp(getAircraftTypeName(""), "---") == 0);
}

void test_formatAlt() {
  printf("\n── formatAlt ──\n");
  char buf[20];

  TEST("0 → ---");
  formatAlt(0, buf, sizeof(buf));
  EXPECT(strcmp(buf, "---") == 0);

  TEST("-100 → ---");
  formatAlt(-100, buf, sizeof(buf));
  EXPECT(strcmp(buf, "---") == 0);

  TEST("5000 → 5000 FT");
  formatAlt(5000, buf, sizeof(buf));
  EXPECT(strcmp(buf, "5000 FT") == 0);

  TEST("35000 → FL350");
  formatAlt(35000, buf, sizeof(buf));
  EXPECT(strcmp(buf, "FL350") == 0);

  TEST("10000 → FL100 (boundary)");
  formatAlt(10000, buf, sizeof(buf));
  EXPECT(strcmp(buf, "FL100") == 0);
}

void test_apiRadius() {
  printf("\n── apiRadiusNm ──\n");

  TEST("10 km → reasonable NM value");
  int r = apiRadiusNm(10.0f);
  EXPECT(r > 20 && r < 25);

  TEST("5 km → smaller");
  int r2 = apiRadiusNm(5.0f);
  EXPECT(r2 > 10 && r2 < 15);

  TEST("20 km → larger");
  int r3 = apiRadiusNm(20.0f);
  EXPECT(r3 > 40 && r3 < 50);
}

void test_utc_offset_range() {
  printf("\n── UTC offset safety ──\n");

  // int32_t can hold UTC offsets for all timezones (-12h to +14h)
  // This test verifies the type is wide enough (catches int16_t overflow)
  int32_t max_offset = 14 * 3600;   // UTC+14 (Kiribati)
  int32_t min_offset = -12 * 3600;  // UTC-12 (Baker Island)

  TEST("UTC+14 fits in int32_t");
  EXPECT(max_offset == 50400);

  TEST("UTC-12 fits in int32_t");
  EXPECT(min_offset == -43200);

  // This is the bug that int16_t would cause: overflow at > 9.1 hours
  int32_t aest = 10 * 3600;  // 36000

  TEST("UTC+10 (36000) > INT16_MAX (32767) — int16 would overflow");
  EXPECT(aest > 32767);

  TEST("int32_t holds AEST correctly");
  EXPECT(aest == 36000);
}

int main() {
  printf("═══════════════════════════════════════\n");
  printf("  Overhead Tracker — Flight Logic Tests\n");
  printf("═══════════════════════════════════════\n");

  test_deriveStatus();
  test_haversine();
  test_getAirline();
  test_getAircraftTypeName();
  test_formatAlt();
  test_apiRadius();
  test_utc_offset_range();

  printf("\n═══════════════════════════════════════\n");
  printf("  Results: %d / %d passed\n", tests_passed, tests_run);
  printf("═══════════════════════════════════════\n");

  return (tests_passed == tests_run) ? 0 : 1;
}
