/*
 * firebase_handler.cpp — Implementasi semua operasi Firebase RTDB + door control.
 *
 * IMPLEMENTASI via Firebase REST API:
 *   PUT  /<path>.json   → overwrite state  (setJSON / setString)
 *   POST /<path>.json   → push auto-ID     (pushJSON)
 *   GET  /<path>.json   → read value       (polling /kontrol/pintu)
 *
 * Test mode (rules publik): tidak butuh ?auth=... di URL.
 * Produksi: tambahkan auth param bila rules menuntutnya.
 */
#include "firebase_handler.h" 
// 

// =====================
// rtdbUrl(path) — bangun URL REST API Firebase RTDB
//
// path: mis. "/sensor/01", "/status/pintu", "/logs/pintu"
// return: "https://<project>.firebaseio.com/sensor/01.json"
// =====================
static String rtdbUrl(const char* path) {
  String base = String(DATABASE_URL);
  if (base.endsWith("/")) base.remove(base.length() - 1);
  return base + path + ".json";
}

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
  digitalWrite(PIN_LED,     LOW);   // LED hijau awal OFF (Firebase belum ready)
}

// =====================
// unlockDoor(source) — buka pintu 3 detik lalu kunci otomatis
//
// source: label audit untuk identifikasi trigger (mis. "firebase-admin")
// =====================
void unlockDoor(const char* source) {
  // === STATE: UNLOCKED (persisten, no auto-lock) ===
  Serial.println("\n========================================");
  Serial.printf(">> [DOOR] 🔓 TERBUKA (source=%s)\n", source);
  Serial.println(">> [DOOR] Status pintu : UNLOCKED");
  Serial.println(">> [DOOR] Relay GPIO27 : HIGH  (solenoid aktif)");
  Serial.println(">> [DOOR] LED GPIO26   : ON    (kuning steady)");
  Serial.println(">> [DOOR] Catatan     : pintu TETAP terbuka sampai LOCK");
  Serial.println("========================================");

  digitalWrite(PIN_RELAY, HIGH);   // energize relay → solenoid tertarik
  digitalWrite(PIN_PINTU, HIGH);   // LED kuning ON selama UNLOCKED
  setStatus("UNLOCKED");
}

// =====================
// lockDoor(source) — kunci pintu & TETAP tertutup
//
// Inverse dari unlockDoor. Dipanggil saat perintah LOCK diterima.
// =====================
void lockDoor(const char* source) {
  // === STATE: LOCKED (persisten) ===
  Serial.println("\n========================================");
  Serial.printf(">> [DOOR] 🔒 TERKUNCI (source=%s)\n", source);
  Serial.println(">> [DOOR] Status pintu : LOCKED");
  Serial.println(">> [DOOR] Relay GPIO27 : LOW   (solenoid lepas)");
  Serial.println(">> [DOOR] LED GPIO26   : OFF   (kuning mati)");
  Serial.println("========================================");

  digitalWrite(PIN_RELAY, LOW);    // matikan relay → solenoid kembali
  digitalWrite(PIN_PINTU, LOW);    // matikan LED kuning
  setStatus("LOCKED");
}

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
// setupFirebase() — test konektivitas RTDB via GET path aman
//
// Test mode = rules publik → tidak butuh auth token.
// Bila GET sukses (200/4xx), firebaseReady = true.
// =====================
void setupFirebase() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠ setupFirebase: WiFi belum konek, skip.");
    return;
  }

  // Tangkap placeholder DATABASE_URL lebih awal — pesan jelas ke user.
  String urlCheck = String(DATABASE_URL);
  if (urlCheck.indexOf("YOUR-PROJECT") >= 0 || urlCheck.indexOf("YOUR-") >= 0) {
    Serial.println("✗ DATABASE_URL masih placeholder.");
    Serial.println("  Edit config.cpp baris 24 → ganti dengan URL RTDB project Anda.");
    Serial.println("  Cara: Firebase Console → Realtime Database → copy URL di atas.");
    firebaseReady = false;
    return;
  }

  // Bootcamp simplification: skip cert validation (sama seperti mobizt default).
  sslClient.setInsecure();
  sslClient.setTimeout(5000);

  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);

  // GET path yang pasti ada/tidak ada — cukup untuk verifikasi konektivitas.
  String url = rtdbUrl("/status/pintu");
  if (!http.begin(sslClient, url)) {
    Serial.println("⚠ HTTP begin gagal — cek DATABASE_URL.");
    return;
  }

  int code = http.GET();
  http.end();

  if (code > 0 && code < 500) {
    firebaseReady = true;
    Serial.printf("✓ Firebase reachable (HTTP %d). ready=YES\n", code);
  } else {
    Serial.printf("⚠ Firebase GET gagal (HTTP %d) — cek DATABASE_URL & rules.\n",
                  code);
    firebaseReady = false;
  }
}

