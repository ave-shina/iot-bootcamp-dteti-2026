/*
 * firebase_handler.h — API untuk semua operasi Firebase RTDB + door control.
 *
 * IMPLEMENTASI via Firebase REST API (HTTPClient + ArduinoJson),
 * BUKAN library Firebase_ESP_Client (mobizt). Lihat config.h untuk alasan.
 *
 * Modul ini meng-encapsulate:
 *   - setupWiFi() + setupFirebase()   — inisialisasi
 *   - publishSensor()                 — write DHT22 ke /sensor/01 (self-throttled)
 *   - setStatus() / pushAudit()       — write status & append audit log
 *   - startKontrolStream()            — initial seed untuk poll admin override
 *   - pollKontrol()                   — GET periodik /kontrol/pintu (ganti stream)
 *   - setupOutputs()                  — pinMode semua relay + LED
 *   - unlockDoor(source)              — buka pintu 3 detik lalu kunci otomatis
 */
#pragma once

#include "config.h"

// ----- Setup -----
void setupOutputs();        // panggil di setup() untuk init semua GPIO
void setupWiFi();
void setupFirebase();       // test konektivitas + set firebaseReady = true

// ----- Operasi RTDB (REST: PUT/POST) -----
void publishSensor();          // self-throttled (cek lastPublish internal)
void setStatus(const char* msg);
void pushAudit(const char* event);

// ----- Polling admin override (ganti "stream" mobizt) -----
// Panggil startKontrolStream() sekali di setup() untuk seed nilai awal,
// lalu pollKontrol() tiap iterasi loop() untuk deteksi perubahan.
void startKontrolStream();     // seed lastKontrolValue, tidak trigger unlock
void pollKontrol();            // self-throttled POLL_INTERVAL_MS

// ----- Door control -----
// State machine persisten (opsi C): UNLOCK dan LOCK dua state stabil.
// Tidak ada auto-lock — pintu tetap di state terakhir sampai ada perintah baru.
void unlockDoor(const char* source);
void lockDoor(const char* source);
