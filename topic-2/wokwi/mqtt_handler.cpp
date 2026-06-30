/*
 * mqtt_handler.cpp — Implementasi semua operasi MQTT + door control.
 */
#include "mqtt_handler.h"

// =====================
// setupOutputs() — pinMode + initial state semua GPIO
//
// Panggil di awal setup() SEBELUM dht.begin() atau apa pun.
// =====================
void setupOutputs() {
  pinMode(PIN_RELAY,   OUTPUT);
  pinMode(PIN_PINTU,   OUTPUT);
  pinMode(PIN_PUBLISH, OUTPUT);
  pinMode(PIN_LED,     OUTPUT);

  digitalWrite(PIN_RELAY,   LOW);   // relay awal OFF (pintu terkunci)
  digitalWrite(PIN_PINTU,   LOW);   // LED kuning awal OFF
  digitalWrite(PIN_PUBLISH, LOW);   // LED biru awal OFF
  digitalWrite(PIN_LED,     LOW);   // LED hijau awal OFF (MQTT belum konek)
}

// =====================
// unlockDoor(source) — buka pintu & TETAP terbuka (no auto-lock)
//
// State machine persisten (opsi C). Pintu hanya akan tertutup bila
// ada perintah LOCK yang masuk. Status dipublish retained supaya
// subscriber baru langsung dapat state terakhir.
//
// source: label audit untuk identifikasi trigger (mis. "mqtt-admin")
// =====================
void unlockDoor(const char* source) {
  Serial.println("\n========================================");
  Serial.printf(">> [DOOR] 🔓 TERBUKA (source=%s)\n", source);
  Serial.println(">> [DOOR] Status pintu : UNLOCKED");
  Serial.println(">> [DOOR] Relay GPIO27 : HIGH  (solenoid aktif)");
  Serial.println(">> [DOOR] LED GPIO26   : ON    (kuning steady)");
  Serial.println("========================================");

  digitalWrite(PIN_RELAY, HIGH);   // energize relay → solenoid tertarik
  digitalWrite(PIN_PINTU, HIGH);   // nyalakan LED kuning (selama UNLOCKED)
  publishStatus("ADMIN_REMOTE");          // audit event (not retained)
  publishStatus("UNLOCKED", true);        // state retained = persisten
}

// =====================
// lockDoor(source) — kunci pintu & TETAP tertutup
//
// Inverse dari unlockDoor. Dipanggil saat perintah LOCK diterima.
// =====================
void lockDoor(const char* source) {
  Serial.println("\n========================================");
  Serial.printf(">> [DOOR] 🔒 TERKUNCI (source=%s)\n", source);
  Serial.println(">> [DOOR] Status pintu : LOCKED");
  Serial.println(">> [DOOR] Relay GPIO27 : LOW   (solenoid lepas)");
  Serial.println(">> [DOOR] LED GPIO26   : OFF   (kuning mati)");
  Serial.println("========================================");

  digitalWrite(PIN_RELAY, LOW);    // matikan relay → solenoid kembali
  digitalWrite(PIN_PINTU, LOW);    // matikan LED kuning
  publishStatus("ADMIN_LOCK");            // audit event (not retained)
  publishStatus("LOCKED", true);          // state retained = persisten
}

// =====================
// MQTT shared objects (di-define di sini, di-expose via config.h)
// =====================
WiFiClient    espClient;
PubSubClient  client(espClient);

unsigned long lastReconnect = 0;
unsigned long lastPublish   = 0;

// =====================
// setupWiFi() — koneksi WiFi (blocking dengan timeout 20s)
// =====================
void setupWiFi() {
  delay(10);
  Serial.print("Menghubungkan WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\n✓ WiFi OK. IP: ");
    Serial.print(WiFi.localIP());
    Serial.print("  RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("\n✗ WiFi gagal.");
  }
}

// =====================
// setupMQTT() — konfigurasi server, keep alive, timeout, callback
//
// Panggil SEKALI di setup() setelah setupWiFi().
// =====================
void setupMQTT() {
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setKeepAlive(60);          // keep alive 60 detik
  client.setSocketTimeout(10);      // timeout socket 10 detik
  client.setCallback(onMessage);    // ⬅ daftarkan callback onMessage
}

