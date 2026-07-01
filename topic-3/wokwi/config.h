/*
 * config.h — Semua deklarasi konstanta, GPIO pin, dan extern globals.
 * Dipakai oleh sketch.cpp + firebase_handler.
 *
 * Instruksi:
 *   - Edit konstanta di config.cpp (bukan di header ini).
 *   - Tambah extern declaration di sini bila perlu share variabel antar modul.
 *
 * Catatan: Topik 3 memakai Firebase RTDB via REST API (HTTPClient + ArduinoJson),
 * BUKAN library Firebase_ESP_Client (mobizt). Alasan: compile time turun ~80%.
 */
#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// =====================
// WiFi credentials
// =====================
extern const char* WIFI_SSID;
extern const char* WIFI_PASS;

// =====================
// Firebase configuration
// GANTI di config.cpp dengan URL + Key project Anda
//
// DATABASE_URL: full URL RTDB, mis.
//   "https://YOUR-PROJECT-default-rtdb.firebaseio.com/"
// API_KEY:      tidak dipakai bila database dalam test mode (rules publik).
//               Disimpan untuk migrasi future bila pakai auth.
// =====================
extern const char* DATABASE_URL;
extern const char* API_KEY;

// =====================
// GPIO pin assignments (compile-time constants)
// =====================
#define PIN_DHT     4      // DHT22 SDA
#define PIN_RELAY   27     // Relay module IN (sim. solenoid lock)
#define PIN_PINTU   26     // LED kuning — indikator pintu terbuka
#define PIN_PUBLISH 18     // LED biru — blink tiap setJSON sensor
#define PIN_LED     12     // LED hijau — Firebase ready indicator
#define DHT_TYPE    DHT22

// =====================
// Timing
// =====================
extern const unsigned long PUBLISH_INTERVAL_MS;   // 5000 ms — interval publish sensor
extern const unsigned long POLL_INTERVAL_MS;      // 1500 ms — interval poll /kontrol/pintu

// =====================
// Shared hardware globals (defined in config.cpp)
// =====================
extern DHT        dht;

// =====================
// Firebase REST shared state (defined in firebase_handler.cpp / config.cpp)
// =====================
extern WiFiClientSecure sslClient;     // koneksi HTTPS ke Firebase RTDB
extern bool            firebaseReady;  // true setelah setupFirebase() sukses konek
extern unsigned long   lastPublish;    // timestamp publish sensor terakhir
extern unsigned long   lastPoll;       // timestamp poll /kontrol/pintu terakhir
extern String          lastKontrolValue; // value terakhir yang dibaca dari /kontrol/pintu
