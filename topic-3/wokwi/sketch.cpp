/*
 * ============================================================================
 * Bootcamp IoT - Topik 3 (Pertemuan Online #3)
 * DEMO: ESP32 Firebase RTDB ALL-IN-ONE (Sensor + Kontrol Pintu)
 * ============================================================================
 *
 * Sama dengan Topik 2 (MQTT all-in-one), tapi pakai Firebase Realtime Database
 * sebagai pengganti MQTT broker. Hardware & wiring IDENTIK dengan Topik 2.
 *
 * ============================================================================
 * STRUKTUR FILE (modular .h + .cpp)
 * ============================================================================
 *
 *   sketch.ino             ← MAIN ENTRY (file ini). Hanya setup() + loop().
 *   config.h     .cpp      ← Konstanta & globals (WiFi creds, Firebase URL/Key,
 *                            GPIO, DHT object).
 *   firebase_handler.h .cpp← API Firebase + door control: setupWiFi,
 *                            setupFirebase, publishSensor, setStatus, pushAudit,
 *                            startKontrolStream, setupOutputs, unlockDoor.
 *
 *   Dependencies:
 *     sketch.ino  →  config.h
 *                →  firebase_handler.h
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
 *   │   2. setupOutputs()         ← firebase_handler          │
 *   │   3. dht.begin()                                        │
 *   │   4. setupWiFi()            ← firebase_handler          │
 *   │   5. setupFirebase()        ← firebase_handler          │
 *   │   6. startKontrolStream()   ← firebase_handler          │
 *   │   7. setStatus("online")    ← firebase_handler          │
 *   │   8. LED hijau ON                                       │
 *   └─────────────────────────────────────────────────────────┘
 *        │
 *        ▼
 *   ┌─────────────────────────────────────────────────────────┐
 *   │ loop()  ← dijalankan TERUS-MENERUS                      │
 *   │                                                          │
 *   │   ┌── firebaseReady? ──────┐                             │
 *   │   │ TIDAK                  │ YA                           │
 *   │   ▼                        ▼                              │
 *   │  LED hijau OFF         LED hijau ON                       │
 *   │  delay(100)            publishSensor()   ← self-throttled │
 *   │                           (5 detik internal)              │
 *   │                        pollKontrol()      ← self-throttled │
 *   │                           (1.5 detik internal)            │
 *   └─────────────────────────────────────────────────────────┘
 *
 *   EVENT PATH (polling berbasis REST):
 *
 *   pollKontrol() GET /kontrol/pintu.json, bila value baru == "UNLOCK":
 *        → pushAudit("ADMIN_REMOTE")
 *        → unlockDoor("firebase-admin")
 *              ├─ relay HIGH + LED kuning HIGH
 *              ├─ setStatus("UNLOCKED")
 *              ├─ delay(3000)  ← blocking 3 detik (demo)
 *              ├─ relay LOW + LED kuning LOW
 *              └─ setStatus("LOCKED")
 *
 * ============================================================================
 * PATH MAPPING (Topik 2 MQTT → Topik 3 Firebase RTDB):
 * ============================================================================
 *
 *   bootcamp/sensor/01     → /sensor/01          (PUT  = state overwrite)
 *   bootcamp/kontrol/pintu → /kontrol/pintu      (GET poll = deteksi perubahan)
 *   bootcamp/status/pintu  → /status/pintu       (PUT  = retained state)
 *   (MQTT tidak punya)     → /logs/pintu/<auto>  (POST = append audit)
 *
 * ============================================================================
 * Wiring (lihat diagram.json):
 * ============================================================================
 *
 *   DHT22  VCC -> 3V3,  GND -> GND,  SDA -> GPIO 4
 *   Relay module IN              -> GPIO 27 (VCC 5V, GND)
 *   LED "pintu" kuning (R 220)   -> GPIO 26  [indikator pintu terbuka]
 *   LED publish biru   (R 220)   -> GPIO 18  [blink tiap setJSON sensor]
 *   LED status hijau   (R 220)   -> GPIO 12  [on = Firebase ready]
 *
 * ============================================================================
 * SETUP WAJIB SEBELUM MENJALANKAN:
 * ============================================================================
 *
 *   1. Buat project Firebase (console.firebase.google.com).
 *   2. Aktifkan Realtime Database (test mode).
 *   3. Daftarkan Web App → dapatkan firebaseConfig.
 *   4. Edit config.cpp → ganti DATABASE_URL + API_KEY.
 *
 * Catatan keamanan:
 *   - API_KEY BUKAN rahasia → keamanan ada di security rules.
 *   - Test mode = permissive 30 hari → siapa saja dengan URL bisa baca/tulis.
 *   - Untuk produksi: auth + rules + secure element.
 * ============================================================================
 */

#include "config.h"
#include "firebase_handler.h"

// =====================================================
// SETUP — dijalankan SEKALI saat ESP32 boot
// =====================================================
void setup() {
  Serial.begin(115200);
  setupOutputs();                    // init semua GPIO (relay, LED)

  delay(300);
  Serial.println();
  Serial.println("============================================");
  Serial.println("  ESP32 + Firebase RTDB (Sensor + Pintu)");
  Serial.println("============================================");
  Serial.println("  Kontrol pintu via Firebase Console / web.");
  Serial.println("  Set /kontrol/pintu = \"UNLOCK\" untuk membuka.");
  Serial.println();

  dht.begin();                       // init sensor DHT22
  setupWiFi();                       // konek WiFi (blocking ~20s)
  setupFirebase();                   // test konektivitas RTDB via REST GET

  if (firebaseReady) {
    startKontrolStream();            // seed lastKontrolValue (tidak trigger)
    setStatus("online");             // tulis status retained
    digitalWrite(PIN_LED, HIGH);     // LED hijau = Firebase ready

    Serial.println();
    Serial.println("Mulai publish sensor tiap 5 detik...");
  } else {
    Serial.println();
    Serial.println("✗ Firebase belum ready — cek DATABASE_URL & rules.");
  }
  Serial.println();
}

// =====================================================
// LOOP — dijalankan TERUS-MENERUS
// =====================================================
void loop() {
  if (!firebaseReady) {
    digitalWrite(PIN_LED, LOW);      // LED hijau OFF
    delay(100);                      // throttle bila belum ready
    return;
  }

  digitalWrite(PIN_LED, HIGH);       // LED hijau ON = Firebase ready

  // publishSensor() self-throttled (cek millis() internal, tiap 5 detik)
  publishSensor();

  // pollKontrol() self-throttled (cek millis() internal, tiap 1.5 detik)
  // Cek /kontrol/pintu via REST GET; trigger unlockDoor bila == "UNLOCK".
  pollKontrol();
}
