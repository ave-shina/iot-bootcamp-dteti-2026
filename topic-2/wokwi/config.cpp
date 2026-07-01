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
// MQTT broker configuration
// test.mosquitto.org = broker publik Eclipse (no auth, plaintext).
// Untuk produksi: ganti ke broker sendiri + auth + TLS (port 8883).
// =====================
const char*    MQTT_BROKER = "test.mosquitto.org";
const uint16_t MQTT_PORT   = 1883;

// =====================
// MQTT topics
// Ganti prefix 'bootcamp' jadi 'bootcamp/<nama-anda>' supaya tidak
// tabrakan dengan peserta lain di broker publik.
// =====================
const char* TOPIC_SENSOR   = "bootcamp/sensor/01";
const char* TOPIC_KONTROL  = "bootcamp/kontrol/pintu";
const char* TOPIC_STATUS   = "bootcamp/status/pintu";
const char* TOPIC_PRESENCE = "bootcamp/status/presence";

String      MQTT_CLIENT_ID = "esp32_all_in_01";

// =====================
// Timing
// =====================
const unsigned long PUBLISH_INTERVAL_MS = 5000UL;   // 5 detik

// =====================
// Shared hardware globals (di-construct di sini)
// =====================
DHT    dht(PIN_DHT, DHT_TYPE);
