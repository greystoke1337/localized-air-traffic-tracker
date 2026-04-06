#pragma once
#include "types.h"

// ─── Airline lookup ───────────────────────────────────
const Airline AIRLINES[] = {
  {"QFA","QANTAS",AL_RED},       {"QLK","QANTASLINK",AL_RED},   {"VOZ","VIRGIN",AL_ROSE},      {"JST","JETSTAR",AL_ORANGE},
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
  // Boeing narrow-body
  {"B737","B737-700","NARROW-BODY"},  {"B738","B737-800","NARROW-BODY"},
  {"B739","B737-900","NARROW-BODY"},  {"B73X","B737-900ER","NARROW-BODY"},
  {"B37M","B737 MAX 7","NARROW-BODY"},{"B38M","B737 MAX 8","NARROW-BODY"},
  {"B39M","B737 MAX 9","NARROW-BODY"},{"B3XM","B737 MAX 10","NARROW-BODY"},
  {"B752","B757-200","NARROW-BODY"},  {"B753","B757-300","NARROW-BODY"},
  {"B712","B717-200","NARROW-BODY"},
  // Boeing wide-body
  {"B762","B767-200","WIDE-BODY"},  {"B763","B767-300","WIDE-BODY"},  {"B764","B767-400","WIDE-BODY"},
  {"B772","B777-200","WIDE-BODY"},  {"B77L","B777-200LR","WIDE-BODY"},
  {"B773","B777-300","WIDE-BODY"},  {"B77W","B777-300ER","WIDE-BODY"},
  {"B788","B787-8","WIDE-BODY"},    {"B789","B787-9","WIDE-BODY"},    {"B78X","B787-10","WIDE-BODY"},
  // Boeing heavy
  {"B741","B747-100","HEAVY"},  {"B742","B747-200","HEAVY"},  {"B743","B747-300","HEAVY"},
  {"B744","B747-400","HEAVY"},  {"B748","B747-8","HEAVY"},
  // Airbus narrow-body
  {"A318","A318","NARROW-BODY"},    {"A319","A319","NARROW-BODY"},
  {"A320","A320","NARROW-BODY"},    {"A321","A321","NARROW-BODY"},
  {"A19N","A319neo","NARROW-BODY"}, {"A20N","A320neo","NARROW-BODY"},
  {"A21N","A321neo","NARROW-BODY"}, {"A21X","A321XLR","NARROW-BODY"},
  // Airbus wide-body
  {"A332","A330-200","WIDE-BODY"},  {"A333","A330-300","WIDE-BODY"},
  {"A338","A330-800neo","WIDE-BODY"},{"A339","A330-900neo","WIDE-BODY"},
  {"A342","A340-200","WIDE-BODY"},  {"A343","A340-300","WIDE-BODY"},
  {"A345","A340-500","WIDE-BODY"},  {"A346","A340-600","WIDE-BODY"},
  {"A359","A350-900","WIDE-BODY"},  {"A35K","A350-1000","WIDE-BODY"},
  // Airbus heavy
  {"A380","A380","HEAVY"},          {"A388","A380-800","HEAVY"},
  // Airbus / Bombardier narrow
  {"BCS1","A220-100","NARROW-BODY"},{"BCS3","A220-300","NARROW-BODY"},
  // Regional jets
  {"E170","E170","REGIONAL JET"},   {"E175","E175","REGIONAL JET"},
  {"E190","E190","REGIONAL JET"},   {"E195","E195","REGIONAL JET"},
  {"E290","E190-E2","REGIONAL JET"},{"E295","E195-E2","REGIONAL JET"},
  {"CRJ2","CRJ-200","REGIONAL JET"},{"CRJ7","CRJ-700","REGIONAL JET"},
  {"CRJ9","CRJ-900","REGIONAL JET"},{"CRJX","CRJ-1000","REGIONAL JET"},
  // Turboprops
  {"DH8A","Dash 8-100","TURBOPROP"},{"DH8B","Dash 8-200","TURBOPROP"},
  {"DH8C","Dash 8-300","TURBOPROP"},{"DH8D","Dash 8-400","TURBOPROP"},
  {"AT43","ATR 42-300","TURBOPROP"},{"AT45","ATR 42-500","TURBOPROP"},
  {"AT72","ATR 72-200","TURBOPROP"},{"AT75","ATR 72-500","TURBOPROP"},{"AT76","ATR 72-600","TURBOPROP"},
  {"SF34","SAAB 340","TURBOPROP"},  {"SB20","SAAB 2000","TURBOPROP"},{"JS41","Jetstream 41","TURBOPROP"},
  {"C208","Cessna Caravan","TURBOPROP"},
  {"B350","King Air 350","TURBOPROP"}, {"BE20","King Air 200","TURBOPROP"},
  {"BE9L","King Air 90","TURBOPROP"},  {"BE40","Beechjet 400","TURBOPROP"},
  {"P180","Piaggio Avanti","TURBOPROP"},{"SW4","Merlin/Metro","TURBOPROP"},
  {"DHC6","Twin Otter","TURBOPROP"},
  // Light aircraft
  {"C172","Cessna 172","LIGHT"},       {"C182","Cessna 182","LIGHT"},
  {"C206","Cessna 206","LIGHT"},       {"C210","Cessna 210","LIGHT"},
  {"C152","Cessna 152","LIGHT"},       {"C150","Cessna 150","LIGHT"},
  {"C402","Cessna 402","LIGHT"},       {"C310","Cessna 310","LIGHT"},
  {"C340","Cessna 340","LIGHT"},       {"C414","Cessna 414","LIGHT"},
  {"C421","Cessna 421","LIGHT"},
  {"PA28","Piper Cherokee","LIGHT"},   {"PA32","Piper Saratoga","LIGHT"},
  {"PA34","Piper Seneca","LIGHT"},     {"PA31","Piper Navajo","LIGHT"},
  {"PA44","Piper Seminole","LIGHT"},   {"PA46","Piper Malibu","LIGHT"},
  {"DA40","Diamond DA40","LIGHT"},     {"DA42","Diamond DA42","LIGHT"},
  {"DA62","Diamond DA62","LIGHT"},     {"DA20","Diamond DA20","LIGHT"},
  {"SR20","Cirrus SR20","LIGHT"},      {"SR22","Cirrus SR22","LIGHT"},
  {"M20P","Mooney M20","LIGHT"},       {"BE33","Bonanza","LIGHT"},
  {"BE36","Bonanza 36","LIGHT"},       {"BE58","Baron 58","LIGHT"},
  {"TB20","Socata TB20","LIGHT"},      {"P28A","Piper PA-28","LIGHT"},
  {"P32R","Piper PA-32R","LIGHT"},     {"AA5","Grumman Tiger","LIGHT"},
  {"CTLS","Flight Design CT","LIGHT"}, {"SPIT","Spitfire","LIGHT"},
  {"RV7","Vans RV-7","LIGHT"},         {"RV8","Vans RV-8","LIGHT"},
  // Seaplanes
  {"DHC2","Beaver","SEAPLANE"},        {"DHC3","Otter","SEAPLANE"},
  // Business jets — Gulfstream
  {"PC12","Pilatus PC-12","BIZ JET"},
  {"GL5T","G550","BIZ JET"},       {"GLEX","Global Express","BIZ JET"},{"GLF6","G650","BIZ JET"},
  {"GLF5","G-V","BIZ JET"},        {"GLF4","G-IV","BIZ JET"},         {"GLF3","G-III","BIZ JET"},
  {"GA5C","G500","BIZ JET"},       {"GA6C","G600","BIZ JET"},         {"G280","G280","BIZ JET"},
  {"ASTR","Astra/G100","BIZ JET"}, {"GALX","Galaxy/G200","BIZ JET"},
  // Business jets — Citation
  {"C25A","Citation CJ2","BIZ JET"},{"C25B","Citation CJ3","BIZ JET"},
  {"C56X","Citation Excel","BIZ JET"},{"C68A","Citation Lat.","BIZ JET"},
  {"C750","Citation X","BIZ JET"}, {"C700","Citation Long.","BIZ JET"},
  {"C510","Citation Mustang","BIZ JET"},{"C525","CitationJet","BIZ JET"},
  {"C560","Citation V","BIZ JET"}, {"C680","Citation Sov.","BIZ JET"},
  // Business jets — Learjet
  {"LJ45","Learjet 45","BIZ JET"}, {"LJ75","Learjet 75","BIZ JET"},
  {"LJ35","Learjet 35","BIZ JET"}, {"LJ60","Learjet 60","BIZ JET"},
  // Business jets — Dassault Falcon
  {"FA7X","Falcon 7X","BIZ JET"},  {"FA8X","Falcon 8X","BIZ JET"},
  {"F900","Falcon 900","BIZ JET"}, {"FA9X","Falcon 9X","BIZ JET"},
  {"F2TH","Falcon 2000","BIZ JET"},{"FA50","Falcon 50","BIZ JET"},
  // Business jets — Bombardier
  {"CL35","Challenger 350","BIZ JET"},{"CL60","Challenger 600","BIZ JET"},
  {"CL30","Challenger 300","BIZ JET"},{"BD70","Global 7500","BIZ JET"},{"GL7T","Global 7500","BIZ JET"},
  // Business jets — Embraer
  {"E55P","Phenom 300","BIZ JET"}, {"E50P","Phenom 100","BIZ JET"},
  {"E545","Legacy 450","BIZ JET"}, {"E550","Praetor 600","BIZ JET"},
  // Business jets — Hawker / other
  {"PRM1","Premier I","BIZ JET"},  {"H25B","Hawker 800","BIZ JET"},   {"HA4T","Hawker 4000","BIZ JET"},
  // Military
  {"C130","C-130 Hercules","MILITARY"},{"P8","P-8 Poseidon","MILITARY"},
  {"C17","C-17 Globemaster","MILITARY"},{"C30J","C-130J Super Herc","MILITARY"},
  {"K35R","KC-135","MILITARY"},     {"E3CF","E-7A Wedgetail","MILITARY"},
  {"A124","AN-124 Ruslan","MILITARY"}, {"H60","Black Hawk","MILITARY"},
  {"F18","F/A-18 Hornet","MILITARY"},  {"F35","F-35 Lightning","MILITARY"},
  {"EUFI","Eurofighter","MILITARY"},   {"A400","A400M Atlas","MILITARY"},
  {"KC30","KC-30A MRTT","MILITARY"},   {"B190","Beech 1900","MILITARY"},
  // Helicopters
  {"EC35","H135","HELICOPTER"},     {"EC45","H145","HELICOPTER"},
  {"S76","Sikorsky S-76","HELICOPTER"},
  {"B06","Bell 206","HELICOPTER"},  {"B407","Bell 407","HELICOPTER"},
  {"R22","Robinson R22","HELICOPTER"},{"R44","Robinson R44","HELICOPTER"},{"R66","Robinson R66","HELICOPTER"},
  {"AS50","AS350 Squirrel","HELICOPTER"},{"EC30","H130","HELICOPTER"},
  {"A139","AW139","HELICOPTER"},    {"S92","Sikorsky S-92","HELICOPTER"},
  {"BK17","BK117","HELICOPTER"},    {"B429","Bell 429","HELICOPTER"},
};
const int AIRCRAFT_TYPE_COUNT = sizeof(AIRCRAFT_TYPES) / sizeof(AIRCRAFT_TYPES[0]);
