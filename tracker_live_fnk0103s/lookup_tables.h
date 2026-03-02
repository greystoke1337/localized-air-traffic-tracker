#pragma once
#include "types.h"

// ─── Airline lookup ───────────────────────────────────
const Airline AIRLINES[] = {
  {"QFA","QANTAS"},   {"VOZ","VIRGIN"},     {"JST","JETSTAR"},
  {"RXA","REX"},      {"UAE","EMIRATES"},   {"ETD","ETIHAD"},
  {"QTR","QATAR"},    {"SIA","SINGAPORE"},  {"ANZ","AIR NZ"},
  {"CPA","CATHAY"},   {"MAS","MALAYSIA"},   {"THA","THAI"},
  {"KAL","KOREAN"},   {"JAL","JAL"},        {"ANA","ANA"},
  {"AAL","AMERICAN"}, {"UAL","UNITED"},     {"BAW","BRITISH"},
  {"DAL","DELTA"},    {"AFR","AIR FRANCE"}, {"DLH","LUFTHANSA"},
  {"NWL","NETWORK"},  {"FJI","FIJI"},       {"EVA","EVA AIR"},
  {"CCA","AIR CHINA"},{"CSN","CHINA STH"},  {"CES","CHINA EST"},
  {"HAL","HAWAIIAN"},
  {"AIC","AIR INDIA"},{"TGW","SCOOT"},       {"LAN","LATAM"},
  {"CHH","HAINAN"},    {"CXA","XIAMEN"},      {"CEB","CEBU PAC"},
  {"VJC","VIETJET"},   {"CRK","HK AIRLINES"}, {"TWB","TWAY AIR"},
  {"ASA","ALASKA"},    {"CSC","SICHUAN"},      {"ACI","AIRCALIN"},
  {"ALK","SRILANKAN"}, {"ANG","AIR NIUGINI"}, {"CAL","CHINA AIR"},
  {"KLM","KLM"},       {"MXD","BATIK AIR"},   {"UTY","ALLIANCE"},
  {"AMX","AEROMEXICO"},{"FRE","FLYPELICAN"},
};
const int AIRLINE_COUNT = sizeof(AIRLINES) / sizeof(AIRLINES[0]);

// ─── Aircraft type lookup ─────────────────────────────
const AircraftType AIRCRAFT_TYPES[] = {
  {"B737","B737-700"},  {"B738","B737-800"},  {"B739","B737-900"},  {"B73X","B737-900ER"},
  {"B37M","B737 MAX 7"},{"B38M","B737 MAX 8"},{"B39M","B737 MAX 9"},{"B3XM","B737 MAX 10"},
  {"B752","B757-200"},  {"B753","B757-300"},
  {"B762","B767-200"},  {"B763","B767-300"},  {"B764","B767-400"},
  {"B772","B777-200"},  {"B77L","B777-200LR"},{"B773","B777-300"},  {"B77W","B777-300ER"},
  {"B788","B787-8"},    {"B789","B787-9"},    {"B78X","B787-10"},
  {"B712","B717-200"},
  {"B741","B747-100"},  {"B742","B747-200"},  {"B743","B747-300"},  {"B744","B747-400"},  {"B748","B747-8"},
  {"A318","A318"},      {"A319","A319"},      {"A320","A320"},      {"A321","A321"},
  {"A19N","A319neo"},   {"A20N","A320neo"},   {"A21N","A321neo"},   {"A21X","A321XLR"},
  {"A332","A330-200"},  {"A333","A330-300"},  {"A338","A330-800neo"},{"A339","A330-900neo"},
  {"A342","A340-200"},  {"A343","A340-300"},  {"A345","A340-500"},  {"A346","A340-600"},
  {"A359","A350-900"},  {"A35K","A350-1000"},
  {"A380","A380"},      {"A388","A380-800"},
  {"E170","E170"},      {"E175","E175"},      {"E190","E190"},      {"E195","E195"},
  {"E290","E190-E2"},   {"E295","E195-E2"},
  {"CRJ2","CRJ-200"},   {"CRJ7","CRJ-700"},   {"CRJ9","CRJ-900"},   {"CRJX","CRJ-1000"},
  {"DH8A","Dash 8-100"},{"DH8B","Dash 8-200"},{"DH8C","Dash 8-300"},{"DH8D","Dash 8-400"},
  {"AT43","ATR 42-300"},{"AT45","ATR 42-500"},{"AT72","ATR 72-200"},{"AT75","ATR 72-500"},{"AT76","ATR 72-600"},
  {"BCS1","A220-100"},  {"BCS3","A220-300"},
  {"SF34","SAAB 340"},  {"SB20","SAAB 2000"}, {"JS41","Jetstream 41"},
  {"PC12","Pilatus PC-12"},{"C208","Cessna Caravan"},
  {"GL5T","G550"},      {"GLEX","Global Express"},{"GLF6","G650"},
  {"C25A","Citation CJ2"},{"C25B","Citation CJ3"},
  {"FA7X","Falcon 7X"}, {"FA8X","Falcon 8X"},
  {"C130","C-130 Hercules"},{"P8","P-8 Poseidon"},
  {"EC35","H135"},      {"EC45","H145"},       {"S76","Sikorsky S-76"},
  {"B06","Bell 206"},   {"B407","Bell 407"},
};
const int AIRCRAFT_TYPE_COUNT = sizeof(AIRCRAFT_TYPES) / sizeof(AIRCRAFT_TYPES[0]);
