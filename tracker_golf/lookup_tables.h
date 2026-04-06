#pragma once

// Aircraft type categories
#define CAT_UNKNOWN    0
#define CAT_NARROWBODY 1
#define CAT_WIDEBODY   2
#define CAT_JUMBO      3
#define CAT_REGIONAL   4
#define CAT_TURBOPROP  5
#define CAT_GA         6

struct AircraftName { const char icao[5]; const char name[16]; uint8_t category; };

static const AircraftName AIRCRAFT_NAMES[] = {
  // Boeing narrow-body
  {"B737","B737-700",   CAT_NARROWBODY}, {"B738","B737-800",   CAT_NARROWBODY},
  {"B739","B737-900",   CAT_NARROWBODY}, {"B73X","B737-900ER", CAT_NARROWBODY},
  {"B37M","B737 MAX 7", CAT_NARROWBODY}, {"B38M","B737 MAX 8", CAT_NARROWBODY},
  {"B39M","B737 MAX 9", CAT_NARROWBODY}, {"B3XM","B737 MAX 10",CAT_NARROWBODY},
  {"B752","B757-200",   CAT_NARROWBODY}, {"B753","B757-300",   CAT_NARROWBODY},
  {"B712","B717-200",   CAT_NARROWBODY},
  // Boeing wide-body
  {"B762","B767-200",   CAT_WIDEBODY},   {"B763","B767-300",   CAT_WIDEBODY},
  {"B764","B767-400",   CAT_WIDEBODY},
  {"B772","B777-200",   CAT_WIDEBODY},   {"B77L","B777-200LR", CAT_WIDEBODY},
  {"B773","B777-300",   CAT_WIDEBODY},   {"B77W","B777-300ER", CAT_WIDEBODY},
  {"B788","B787-8",     CAT_WIDEBODY},   {"B789","B787-9",     CAT_WIDEBODY},
  {"B78X","B787-10",    CAT_WIDEBODY},
  // Boeing jumbo
  {"B741","B747-100",   CAT_JUMBO},      {"B742","B747-200",   CAT_JUMBO},
  {"B743","B747-300",   CAT_JUMBO},      {"B744","B747-400",   CAT_JUMBO},
  {"B748","B747-8",     CAT_JUMBO},
  // Airbus narrow-body
  {"A318","A318",       CAT_NARROWBODY}, {"A319","A319",       CAT_NARROWBODY},
  {"A320","A320",       CAT_NARROWBODY}, {"A321","A321",       CAT_NARROWBODY},
  {"A19N","A319neo",    CAT_NARROWBODY}, {"A20N","A320neo",    CAT_NARROWBODY},
  {"A21N","A321neo",    CAT_NARROWBODY}, {"A21X","A321XLR",    CAT_NARROWBODY},
  // Airbus wide-body
  {"A332","A330-200",   CAT_WIDEBODY},   {"A333","A330-300",   CAT_WIDEBODY},
  {"A338","A330-800neo",CAT_WIDEBODY},   {"A339","A330-900neo",CAT_WIDEBODY},
  {"A342","A340-200",   CAT_WIDEBODY},   {"A343","A340-300",   CAT_WIDEBODY},
  {"A345","A340-500",   CAT_WIDEBODY},   {"A346","A340-600",   CAT_WIDEBODY},
  {"A359","A350-900",   CAT_WIDEBODY},   {"A35K","A350-1000",  CAT_WIDEBODY},
  // Airbus jumbo
  {"A380","A380",       CAT_JUMBO},      {"A388","A380-800",   CAT_JUMBO},
  // Embraer regional jets
  {"E170","E170",       CAT_REGIONAL},   {"E175","E175",       CAT_REGIONAL},
  {"E190","E190",       CAT_REGIONAL},   {"E195","E195",       CAT_REGIONAL},
  {"E75L","E175-E2",    CAT_REGIONAL},   {"E290","E190-E2",    CAT_REGIONAL},
  {"E295","E195-E2",    CAT_REGIONAL},
  // Bombardier / Airbus regional
  {"CRJ2","CRJ-200",    CAT_REGIONAL},   {"CRJ7","CRJ-700",    CAT_REGIONAL},
  {"CRJ9","CRJ-900",    CAT_REGIONAL},   {"CRJX","CRJ-1000",   CAT_REGIONAL},
  {"BCS1","A220-100",   CAT_REGIONAL},   {"BCS3","A220-300",   CAT_REGIONAL},
  // ATR / turboprop
  {"AT43","ATR 42-300", CAT_TURBOPROP},  {"AT45","ATR 42-500", CAT_TURBOPROP},
  {"AT72","ATR 72-200", CAT_TURBOPROP},  {"AT75","ATR 72-500", CAT_TURBOPROP},
  {"AT76","ATR 72-600", CAT_TURBOPROP},
  {"DH8A","DASH 8-100", CAT_TURBOPROP},  {"DH8B","DASH 8-200", CAT_TURBOPROP},
  {"DH8C","DASH 8-300", CAT_TURBOPROP},  {"DH8D","DASH 8-400", CAT_TURBOPROP},
  {"SF34","SAAB 340",   CAT_TURBOPROP},  {"SB20","SAAB 2000",  CAT_TURBOPROP},
  // General aviation
  {"PC12","PILATUS PC-12",CAT_GA},       {"C208","CESSNA CARAVAN",CAT_GA},
};
static const int AIRCRAFT_NAME_COUNT = sizeof(AIRCRAFT_NAMES) / sizeof(AIRCRAFT_NAMES[0]);

