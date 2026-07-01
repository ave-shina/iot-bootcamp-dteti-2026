/*
 * config.h — Semua deklarasi konstanta, GPIO pin, dan extern globals.
 * Dipakai oleh sketch.cpp + mqtt_handler.
 *
 * Instruksi:
 *   - Edit konstanta di config.cpp (bukan di header ini).
 *   - Tambah extern declaration di sini bila perlu share variabel antar modul.
 */
#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// =====================
// WiFi credentials
// =====================
extern const char* WIFI_SSID;
extern const char* WIFI_PASS;

// =====================
// MQTT broker configuration
// =====================
extern const char*    MQTT_BROKER;
extern const uint16_t MQTT_PORT;

// =====================
// MQTT topics
// =====================
extern const char* TOPIC_SENSOR;     // publish data DHT22
extern const char* TOPIC_KONTROL;    // subscribe admin UNLOCK / LOCK
extern const char* TOPIC_STATUS;     // publish state pintu (LOCKED/UNLOCKED) + audit
extern const char* TOPIC_PRESENCE;   // publish presence (online/offline LWT)
extern String      MQTT_CLIENT_ID;   // harus unik di broker

// =====================
// GPIO pin assignments (compile-time constants)
// =====================
#define PIN_DHT     4      // DHT22 SDA
#define PIN_RELAY   27     // Relay module IN (sim. solenoid lock)
#define PIN_PINTU   26     // LED kuning — indikator pintu terbuka
#define PIN_PUBLISH 18     // LED biru — blink tiap publish sensor
#define PIN_LED     12     // LED hijau — MQTT connected indicator
#define DHT_TYPE    DHT22

// =====================
// Timing
// =====================
extern const unsigned long PUBLISH_INTERVAL_MS;   // 5000 ms

// =====================
// Shared hardware globals (defined in config.cpp)
// =====================
extern DHT        dht;

// =====================
// MQTT shared objects (defined in mqtt_handler.cpp)
// =====================
extern WiFiClient    espClient;
extern PubSubClient  client;
extern unsigned long lastReconnect;   // timestamp upaya reconnect terakhir
extern unsigned long lastPublish;     // timestamp publish sensor terakhir
