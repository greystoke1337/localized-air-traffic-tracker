#pragma once
#include "types.h"

// ─── Airline lookup ───────────────────────────────────
const Airline AIRLINES[] = {
  {"QFA","QANTAS",AL_RED},       {"VOZ","VIRGIN",AL_ROSE},      {"JST","JETSTAR",AL_ORANGE},
  {"RXA","REX",AL_GREEN},        {"UAE","EMIRATES",AL_GOLD},     {"ETD","ETIHAD",AL_GOLD},
  {"QTR","QATAR",AL_ROSE},       {"SIA","SINGAPORE",AL_GOLD},   {"ANZ","AIR NZ",AL_TEAL},
  {"CPA","CATHAY",AL_GREEN},     {"MAS","MALAYSIA",AL_ROSE},    {"THA","THAI",AL_VIOLET},
  {"KAL","KOREAN",AL_SKY},       {"JAL","JAL",AL_RED},          {"ANA","ANA",AL_SKY},
  {"AAL","AMERICAN",AL_SKY},     {"UAL","UNITED",AL_SKY},       {"BAW","BRITISH",AL_RED},
  {"DAL","DELTA",AL_SKY},        {"AFR","AIR FRANCE",AL_SKY},   {"DLH","LUFTHANSA",AL_GOLD},
  {"NWL","NETWORK",C_AMBER},     {"FJI","FIJI",AL_TEAL},        {"EVA","EVA AIR",AL_GREEN},
  {"CCA","AIR CHINA",AL_RED},    {"CSN","CHINA STH",AL_SKY},    {"CES","CHINA EST",AL_SKY},
  {"HAL","HAWAIIAN",AL_VIOLET},
  {"AIC","AIR INDIA",AL_ORANGE}, {"TGW","SCOOT",AL_GOLD},       {"LAN","LATAM",AL_SKY},
  {"CHH","HAINAN",AL_RED},       {"CXA","XIAMEN",AL_SKY},       {"CEB","CEBU PAC",AL_GOLD},
  {"VJC","VIETJET",AL_RED},      {"CRK","HK AIRLINES",AL_ROSE}, {"TWB","TWAY AIR",AL_ROSE},
  {"ASA","ALASKA",AL_SKY},       {"CSC","SICHUAN",AL_SKY},      {"ACI","AIRCALIN",AL_SKY},
  {"ALK","SRILANKAN",AL_GREEN},  {"ANG","AIR NIUGINI",AL_TEAL}, {"CAL","CHINA AIR",AL_ROSE},
  {"KLM","KLM",AL_SKY},          {"MXD","BATIK AIR",AL_RED},    {"UTY","ALLIANCE",AL_TEAL},
  {"AMX","AEROMEXICO",AL_SKY},   {"FRE","FLYPELICAN",C_AMBER},
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