// =====================
// setStatus(msg) — PUT /status/pintu.json = "msg"
//
// Body JSON-encoded string (mis. "UNLOCKED" lengkap dengan tanda kutip).
// =====================
void setStatus(const char* msg) {
  if (!firebaseReady) return;
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  if (!http.begin(sslClient, rtdbUrl("/status/pintu"))) {
    Serial.println("!! setStatus: HTTP begin gagal.");
    return;
  }
  http.addHeader("Content-Type", "application/json");

  String body = "\"" + String(msg) + "\"";   // JSON-encoded string
  int code = http.PUT(body);
  http.end();

  if (code == 200) {
    Serial.printf(">> Status set: %s\n", msg);
  } else {
    Serial.printf("!! setStatus gagal: HTTP %d\n", code);
  }
}

// =====================
// pushAudit(event) — POST /logs/pintu.json → append entry baru
//
// Body: JSON object {event, ts, rssi}. Response: {"name":"<auto-id>"}.
// =====================
void pushAudit(const char* event) {
  if (!firebaseReady) return;
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  if (!http.begin(sslClient, rtdbUrl("/logs/pintu"))) {
    Serial.println("!! pushAudit: HTTP begin gagal.");
    return;
  }
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<128> doc;
  doc["event"] = event;
  doc["ts"]    = (int)(millis() / 1000);   // detik (hemat space)
  doc["rssi"]  = WiFi.RSSI();

  String body;
  serializeJson(doc, body);

  int code = http.POST(body);
  String resp = http.getString();
  http.end();

  if (code == 200) {
    StaticJsonDocument<128> rdoc;
    if (!deserializeJson(rdoc, resp)) {
      const char* name = rdoc["name"];
      Serial.printf(">> Audit pushed: %s (key=%s)\n",
                    event, name ? name : "?");
    } else {
      Serial.printf(">> Audit pushed: %s\n", event);
    }
  } else {
    Serial.printf("!! pushAudit gagal: HTTP %d\n", code);
  }
}

// =====================
// publishSensor() — baca DHT22 + PUT /sensor/01.json
//
// Self-throttled: cek millis() internal, skip bila belum waktunya.
// Caller cukup panggil setiap iterasi loop().
// =====================
void publishSensor() {
  if (millis() - lastPublish < PUBLISH_INTERVAL_MS) return;
  lastPublish = millis();

  float rawSuhu   = dht.readTemperature();
  float rawLembap = dht.readHumidity();

  if (isnan(rawSuhu) || isnan(rawLembap)) {
    Serial.println("!! DHT22 baca gagal — skip publish.");
    return;
  }

  // Tambahkan jitter random supaya data bervariasi.
  // (DHT22 di Wokwi default mengembalikan nilai konstan 25°C / 50%.)
  float suhu   = rawSuhu   + (random(-200, 301) / 100.0);   // baseline ±2 °C
  float lembap = rawLembap + (random(-500, 501) / 100.0);   // baseline ±5 %

  // Clamp ke range realistis (mencegah nilai ekstrem dari noise RNG).
  suhu   = constrain(suhu,   20.0, 40.0);
  lembap = constrain(lembap, 30.0, 90.0);

  StaticJsonDocument<128> doc;
  doc["id"]     = "sensor_01";
  doc["suhu"]   = roundf(suhu * 10.0f) / 10.0f;
  doc["lembap"] = roundf(lembap * 10.0f) / 10.0f;
  doc["ts"]     = millis();
  doc["rssi"]   = WiFi.RSSI();

  String body;
  serializeJson(doc, body);

  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  if (!http.begin(sslClient, rtdbUrl("/sensor/01"))) {
    Serial.println("!! publishSensor: HTTP begin gagal.");
    return;
  }
  http.addHeader("Content-Type", "application/json");

  int code = http.PUT(body);
  http.end();

  if (code == 200) {
    Serial.printf(">> Write /sensor/01 OK (suhu=%.1f lembap=%.1f)\n", suhu, lembap);
    digitalWrite(PIN_PUBLISH, HIGH);   // LED biru ON
    delay(30);
    digitalWrite(PIN_PUBLISH, LOW);    // LED biru OFF
  } else {
    Serial.printf("!! Write sensor gagal: HTTP %d\n", code);
  }
}

// =====================
// parseKontrolValue(body) — parse body JSON string → String uppercase
//
// Body mungkin: "UNLOCK" (valid), null (path kosong), "" (jarang).
// Return true jika valid (out diisi), false jika null/empty/parse error.
// =====================
static bool parseKontrolValue(const String& body, String& out) {
  String b = body;
  b.trim();
  if (b.length() == 0 || b.equalsIgnoreCase("null")) return false;

  StaticJsonDocument<64> doc;
  if (deserializeJson(doc, b)) {
    Serial.print("!! pollKontrol parse error: ");
    Serial.println(b);
    return false;
  }
  const char* val = doc.as<const char*>();
  if (!val) return false;

  out = String(val);
  out.toUpperCase();
  out.trim();
  return true;
}