static uint16_t categoryToColor(uint8_t cat) {
  switch (cat) {
    case CAT_NARROWBODY: return C_CAT_NARROW;
    case CAT_WIDEBODY:   return C_CAT_WIDE;
    case CAT_JUMBO:      return C_CAT_JUMBO;
    case CAT_REGIONAL:   return C_CAT_REGIONAL;
    case CAT_TURBOPROP:  return C_CAT_TURBOPROP;
    case CAT_GA:         return C_WHITE;
    default:             return C_AMBER;
  }
}

// Resolves ICAO type code to a display name. Falls back to the raw code if not found.
static void resolveTypeName(const char *icao, char *out, size_t outLen) {
  for (int i = 0; i < AIRCRAFT_NAME_COUNT; i++) {
    if (strncasecmp(icao, AIRCRAFT_NAMES[i].icao, 4) == 0) {
      strncpy(out, AIRCRAFT_NAMES[i].name, outLen - 1);
      out[outLen - 1] = '\0';
      return;
    }
  }
  strncpy(out, icao, outLen - 1);
  out[outLen - 1] = '\0';
}

// Returns the category color for a raw ICAO type code.
static uint16_t getTypeColor(const char *icao) {
  for (int i = 0; i < AIRCRAFT_NAME_COUNT; i++) {
    if (strncasecmp(icao, AIRCRAFT_NAMES[i].icao, 4) == 0)
      return categoryToColor(AIRCRAFT_NAMES[i].category);
  }
  return C_AMBER;
}

// Airline ICAO prefix → brand color
struct AirlineEntry { const char prefix[4]; uint16_t color; };
static const AirlineEntry AIRLINE_COLORS[] = {
  // Australia / Pacific
  {"QFA", 0xF800}, {"VOZ", 0xF800}, {"JST", 0xF81F}, {"REX", 0x001F},
  {"ANZ", 0xFFFF}, {"FJI", 0x07FF},
  // Asia
  {"SIA", 0xF81F}, {"UAE", 0xF814}, {"QTR", 0xFFE0}, {"CPA", 0x001F},
  {"JAL", 0xF800}, {"ANA", 0x07FF}, {"KAL", 0x07FF}, {"MAS", 0xF800},
  {"THA", 0xFFE0},
  // Europe
  {"BAW", 0x07FF}, {"RYR", 0x07FF}, {"EZY", 0xF814}, {"DLH", 0xF81F},
  {"AFR", 0x07FF}, {"KLM", 0x07FF}, {"IBE", 0xFFE0}, {"VIR", 0xF800},
  // North America
  {"AAL", 0x07FF}, {"UAL", 0x07FF}, {"DAL", 0xF800}, {"SWA", 0xF81F},
  // Cargo
  {"FDX", 0xFFE0}, {"UPS", 0xF814}, {"DHL", 0xF81F},
};
static const int AIRLINE_COLOR_COUNT = sizeof(AIRLINE_COLORS) / sizeof(AIRLINE_COLORS[0]);

