/*
 * ============================================================================
 * Bootcamp IoT - Topik 2 (Pertemuan Online #2)
 * DEMO: ESP32 MQTT ALL-IN-ONE (Publisher + Subscriber dalam 1 board)
 * ============================================================================
 *
 * 1 ESP32 yang sekaligus jadi publisher (sensor DHT22) DAN subscriber
 * (kontrol akses pintu via MQTT admin override).
 *
 * ============================================================================
 * STRUKTUR FILE (modular .h + .cpp)
 * ============================================================================
 *
 *   sketch.c               ← MAIN ENTRY (file ini). Hanya setup() + loop().
 *   config.h     .cpp      ← Konstanta & globals (WiFi creds, MQTT broker,
 *                            topics, GPIO, DHT object).
 *   mqtt_handler.h .cpp    ← API MQTT + door control: setupWiFi, setupMQTT,
 *                            reconnect, publishSensor, publishStatus,
 *                            onMessage, setupOutputs, unlockDoor.
 *
 *   Dependencies:
 *     sketch.c    →  config.h
 *                →  mqtt_handler.h
 *
 * ============================================================================
 * FLOW PROGRAM (alur program):
 * ============================================================================
 *
 *   POWER ON / RESET
 *        │
 *        ▼
 *   ┌─────────────────────────────────────────────────────────┐
 *   │ setup()  ← dijalankan SEKALI                            │
 *   │   1. Serial.begin(115200)                               │
 *   │   2. setupOutputs()         ← mqtt_handler (pinMode)    │
 *   │   3. dht.begin()                                        │
 *   │   4. setupWiFi()            ← mqtt_handler              │
 *   │   5. setupMQTT()            ← mqtt_handler              │
 *   └─────────────────────────────────────────────────────────┘
 *        │
 *        ▼
 *   ┌─────────────────────────────────────────────────────────┐
 *   │ loop()  ← dijalankan TERUS-MENERUS                      │
 *   │                                                          │
 *   │   ┌── client.connected()? ─┐                             │
 *   │   │ TIDAK                  │ YA                           │
 *   │   ▼                        ▼                              │
 *   │  reconnect()         client.loop()  ← pompa callback      │
 *   │  (throttled 5s)           │                               │
 *   │                           ▼                               │
 *   │                    publishSensor()  ← self-throttled 5s   │
 *   └─────────────────────────────────────────────────────────┘
 *
 *   EVENT PATH (asynchronous):
 *
 *   Pesan MQTT masuk di TOPIC_KONTROL dengan payload "UNLOCK":
 *        onMessage() ter-trigger via client.loop()
 *          → publishStatus("ADMIN_REMOTE")
 *          → unlockDoor("mqtt-admin")
 *              ├─ relay HIGH + LED kuning HIGH
 *              ├─ publishStatus("UNLOCKED")
 *              ├─ delay(3000)  ← blocking 3 detik (demo)
 *              ├─ relay LOW + LED kuning LOW
 *              └─ publishStatus("LOCKED")
 *
 * ============================================================================
 * MQTT TOPICS (3 path):
 * ============================================================================
 *
 *   bootcamp/sensor/01    (ESP → broker)   JSON DHT22 tiap 5 detik
 *   bootcamp/kontrol/pintu (broker → ESP)  "UNLOCK" admin override
 *   bootcamp/status/pintu (ESP → broker)  audit + state (retained)
 *                                         UNLOCKED/LOCKED/ADMIN_REMOTE/
 *                                         online/offline (LWT)
 *
 * ============================================================================
 * Wiring (lihat diagram.json):
 * ============================================================================
 *
 *   DHT22  VCC -> 3V3,  GND -> GND,  SDA -> GPIO 4
 *   Relay module IN              -> GPIO 27 (VCC 5V, GND)
 *   LED "pintu" kuning (R 220)   -> GPIO 26  [indikator pintu terbuka]
 *   LED publish biru   (R 220)   -> GPIO 18  [blink tiap publish sensor]
 *   LED status hijau   (R 220)   -> GPIO 12  [on = MQTT connected]
 *
 * ============================================================================
 * Cara test:
 * ============================================================================
 *
 *   1. Start simulation → buka Serial Monitor (115200 baud).
 *   2. Sensor publish tiap 5 detik → LED biru blink + ">> Publish sensor OK".
 *   3. Klik tombol "Buka Pintu" di dashboard Node-RED → relay + LED kuning
 *      aktif 3 detik → audit "ADMIN_REMOTE" muncul.
 *   4. Atau via MQTT Explorer → publish "UNLOCK" ke "bootcamp/kontrol/pintu".
 *   5. Subscribe "bootcamp/sensor/#" + "bootcamp/status/pintu" → lihat data + audit.
 *
 * Catatan keamanan:
 *   - Broker test.mosquitto.org = PUBLIK tanpa auth.
 *   - Siapa saja bisa subscribe + publish UNLOCK ke topic Anda.
 *   - Untuk produksi: broker sendiri + auth + TLS + secure element.
 * ============================================================================
 */

#include "config.h"
#include "mqtt_handler.h"

// =====================================================
// SETUP — dijalankan SEKALI saat ESP32 boot
// =====================================================
void setup() {
  Serial.begin(115200);
  setupOutputs();                    // init semua GPIO (relay, LED)

  delay(300);
  Serial.println();
  Serial.println("============================================");
  Serial.println("  ESP32 MQTT ALL-IN-ONE (Sensor + Pintu)");
  Serial.println("============================================");
  Serial.println("  Kontrol pintu via dashboard Node-RED.");
  Serial.println("  Klik tombol 'Buka Pintu' untuk membuka.");
  Serial.println();

  dht.begin();                       // init sensor DHT22
  setupWiFi();                       // konek WiFi (blocking ~20s)
  setupMQTT();                       // setServer + setCallback + setKeepAlive

  // Cetak konfigurasi topic untuk verifikasi
  Serial.print("Topic sensor  : ");
  Serial.println(TOPIC_SENSOR);
  Serial.print("Topic kontrol : ");
  Serial.println(TOPIC_KONTROL);
  Serial.print("Topic status  : ");
  Serial.println(TOPIC_STATUS);
  Serial.println();
}

// =====================================================
// LOOP — dijalankan TERUS-MENERUS
// =====================================================
void loop() {
  if (!client.connected()) {
    reconnect();                     // throttled 5 detik internal
  } else {
    client.loop();                   // ⚠ WAJIB: pompa callback + keep alive MQTT
    publishSensor();                 // self-throttled (cek millis() internal, 5 detik)
  }
}