// =====================
// startKontrolStream() — initial seed untuk polling admin override
//
// Panggil SEKALI di setup() setelah setupFirebase(). Lakukan GET awal,
// simpan sebagai lastKontrolValue TANPA trigger unlock (hanya seed).
// Sesudah ini, pollKontrol() di loop() akan deteksi perubahan.
// =====================
void startKontrolStream() {
  if (!firebaseReady) return;

  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  if (!http.begin(sslClient, rtdbUrl("/kontrol/pintu"))) {
    Serial.println("⚠ startKontrolStream: HTTP begin gagal.");
    return;
  }
  int code = http.GET();
  String body = http.getString();
  http.end();

  if (code == 200) {
    String v;
    if (parseKontrolValue(body, v)) {
      lastKontrolValue = v;
      Serial.printf("✓ Seed /kontrol/pintu = %s\n", v.c_str());

      // State restoration: sinkronkan state pintu dengan nilai Firebase.
      // Dengan state machine persisten (opsi C), nilai kontrol = desired state.
      // Jadi saat boot, pintu harus langsung restore ke state tersebut.
      if (v == "UNLOCK") {
        Serial.println(">> [SEED] Restore state → UNLOCKED");
        unlockDoor("firebase-boot-restore");
      } else if (v == "LOCK") {
        Serial.println(">> [SEED] Restore state → LOCKED");
        lockDoor("firebase-boot-restore");
      } else {
        Serial.printf(">> [SEED] Value '%s' bukan UNLOCK/LOCK — pintu tetap LOCKED.\n",
                      v.c_str());
      }
    } else {
      Serial.println("✓ /kontrol/pintu kosong/null — siap menerima perintah.");
    }
  } else if (code == 404) {
    Serial.println("✓ /kontrol/pintu belum dibuat — siap menerima perintah.");
  } else {
    Serial.printf("⚠ startKontrolStream HTTP %d\n", code);
  }
  Serial.println("✓ Polling /kontrol/pintu tiap 5 dtk (admin override ready).");
}

// =====================
// pollKontrol() — GET /kontrol/pintu.json tiap POLL_INTERVAL_MS
//
// Bila value berubah dan == "UNLOCK", trigger pushAudit + unlockDoor.
// Bukan "stream" sejati (SSE), tapi cukup untuk demo bootcamp.
// =====================
void pollKontrol() {
  if (!firebaseReady) return;
  if (millis() - lastPoll < POLL_INTERVAL_MS) return;
  lastPoll = millis();

  Serial.println("\n---------- [POLL 5s] Cek /kontrol/pintu ----------");

  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  if (!http.begin(sslClient, rtdbUrl("/kontrol/pintu"))) {
    Serial.println("!! [POLL] HTTP begin gagal.");
    return;
  }

  int code = http.GET();
  if (code != 200) {
    if (code == 404) {
      Serial.println(">> [POLL] /kontrol/pintu belum dibuat (404) — tidak ada perintah.");
    } else if (code > 0) {
      Serial.printf("⚠ [POLL] HTTP %d\n", code);
    }
    http.end();
    return;
  }

  String body = http.getString();
  http.end();

  Serial.printf(">> [POLL] Body mentah: %s\n", body.c_str());

  String v;
  if (!parseKontrolValue(body, v)) {
    Serial.println(">> [POLL] Value kosong/null — tidak ada perintah baru.");
    Serial.printf(">> [POLL] lastKontrolValue tetap: '%s'\n", lastKontrolValue.c_str());
    return;
  }

  Serial.printf(">> [POLL] Value sekarang : '%s'\n", v.c_str());
  Serial.printf(">> [POLL] Value sebelumnya: '%s'\n", lastKontrolValue.c_str());

  if (v == lastKontrolValue) {
    Serial.println(">> [POLL] Tidak ada perubahan — skip trigger.");
    return;
  }
  lastKontrolValue = v;

  Serial.printf("<< [POLL] *** PERUBAHAN DETECTED → '%s' ***\n", v.c_str());

  if (v == "UNLOCK") {
    pushAudit("ADMIN_REMOTE");
    unlockDoor("firebase-admin");
  } else if (v == "LOCK") {
    pushAudit("ADMIN_LOCK");
    lockDoor("firebase-admin");
  } else {
    Serial.print("!! [POLL] Perintah tidak dikenal: ");
    Serial.println(v);
  }
}