// Returns the brand color for an airline given the first 3 chars of its callsign.
static uint16_t getAirlineColor(const char *callsign) {
  if (!callsign || strlen(callsign) < 3) return C_AMBER;
  for (int i = 0; i < AIRLINE_COLOR_COUNT; i++) {
    if (strncasecmp(callsign, AIRLINE_COLORS[i].prefix, 3) == 0)
      return AIRLINE_COLORS[i].color;
  }
  return C_AMBER;
}

// ICAO → IATA airport code lookup
struct IcaoIataEntry { const char icao[5]; const char iata[4]; };
static const IcaoIataEntry ICAO_IATA[] = {
  // Australia
  {"YSSY","SYD"}, {"YMML","MEL"}, {"YBBN","BNE"}, {"YPAD","ADL"},
  {"YPPH","PER"}, {"YMAV","CBR"}, {"YBTL","TSV"}, {"YBCS","CNS"},
  {"YMEN","MEB"}, {"YHBA","HBA"}, {"YBMC","MCY"}, {"YGOL","OOL"},
  {"YSCB","CBR"}, {"YMBA","MKY"},
  // New Zealand
  {"NZAA","AKL"}, {"NZCH","CHC"}, {"NZWN","WLG"}, {"NZQN","ZQN"},
  // United Kingdom
  {"EGLL","LHR"}, {"EGKK","LGW"}, {"EGGW","LTN"}, {"EGSS","STN"},
  {"EGPH","EDI"}, {"EGCC","MAN"},
  // Europe
  {"LFPG","CDG"}, {"LEMD","MAD"}, {"EHAM","AMS"}, {"EDDF","FRA"},
  {"LIRF","FCO"}, {"LEBL","BCN"},
  // Asia
  {"WSSS","SIN"}, {"VHHH","HKG"}, {"RJTT","HND"}, {"RJAA","NRT"},
  {"RKSI","ICN"}, {"VTBS","BKK"}, {"WMKK","KUL"}, {"VIDP","DEL"},
  {"VABB","BOM"}, {"OMDB","DXB"}, {"OERK","RUH"},
  // North America (most US follow KXXX→XXX — handled by fallback below)
  {"KORD","ORD"}, {"KLAX","LAX"}, {"KJFK","JFK"}, {"KATL","ATL"},
  {"KDEN","DEN"}, {"KSFO","SFO"}, {"KLAS","LAS"}, {"KMIA","MIA"},
  {"KMDW","MDW"}, {"KMSP","MSP"}, {"KBOS","BOS"}, {"KDTW","DTW"},
  {"KPHX","PHX"}, {"KSEA","SEA"}, {"KEWR","EWR"}, {"KLGA","LGA"},
  {"CYYZ","YYZ"}, {"CYVR","YVR"}, {"CYUL","YUL"},
};
static const int ICAO_IATA_COUNT = sizeof(ICAO_IATA) / sizeof(ICAO_IATA[0]);

// Converts an ICAO airport code to an IATA code. Falls back to stripping
// the leading K (US ICAO) or raw input truncated to 4 chars if not in table.
static void icaoToIata(const char *icao, char *out, size_t outLen) {
  if (!icao || !icao[0]) { out[0] = '\0'; return; }
  for (int i = 0; i < ICAO_IATA_COUNT; i++) {
    if (strncasecmp(icao, ICAO_IATA[i].icao, 4) == 0) {
      strncpy(out, ICAO_IATA[i].iata, outLen - 1);
      out[outLen - 1] = '\0';
      return;
    }
  }
  // Fallback: strip leading K (US ICAO) or use raw input, max 4 chars
  const char *src = (icao[0] == 'K' && icao[1] != '\0') ? icao + 1 : icao;
  strncpy(out, src, outLen - 1);
  out[outLen - 1] = '\0';
  if (strlen(out) > 4) out[4] = '\0';
}
