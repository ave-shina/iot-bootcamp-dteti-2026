/*
 * config.cpp — Definisi konstanta & shared globals.
 * Edit nilai di sini, BUKAN di config.h.
 */
#include "config.h"

// =====================
// WiFi credentials (Wokwi default)
// =====================
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// =====================
// Firebase configuration — GANTI DENGAN PROJECT ANDA
//
// Cara dapatkan:
//   1. console.firebase.google.com → buat project
//   2. Aktifkan Realtime Database (test mode)
//   3. Project Settings → Web App → copy firebaseConfig
//
// API_KEY tidak dipakai untuk test mode (rules publik). Disimpan untuk
// kompatibilitas future bila ingin mengaktifkan auth.
// =====================
const char* DATABASE_URL = "https://YOUR-PROJECT-default-rtdb.firebaseio.com/";
const char* API_KEY      = "YOUR-API-KEY";

// =====================
// Timing
// =====================
const unsigned long PUBLISH_INTERVAL_MS = 15000UL;  // 15 detik — publish sensor
const unsigned long POLL_INTERVAL_MS    = 5000UL;   // 5 detik — poll admin override

// =====================
// Shared hardware globals
// =====================
DHT    dht(PIN_DHT, DHT_TYPE);

// =====================
// Firebase REST shared state
// =====================
WiFiClientSecure sslClient;          // di-setup di setupFirebase() (setInsecure)
bool            firebaseReady    = false;
unsigned long   lastPublish      = 0;
unsigned long   lastPoll         = 0;
String          lastKontrolValue = "";
