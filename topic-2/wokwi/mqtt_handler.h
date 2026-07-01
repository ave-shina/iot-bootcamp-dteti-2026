/*
 * mqtt_handler.h — API untuk semua operasi MQTT (pub/sub) + door control.
 *
 * Modul ini meng-encapsulate:
 *   - setupWiFi() + setupMQTT()    — inisialisasi WiFi & MQTT client
 *   - reconnect()                  — koneksi management (throttled 5s)
 *   - publishSensor()              — publish JSON DHT22 ke TOPIC_SENSOR (self-throttled)
 *   - publishStatus()              — publish audit/state ke TOPIC_STATUS
 *   - onMessage()                  — callback pesan masuk (admin override UNLOCK/LOCK)
 *   - setupOutputs()               — pinMode semua relay + LED
 *   - unlockDoor(source)           — buka pintu (persistent, no auto-lock)
 *   - lockDoor(source)             — kunci pintu (persistent)
 */
#pragma once

#include "config.h"

// ----- Setup -----
void setupOutputs();        // panggil di setup() untuk init semua GPIO
void setupWiFi();
void setupMQTT();           // setServer + setCallback + setKeepAlive

// ----- Connection management -----
bool reconnect();           // throttled 5 detik, returns true jika (re)connected

// ----- Publish -----
void publishSensor();       // self-throttled (cek lastPublish internal)
void publishStatus(const char* msg, bool retained = false);

// ----- Door control -----
// State machine persisten (opsi C): UNLOCK dan LOCK dua state stabil.
// Tidak ada auto-lock — pintu tetap di state terakhir sampai ada perintah baru.
void unlockDoor(const char* source);
void lockDoor(const char* source);

// ----- Callback (dipanggil client.loop(), jangan dipanggil manual) -----
void onMessage(char* topic, byte* payload, unsigned int length);