// =====================
// reconnect() — reconnect MQTT dengan throttle 5 detik
//
// Non-blocking: bila belum 5 detik sejak upaya terakhir, langsung return false.
// Setiap upaya connect() juga set Last Will Testament (LWT) supaya broker
// publish "offline" ke TOPIC_STATUS bila ESP disconnect mendadak.
// =====================
bool reconnect() {
  if (client.connected()) return true;
  if (millis() - lastReconnect < 5000UL) return false;
  lastReconnect = millis();

  Serial.print("Reconnect MQTT... ");
  // connect() params:
  //   client_id, username, password, willTopic, willQoS, willRetain, willMessage
  bool ok = client.connect(
    MQTT_CLIENT_ID.c_str(),   // client ID unik (anti kick dari broker)
    NULL, NULL,               // username & password (NULL = anonim)
    TOPIC_STATUS,             // will topic: broker publish ke sini bila ESP mati
    1,                        // will QoS = 1 (at least once)
    true,                     // will retained = true
    "offline"                 // will payload (LWT message)
  );

  if (ok) {
    Serial.println("OK");
    // Re-subscribe setelah connect (state sesi hilang saat disconnect)
    client.subscribe(TOPIC_KONTROL);
    Serial.print("  Subscribed: ");
    Serial.println(TOPIC_KONTROL);

    // Overwrite LWT dengan "online" (retained)
    client.publish(TOPIC_STATUS, "online", true);
    digitalWrite(PIN_LED, HIGH);     // LED hijau = MQTT connected
  } else {
    Serial.print("FAIL state=");
    Serial.println(client.state());  // state() = kode error (lihat PubSubClient.h)
    digitalWrite(PIN_LED, LOW);
  }
  return ok;
}

// =====================
// publishStatus() — kirim event audit/state ke TOPIC_STATUS
//
// retained=true → broker simpan sebagai state terakhir (dashboard langsung
// dapat nilai terakhir saat subscribe).
// =====================
void publishStatus(const char* msg, bool retained) {
  if (client.connected()) {
    client.publish(TOPIC_STATUS, msg, retained);
    Serial.printf(">> Audit publish: %s\n", msg);
  }
}

// =====================
// publishSensor() — baca DHT22 + publish JSON ke TOPIC_SENSOR
//
// Self-throttled: cek millis() internal, skip bila belum waktunya.
// Caller cukup panggil setiap iterasi loop() saat MQTT connected.
// =====================
void publishSensor() {
  if (millis() - lastPublish < PUBLISH_INTERVAL_MS) return;
  lastPublish = millis();

  float suhu   = dht.readTemperature();
  float lembap = dht.readHumidity();

  if (isnan(suhu) || isnan(lembap)) {
    Serial.println("!! DHT22 baca gagal — skip publish.");
    return;
  }

  StaticJsonDocument<160> doc;
  doc["id"]     = "sensor_01";
  doc["suhu"]   = roundf(suhu * 10.0f) / 10.0f;
  doc["lembap"] = roundf(lembap * 10.0f) / 10.0f;
  doc["ts"]     = millis();
  doc["rssi"]   = WiFi.RSSI();

  char buf[200];
  size_t n = serializeJson(doc, buf, sizeof(buf));

  if (client.publish(TOPIC_SENSOR, buf)) {
    Serial.printf(">> Publish sensor OK (%u bytes): %s\n", (unsigned)n, buf);
    digitalWrite(PIN_PUBLISH, HIGH);   // LED biru ON
    delay(30);
    digitalWrite(PIN_PUBLISH, LOW);    // LED biru OFF
  } else {
    Serial.println("!! Publish sensor gagal.");
  }
}

// =====================
// onMessage() — callback dipanggil client.loop() saat pesan MQTT masuk
//
// Konversi payload byte[] → string null-terminated, lalu routing per topic.
// =====================
void onMessage(char* topic, byte* payload, unsigned int length) {
  // 1. Konversi payload byte[] → string null-terminated
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';

  Serial.print("<< Pesan masuk topic=");
  Serial.print(topic);
  Serial.print(" payload=");
  Serial.println(message);

  // 2. Routing per topic (siap multi-topic di kemudian hari)
  if (String(topic) == TOPIC_KONTROL) {
    // Normalisasi: uppercase + trim → case-insensitive
    String cmd = String(message);
    cmd.toUpperCase();
    cmd.trim();

    if (cmd == "UNLOCK") {
      unlockDoor("mqtt-admin");
    } else if (cmd == "LOCK") {
      lockDoor("mqtt-admin");
    } else {
      Serial.print("!! Perintah tidak dikenal: ");
      Serial.println(message);
    }
  }
}
