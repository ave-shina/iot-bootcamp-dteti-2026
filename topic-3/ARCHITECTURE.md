# Arsitektur & Flow — Topik 3 (Firebase All-in-One)

> Dokumentasi arsitektur sistem dan alur program untuk project ESP32 Firebase RTDB All-in-One (Sensor DHT22 + Kontrol Pintu via Firebase Console / Node-RED / web dashboard). Hardware identik dengan Topik 2, hanya berbeda layer komunikasi. Topik 3 mengakses Firebase via **REST API** (`HTTPClient` + `ArduinoJson`) — bukan library Mobizt — untuk mempercepat build di Wokwi.

---

## Daftar Isi

1. [Arsitektur Sistem](#1-arsitektur-sistem)
2. [Komponen Hardware](#2-komponen-hardware)
3. [Komponen Software](#3-komponen-software)
4. [Path Firebase RTDB](#4-path-firebase-rtdb)
5. [Alur Program](#5-alur-program)
6. [State Machine Pintu](#6-state-machine-pintu)
7. [Skenario End-to-End](#7-skenario-end-to-end)
8. [Perbandingan dengan Topik 2](#8-perbandingan-dengan-topik-2)
9. [Integrasi Node-RED](#9-integrasi-node-red)

---

## 1. Arsitektur Sistem

### 1.1 Diagram High-Level

```
   ┌──────────────────────────────────────────────────────────────┐
   │                       ESP32 (Wokwi)                          │
   │                       All-in-One                             │
   │                                                              │
   │   DHT22 ───► baca suhu/lembap + jitter random               │
   │              ──► PUT /sensor/01.json tiap 15 detik           │
   │                                                              │
   │   pollKontrol() GET /kontrol/pintu.json tiap 5 dtk           │
   │       │                                                      │
   │       ├─ bila value BERUBAH → "UNLOCK" → unlockDoor()        │
   │       │                       → "LOCK"   → lockDoor()        │
   │       │                                                      │
   │   startKontrolStream() saat boot:                            │
   │       └─ baca awal + restore state (UNLOCK/LOCK)             │
   │                                                              │
   │   PUT /status/pintu.json = "LOCKED"|"UNLOCKED"|"online"      │
   │   POST /logs/pintu.json = {event, ts, rssi}  ── audit trail  │
   │                                                              │
   │   Relay (GPIO 27) ◄── digitalWrite(HIGH/LOW)                 │
   │   LED biru (GPIO 18) ◄── blink tiap PUT sensor               │
   │   LED hijau (GPIO 12) ◄── indikator firebaseReady            │
   │   LED kuning (GPIO 26) ◄── ON steady saat UNLOCKED           │
   └─────────────────────────┬────────────────────────────────────┘
                             │
                             │ WiFi (Wokwi-GUEST)
                             │ HTTPS (TLS via WiFiClientSecure)
                             ▼
   ┌──────────────────────────────────────────────────────────────┐
   │             Firebase Realtime Database (RTDB)                │
   │             console.firebase.google.com                      │
   │                                                              │
   │   • Google-managed, auto-scale                              │
   │   • JSON tree structure                                     │
   │   • REST endpoint: https://<project>.firebaseio.com/<p>.json│
   │   • Real-time sync via WebSocket (dipakai Web SDK, BUKAN ESP)│
   │   • Test mode (permissive 30 hari untuk bootcamp)           │
   │                                                              │
   │   Path yang dipakai:                                         │
   │     /sensor/01      (ESP32 PUT, state overwrite)            │
   │     /kontrol/pintu  (desired state: "UNLOCK"|"LOCK")        │
   │     /status/pintu   (ESP32 PUT, current state, retained)    │
   │     /logs/pintu/*   (ESP32 POST, append audit log)          │
   └─────────────────────────┬────────────────────────────────────┘
                             │
              ┌──────────────┴───────────────┐
              │                              │
              ▼                              ▼
   ┌─────────────────────┐       ┌─────────────────────────────┐
   │  Firebase Console   │       │  Node-RED Dashboard         │
   │  (admin debug)      │       │  (HTTP REST + UI Dashboard) │
   │                     │       │                             │
   │  - Real-time tree   │       │  - Poll sensor 15s          │
   │  - Manual edit data │       │    → gauge + chart          │
   │  - Rules playground │       │  - Poll status 5s           │
   └─────────────────────┘       │  - Button UNLOCK / LOCK     │
                                 │    → PUT /kontrol/pintu     │
                                 │  - Poll audit 10s → table   │
                                 └─────────────────────────────┘
```

### 1.2 Layer Arsitektur

```
   LAYER 4: DASHBOARD          Node-RED Dashboard / Web HTML / Firebase Console
        ▲                          │
        │ HTTPS REST poll          │ (Web SDK: WebSocket + onValue)
        ▼                          │
   LAYER 3: BACKEND               Firebase RTDB (Google Cloud)
        ▲                          │
        │ HTTPS REST (PUT/POST/GET)│
        ▼                          │
   LAYER 2: TRANSPORT             WiFi (Wokwi-GUEST) + WiFiClientSecure
                                   + HTTPClient + ArduinoJson
        ▲                          │
        │                          │
        ▼                          │
   LAYER 1: EDGE DEVICE           ESP32 + DHT22 + relay + 3 LED
                                   (firmware C/C++ via Wokwi)
```

### 1.3 Pilihan Teknologi: REST API vs Mobizt Library

| Aspek                | REST API (yang dipakai)             | Mobizt (`Firebase_ESP_Client`)     |
| -------------------- | ----------------------------------- | ---------------------------------- |
| Sumber library       | ESP32 core + ArduinoJson            | Library eksternal ~10 MB source    |
| Build time Wokwi     | ~30–45 detik                        | 3–5 menit                          |
| Cakupan fitur        | Cukup RTDB (PUT/POST/GET)           | Semua layanan Firebase             |
| Komunikasi ke RTDB   | HTTP request explicit               | Abstraksi high-level + WebSocket   |
| Real-time push web   | Native via JS SDK (terpisah dari ESP) | Native via library               |
| Cocok untuk         | Bootcamp, IoT ringan, cepat iterasi  | Produksi kompleks butuh fitur lain |

---

## 2. Komponen Hardware

| Komponen                | Pin ESP32                | Fungsi                            | Kategori     |
| ----------------------- | ------------------------ | --------------------------------- | ------------ |
| DHT22                   | VCC 3V3 / GND / SDA **GPIO 4** | Sensor suhu + kelembapan     | Input        |
| Relay module            | **GPIO 27** (VCC 5V, GND) | Simulasi solenoid door lock       | Output       |
| LED "pintu" (kuning)    | **GPIO 26** (R220)       | ON steady saat UNLOCKED            | Output       |
| LED publish (biru)      | **GPIO 18** (R220)       | Blink tiap PUT sensor             | Output       |
| LED status (hijau)      | **GPIO 12** (R220)       | Indikator `firebaseReady == true` | Output       |

**Total GPIO dipakai**: 5 pin (1 DHT + 1 relay + 3 LED)

---

## 3. Komponen Software

### 3.1 Struktur File Modular

```
topic-3/
├── wokwi/
│   ├── sketch.ino             ← MAIN: setup() + loop() saja
│   ├── config.h     .cpp      ← Konstanta & globals (WiFi, DATABASE_URL, GPIO, sslClient)
│   ├── firebase_handler.h .cpp← REST API Firebase + door control (setup, publish,
│   │                            pollKontrol, setStatus, pushAudit, unlockDoor, lockDoor)
│   ├── diagram.json            ← Wokwi wiring
│   ├── libraries.txt           ← ArduinoJson + DHT sensor library
│   └── README.md               ← Dokumentasi penggunaan
└── nodered/
    ├── flow.json               ← Flow Node-RED lengkap (hardcoded URL)
    └── flow-minimal.json       ← Flow minimal untuk debug/troubleshooting
```

### 3.2 Library Yang Dipakai

| Library                | Author     | Fungsi                                | Sumber           |
| ---------------------- | ---------- | ------------------------------------- | ---------------- |
| `WiFi.h`               | ESP32 core | WiFi stack (STA mode)                 | bawaan core      |
| `WiFiClientSecure.h`   | ESP32 core | Koneksi TCP terenkripsi TLS           | bawaan core      |
| `HTTPClient.h`         | ESP32 core | HTTP request (GET/PUT/POST)           | bawaan core      |
| `ArduinoJson.h`        | bblanchon  | Serialize/parse JSON (StaticJsonDoc)  | libraries.txt    |
| `DHT.h`                | Adafruit   | Driver sensor DHT22                   | libraries.txt    |

> Tidak ada library Firebase eksternal. Semua akses RTDB dibangun dari HTTP primitif.

### 3.3 Dependency Graph Antar-Modul

```
   sketch.ino
      │
      ├──► config.h
      │
      └──► firebase_handler.h ──► config.h
```

**Catatan**: Hanya 2 modul — `config` (konstanta/globals/shared state) dan `firebase_handler` (semua logika REST Firebase + door control). Struktur identik Topik 2, hanya isi handler yang berbeda.

### 3.4 Shared State antar Modul

Dideklarasikan `extern` di `config.h`, didefinisikan di `config.cpp` / `firebase_handler.cpp`:

| Variabel             | Tipe              | Tujuan                                         |
| -------------------- | ----------------- | ---------------------------------------------- |
| `sslClient`          | `WiFiClientSecure`| Saluran TLS tunggal, dipakai semua HTTP request|
| `firebaseReady`      | `bool`            | Flag "konektivitas RTDB sudah diverifikasi"   |
| `lastPublish`        | `unsigned long`   | Timestamp `millis()` publish sensor terakhir   |
| `lastPoll`           | `unsigned long`   | Timestamp `millis()` poll kontrol terakhir     |
| `lastKontrolValue`   | `String`          | Value terakhir `/kontrol/pintu` (deteksi change)|
| `dht`                | `DHT`             | Objek sensor DHT22                             |

---

## 4. Path Firebase RTDB

### 4.1 Struktur JSON Tree

```
   <project-root>/
   ├── sensor/
   │   └── 01/                 ──► ESP32 PUT tiap 15 detik (state overwrite + jitter)
   │       ├── id:    "sensor_01"
   │       ├── suhu:   28.5    (raw + random jitter ±2°C, clamped 20-40)
   │       ├── lembap: 72.0    (raw + random jitter ±5%,  clamped 30-90)
   │       ├── ts:     4521
   │       └── rssi:   -58
   ├── status/
   │   └── pintu               ──► ESP32 PUT (current state: LOCKED | UNLOCKED | online)
   ├── kontrol/
   │   └── pintu               ──► Desired state (Console/Node-RED set "UNLOCK"|"LOCK")
   └── logs/
       └── pintu/              ──► ESP32 POST (append audit log, auto-ID)
           ├── -NXYZ123: { event:"ADMIN_REMOTE",        ts:4521, rssi:-58 }
           ├── -NXYZ124: { event:"ADMIN_LOCK",          ts:4530, rssi:-58 }
           └── ...
```

### 4.2 Operasi per Path (REST Mapping)

| Path                | HTTP Method         | URL                                            | Arah               | Setara MQTT         |
| ------------------- | ------------------- | ---------------------------------------------- | ------------------ | ------------------- |
| `/sensor/01`        | `PUT`               | `https://<proj>.firebaseio.com/sensor/01.json` | ESP → RTDB         | publish (retain)    |
| `/kontrol/pintu`    | `GET` (poll 5 dtk)  | `https://<proj>.firebaseio.com/kontrol/pintu.json` | RTDB → ESP (poll) | subscribe           |
| `/status/pintu`     | `PUT`               | `https://<proj>.firebaseio.com/status/pintu.json` | ESP → RTDB       | publish retained    |
| `/logs/pintu/*`     | `POST`              | `https://<proj>.firebaseio.com/logs/pintu.json` | ESP → RTDB (append)| (tidak ada di MQTT) |

### 4.3 Konsep REST Firebase Penting

- **PUT vs POST**: `PUT` = overwrite path (state terakhir menang). `POST` = append auto-ID (historis, response berisi `{"name":"<auto-id>"}`).
- **Test mode**: rules publik → URL tidak butuh `?auth=...`. Produksi: tambahkan auth param atau pakai SDK ber-auth.
- **`.json` suffix**: wajib di setiap endpoint REST Firebase RTDB — tanpa ini response 404.
- **Polling vs Stream**: ESP32 memakai polling 5 dtk (hemat memori, latensi acceptable). Web dashboard pakai WebSocket native via JS SDK.
- **Security Rules**: permissive di Test mode (30 hari). Produksi wajib auth + rules per path.

---

## 5. Alur Program

### 5.1 Boot Flow (dijalankan SEKALI saat power-on)

```
   POWER ON
      │
      ▼
   setup()
      │
      ├─ 1. Serial.begin(115200)
      ├─ 2. setupOutputs()             ← firebase_handler (pinMode semua GPIO)
      ├─ 3. dht.begin()                ← init sensor DHT22
      ├─ 4. setupWiFi()                ← firebase_handler (blocking, ~20s timeout)
      │      └─ WiFi STA mode → connect Wokwi-GUEST
      ├─ 5. setupFirebase()            ← firebase_handler
      │      ├─ cek placeholder DATABASE_URL ("YOUR-PROJECT")
      │      ├─ sslClient.setInsecure()         ← skip TLS cert validation
      │      ├─ HTTP GET /status/pintu.json     ← test konektivitas
      │      └─ bila HTTP code 0..499 → firebaseReady = true
      ├─ 6. startKontrolStream()       ← baca awal + STATE RESTORATION
      │      ├─ GET /kontrol/pintu.json
      │      ├─ simpan sebagai lastKontrolValue
      │      └─ bila "UNLOCK" → unlockDoor("firebase-boot-restore")
      │         bila "LOCK"   → lockDoor("firebase-boot-restore")
      ├─ 7. setStatus("online")        ← PUT /status/pintu.json
      └─ 8. LED hijau ON
      │
      ▼
   loop() (RESTART dari sini bila reboot)
```

> **Penting**: Saat boot, ESP32 menyinkronkan state pintu dengan nilai `/kontrol/pintu`. Bila Firebase menyimpan "UNLOCK" (dari sesi sebelumnya), pintu akan langsung terbuka. Ini memastikan konsistensi state lintas reboot.

### 5.2 Main Loop Flow (dijalankan TERUS-MENERUS)

```
   loop()
      │
      ├─ firebaseReady ?
      │     │
      │     ├─ TIDAK ──► LED hijau OFF
      │     │            delay(100)   (throttle bila belum ready)
      │     │            return
      │     │
      │     └─ YA ──► LED hijau ON
      │               publishSensor()    (self-throttled 15 detik)
      │                 ├─ dht.read() + jitter random ±2°C / ±5%
      │                 ├─ StaticJsonDocument build
      │                 ├─ http.PUT /sensor/01.json
      │                 └─ LED biru blink (HIGH 30ms → LOW)
      │
      │               pollKontrol()     (self-throttled 5 detik)
      │                 ├─ Serial "[POLL 5s] Cek /kontrol/pintu"
      │                 ├─ http.GET /kontrol/pintu.json
      │                 ├─ parseKontrolValue() + log body mentah
      │                 ├─ compare dengan lastKontrolValue
      │                 └─ bila BERUBAH:
      │                       ├─ "UNLOCK" → pushAudit + unlockDoor
      │                       └─ "LOCK"   → pushAudit + lockDoor
      │
      └─ (loop kembali ke atas)
```

### 5.3 Event Flow: Admin Trigger UNLOCK

```
   Operator set /kontrol/pintu = "UNLOCK" (via Console atau Node-RED button)
      │
      ▼
   Firebase RTDB simpan value + broadcast ke subscriber (Web SDK) via WebSocket
      │
      ▼
   ESP32 pollKontrol() GET /kontrol/pintu.json (paling lambat 5 dtk kemudian)
      │
      ├─ Serial: "---------- [POLL 5s] Cek /kontrol/pintu ----------"
      ├─ Serial: ">> [POLL] Body mentah: \"UNLOCK\""
      ├─ parseKontrolValue(body, v) → v = "UNLOCK"
      ├─ compare v dengan lastKontrolValue
      └─ bila berubah:
            │
            └─ v == "UNLOCK"?
                  │
                  └─ YA ──► pushAudit("ADMIN_REMOTE")
                           │     └─ POST /logs/pintu.json
                           │        body: { event, ts, rssi }
                           │        resp: { "name": "-NXYZ123" }
                           │
                           └─ unlockDoor("firebase-admin")
                                 ├─ relay HIGH + LED kuning HIGH (steady)
                                 ├─ setStatus("UNLOCKED")   ← PUT
                                 └─ Pintu TETAP terbuka (no auto-lock)
```

> Pintu hanya akan tertutup kembali bila operator mengirim perintah `"LOCK"`.

### 5.4 Event Flow: Admin Trigger LOCK

```
   Operator set /kontrol/pintu = "LOCK"
      │
      ▼
   ESP32 pollKontrol() GET (≤5 dtk kemudian)
      │
      └─ v == "LOCK" (berubah dari "UNLOCK")
            │
            └─► pushAudit("ADMIN_LOCK")    ← POST /logs/pintu.json
                lockDoor("firebase-admin")
                   ├─ relay LOW + LED kuning LOW
                   └─ setStatus("LOCKED")  ← PUT /status/pintu.json
```

### 5.5 Data Flow: Sensor Publish (15 detik)

```
   Timer millis() - lastPublish >= 15000 ms
      │
      ▼
   publishSensor() [di firebase_handler.cpp]
      │
      ├─ 1. rawSuhu   = dht.readTemperature()  ──► 25.0 °C (Wokwi default konstan)
      ├─ 2. rawLembap = dht.readHumidity()     ──► 50.0 %
      │
      ├─ 3. Tambah jitter random (simulasi sensor realistis):
      │      suhu   = rawSuhu   + random(-200, 301) / 100.0   → ±2 °C
      │      lembap = rawLembap + random(-500, 501) / 100.0   → ±5 %
      │
      ├─ 4. Clamp range realistis:
      │      suhu   = constrain(suhu,   20.0, 40.0)
      │      lembap = constrain(lembap, 30.0, 90.0)
      │
      ├─ 5. Build StaticJsonDocument<128>:
      │      {
      │        "id":    "sensor_01",
      │        "suhu":   26.3,    (dibulatkan 1 desimal)
      │        "lembap": 52.8,
      │        "ts":     4521,
      │        "rssi":   -58
      │      }
      │
      ├─ 6. serializeJson(doc, body)
      │
      ├─ 7. http.begin(sslClient, rtdbUrl("/sensor/01"))
      │      http.addHeader("Content-Type", "application/json")
      │      http.PUT(body)         ← overwrite state
      │
      ├─ 8. LED biru blink (HIGH 30ms → LOW)
      │
      └─ 9. Serial log: ">> Write /sensor/01 OK (suhu=26.3 lembap=52.8)"
```

### 5.6 Data Flow: Audit Log (POST = append)

```
   Event terjadi (unlockDoor / lockDoor dipanggil)
      │
      ▼
   pushAudit(eventName)
      │
      ├─ Build StaticJsonDocument<128>:
      │      {
      │        "event": "ADMIN_REMOTE" | "ADMIN_LOCK" | ...,
      │        "ts":    4521,
      │        "rssi":  -58
      │      }
      │
      ├─ http.begin(sslClient, rtdbUrl("/logs/pintu"))
      │  http.addHeader("Content-Type", "application/json")
      │  http.POST(body)
      │   └─ Response: { "name": "-NXYZ123" }   ← Firebase auto-generate key
      │   └─ Append, tidak overwrite history
      │
      └─ Console tampil:
        /logs/pintu/
        ├── -NXYZ123: { event:"ADMIN_REMOTE", ts:4521, rssi:-58 }   (saat UNLOCK)
        ├── -NXYZ124: { event:"ADMIN_LOCK",   ts:8421, rssi:-57 }   (saat LOCK)
        └── ...
```

### 5.7 Self-Throttling Pattern

Kedua task periodik (`publishSensor`, `pollKontrol`) memakai pola non-blocking berbasis `millis()`:

```cpp
if (millis() - lastPublish < PUBLISH_INTERVAL_MS) return;
lastPublish = millis();
// ... do work
```

**Keuntungan**: `loop()` tetap responsif, tidak ada `delay()` jangka panjang yang mem-block eksekusi lain. Setelah perubahan ke state machine persisten (opsi C), `delay(3000)` di `unlockDoor()` sudah dihapus — semua operasi pintu non-blocking.

---

## 6. State Machine Pintu

### 6.1 Desain: Persistent State (Opsi C)

Dipilih desain **persistent LOCKED ⇄ UNLOCKED** tanpa auto-lock. Pintu tetap di state terakhir sampai ada perintah eksplisit baru. State di Firebase bertindak sebagai **desired state** (source of truth), dan ESP32 menyinkronkan diri.

### 6.2 State Diagram

```
   ┌────────────────────────────────┐
   │          LOCKED                │ ◄── default state saat boot
   │   relay LOW (solenoid lepas)   │     (kalau /kontrol/pintu kosong)
   │   LED kuning OFF               │
   └───────────────┬────────────────┘
                   │
                   │ trigger: pollKontrol baca /kontrol/pintu == "UNLOCK"
                   │        (atau saat boot: startKontrolStream restore)
                   │        (value harus BERUBAH dari lastKontrolValue)
                   │
                   ▼  unlockDoor(source)
                      ├─ pushAudit("ADMIN_REMOTE")  ← POST /logs/pintu
                      ├─ relay HIGH, LED kuning ON
                      └─ setStatus("UNLOCKED")      ← PUT /status/pintu
   ┌────────────────────────────────┐
   │          UNLOCKED              │
   │   relay HIGH (solenoid aktif)  │ ── TETAP di state ini tanpa batas waktu
   │   LED kuning ON steady         │ ── Tidak ada auto-lock
   └───────────────┬────────────────┘
                   │
                   │ trigger: pollKontrol baca /kontrol/pintu == "LOCK"
                   │        (atau saat boot: startKontrolStream restore)
                   │        (value harus BERUBAH dari lastKontrolValue)
                   │
                   ▼  lockDoor(source)
                      ├─ pushAudit("ADMIN_LOCK")    ← POST /logs/pintu
                      ├─ relay LOW, LED kuning OFF
                      └─ setStatus("LOCKED")        ← PUT /status/pintu
   (kembali ke LOCKED)
```

### 6.3 Audit Event Sequence

```
   Trigger via Firebase Console / Node-RED / web:

   UNLOCK command:
     1. pushAudit("ADMIN_REMOTE")   ── POST /logs/pintu/<auto-id-1>
     2. setStatus("UNLOCKED")       ── PUT /status/pintu = "UNLOCKED"
     (pintu tetap terbuka)

   LOCK command:
     1. pushAudit("ADMIN_LOCK")     ── POST /logs/pintu/<auto-id-2>
     2. setStatus("LOCKED")         ── PUT /status/pintu = "LOCKED"
     (pintu tetap tertutup)
```

### 6.4 Edge Case: Polling Tidak Re-trigger

`pollKontrol()` menyimpan `lastKontrolValue` setiap pembacaan. Bila value sama dengan sebelumnya → skip. Ini mencegah re-trigger berulang saat nilai tidak berubah.

**Dampak dengan state machine persisten**:
- UNLOCK → LOCK → UNLOCK → LOCK → ... (alternasi bebas, semua trigger)
- UNLOCK → UNLOCK (tidak ada perubahan) → skip, tidak re-trigger
- Untuk UNLOCK dua kali berturut-turut, sisipkan LOCK di antaranya

### 6.5 Edge Case: Boot State Restoration

Saat ESP32 reboot, `startKontrolStream()` membaca nilai `/kontrol/pintu` dan **menyinkronkan state pintu** dengan nilai tersebut:

| Nilai `/kontrol/pintu` saat boot | Aksi ESP32 |
|---|---|
| `"UNLOCK"` | `unlockDoor("firebase-boot-restore")` → pintu terbuka |
| `"LOCK"` | `lockDoor("firebase-boot-restore")` → pintu tertutup |
| `null` / kosong / 404 | Default LOCKED (tidak ada aksi) |
| Lainnya (mis. `"FOO"`) | Default LOCKED, log warning |

Ini memastikan **state survive power loss** — operator tidak perlu re-trigger command setelah ESP32 reboot.

---

## 7. Skenario End-to-End

### 7.1 Demo Lengkap (urutan rekomendasi)

| Step | Aksi                                          | Expected Result                                          |
| ---- | --------------------------------------------- | -------------------------------------------------------- |
| 1    | Setup Firebase project + update `config.cpp`  | Dapat DATABASE_URL + API_KEY                             |
| 2    | Start Wokwi simulation                        | Serial: "Firebase reachable (HTTP 200)" + LED hijau ON   |
| 3    | Buka Firebase Console → RTDB → Data           | Node `/sensor/01` muncul, update tiap 15 detik dengan variasi |
| 4    | Console: set `/kontrol/pintu = "UNLOCK"`      | ≤5 dtk: relay + LED kuning ON; audit `ADMIN_REMOTE`; status `UNLOCKED` |
| 5    | Tunggu beberapa menit                          | Pintu **TETAP terbuka** (tidak auto-lock)                |
| 6    | Console: set `/kontrol/pintu = "LOCK"`        | ≤5 dtk: relay + LED kuning OFF; audit `ADMIN_LOCK`; status `LOCKED` |
| 7    | Lihat `/logs/pintu/`                          | Audit log muncul dengan auto-ID unik                     |
| 8    | Reboot ESP32 (restart Wokwi)                   | State dipulihkan sesuai `/kontrol/pintu` terakhir        |

### 7.2 Skenario Failure Modes

| Skenario                | Behavior ESP32                                                |
| ----------------------- | ------------------------------------------------------------- |
| WiFi putus              | `firebaseReady` tidak di-reset; ESP32 core auto-reconnect WiFi|
| `DATABASE_URL` placeholder | `setupFirebase()` deteksi "YOUR-PROJECT" → `firebaseReady = false` |
| API_KEY salah           | Tidak berdampak di test mode (tidak dipakai)                  |
| Security rules ketat    | HTTP 401/403 → log error, `firebaseReady` tetap true          |
| Path belum ada (404)    | `pollKontrol`/`startKontrolStream` skip silent — wajar         |
| DHT22 baca NaN          | `publishSensor()` skip cycle (tidak crash)                    |
| Database URL salah      | HTTP GET gagal → `firebaseReady = false`                      |
| `ssl/TLS` handshake fail| `setInsecure()` skip cert — jarang gagal kecuali DNS          |

---

## 8. Perbandingan dengan Topik 2

| Aspek                    | Topik 2 (MQTT)                       | Topik 3 (Firebase RTDB REST)               |
| ------------------------ | ------------------------------------ | ------------------------------------------ |
| Backend                  | `test.mosquitto.org:1883`            | Firebase Console (Google)                  |
| Auth                     | ❌ Anonim (broker publik)             | API Key + Security Rules (opsional test mode) |
| Library komunikasi       | `PubSubClient`                       | `HTTPClient` + `ArduinoJson` (ESP32 core)  |
| Subscribe                | `client.subscribe(topic)` + callback | `GET` poll `/kontrol/pintu.json` tiap 5 dtk |
| Publish state            | `client.publish(topic, msg)`         | `http.PUT(url, body)`                      |
| Audit log (append)       | ❌ Tidak native                       | ✅ `http.POST` auto-generate ID            |
| Retained state           | ✅ Flag retain di publish             | ✅ Default behavior (state disimpan)       |
| LWT (offline detect)     | ✅ Native MQTT feature                | ⚠️ Butuh Cloud Function                    |
| Realtime push ke web     | MQTT.js + WebSocket                   | ✅ Native via Firebase JS SDK              |
| Setup awal               | Langsung jalan (no config)            | Perlu daftar project + dapatkan DATABASE_URL|
| Build time Wokwi         | ~30 detik                             | ~30–45 detik (sama, tanpa library berat)   |
| State machine pintu      | **Persistent LOCKED ⇄ UNLOCKED** (opsi C) | **Persistent LOCKED ⇄ UNLOCKED** (opsi C) |
| Auto-lock                | ❌ Dihapus                            | ❌ Dihapus                                  |
| Boot behavior            | Retained message → callback → restore | `startKontrolStream()` → restore           |
| Hardware                 | **IDENTIK** (DHT22 + relay + 3 LED)   |                                            |
| GPIO pin                 | **IDENTIK** (4, 27, 26, 18, 12)       |                                            |
| Struktur file modular    | **IDENTIK** (config + handler)        |                                            |

> **Insight**: Hardware dan logic bisnis (door control) SAMA, termasuk state machine persistent. Yang berbeda hanya layer komunikasi. Modular code memungkinkan swap MQTT ↔ Firebase REST tanpa ubah logic bisnis.

---

## 9. Integrasi Node-RED

### 9.1 Arsitektur Node-RED

Node-RED berperan sebagai **dashboard & control panel** alternatif Firebase Console. Komunikasi via REST API ke Firebase (sama seperti ESP32, bukan subscribe realtime).

```
   ┌─────────────────────────────────────────────┐
   │           Node-RED Flow                     │
   │                                             │
   │  [Inject 15s] → GET /sensor/01.json         │
   │                    ↓                        │
   │              [Split suhu & lembap]          │
   │                    ↓                        │
   │   [Gauge Suhu]  [Gauge Lembap]  [Chart]     │
   │                                             │
   │  [Inject 5s] → GET /status/pintu.json       │
   │                    ↓                        │
   │              [Text Status]                  │
   │                                             │
   │  [Inject 5s] → GET /kontrol/pintu.json      │
   │                    ↓                        │
   │              [Text Kontrol]                 │
   │                                             │
   │  [Button UNLOCK] → PUT /kontrol/pintu       │
   │                    = "UNLOCK"               │
   │                                             │
   │  [Button LOCK]   → PUT /kontrol/pintu       │
   │                    = "LOCK"                 │
   │                                             │
   │  [Inject 10s] → GET /logs/pintu.json        │
   │                    ↓                        │
   │           [Function: obj → array]           │
   │                    ↓                        │
   │           [Template: HTML Table]            │
   └─────────────────────────────────────────────┘
```

### 9.2 File Flow

| File | Isi | Use Case |
|---|---|---|
| `nodered/flow.json` | Flow lengkap dengan semua polling, gauge, chart, button, audit table | Production demo |
| `nodered/flow-minimal.json` | Flow minimal (1 inject + 1 HTTP + 1 gauge + 1 debug) | Troubleshooting awal |

### 9.3 Prasyarat

- **node-red-dashboard** (Palette Manager → install) — untuk widget UI (gauge, chart, button, text, template)

### 9.4 Cara Pakai

1. Import `flow.json` ke Node-RED
2. Deploy
3. Akses dashboard di `http://localhost:1880/ui`
4. Tunggu polling otomatis:
   - Sensor: ≤15 detik
   - Status pintu: ≤5 detik
   - Audit log: ≤10 detik
5. Click button UNLOCK / LOCK untuk kontrol

### 9.5 Ganti URL Firebase

URL Firebase di-hardcode di 6 HTTP request node. Untuk ganti ke project lain, edit URL di:

1. `GET /sensor/01.json`
2. `GET /status/pintu.json`
3. `GET /kontrol/pintu.json`
4. `PUT /kontrol/pintu = UNLOCK`
5. `PUT /kontrol/pintu = LOCK`
6. `GET /logs/pintu.json`

---

## Lampiran: Referensi Cepat

### A. Konstanta Penting (`config.cpp`)

```cpp
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

const char* DATABASE_URL = "https://YOUR-PROJECT-default-rtdb.firebaseio.com/";
const char* API_KEY      = "YOUR-API-KEY";   // tidak dipakai di test mode

const unsigned long PUBLISH_INTERVAL_MS = 15000UL;  // 15 detik — publish sensor
const unsigned long POLL_INTERVAL_MS    = 5000UL;   // 5 detik — poll admin override
```

### B. Pseudo-code Loop Utama

```python
while True:
    if not firebase_ready:
        led_hijau.off()
        sleep(0.1)
        continue

    led_hijau.on()
    if elapsed(last_publish) >= 15s:
        publish_sensor()     # PUT /sensor/01.json (dengan jitter random)

    if elapsed(last_poll) >= 5s:
        poll_kontrol()       # GET /kontrol/pintu.json
        # bila BERUBAH:
        #   "UNLOCK" → unlock_door()  (pintu terbuka persisten)
        #   "LOCK"   → lock_door()    (pintu tertutup persisten)
```

### C. REST Endpoint Pattern

```
   Base URL:  https://<project-id>-default-rtdb.firebaseio.com
   Suffix:    .json  (wajib di setiap path)

   PUT  /sensor/01.json       → timpa state sensor
   PUT  /status/pintu.json    → current state ("LOCKED"|"UNLOCKED"|"online")
   POST /logs/pintu.json      → append audit, response {"name":"<auto-id>"}
   GET  /kontrol/pintu.json   → baca desired state (poll tiap 5 dtk)
   PUT  /kontrol/pintu.json   → set desired state ("UNLOCK"|"LOCK") — dari admin
```

### D. Decision Tree Akses Pintu

```
   pollKontrol() GET /kontrol/pintu.json (tiap 5 dtk)
      │
      ├─ body == "null" / 404 ──► skip (path belum ada)
      ├─ value == lastKontrolValue ──► skip (tidak berubah)
      │
      └─ value BERUBAH
            │
            ├─ "UNLOCK" ──► pushAudit("ADMIN_REMOTE")
            │              unlockDoor("firebase-admin")
            │                 ├─ relay HIGH + LED kuning HIGH (steady)
            │                 └─ setStatus("UNLOCKED")
            │                 (pintu TETAP terbuka)
            │
            └─ "LOCK" ────► pushAudit("ADMIN_LOCK")
                           lockDoor("firebase-admin")
                              ├─ relay LOW + LED kuning LOW
                              └─ setStatus("LOCKED")
                              (pintu TETAP tertutup)
```

### E. Boot Restoration Logic

```
   startKontrolStream() saat setup()
      │
      ├─ GET /kontrol/pintu.json
      │
      └─ bila value terdeteksi:
            │
            ├─ "UNLOCK" ──► unlockDoor("firebase-boot-restore")
            │                 (pintu langsung terbuka saat boot)
            │
            ├─ "LOCK" ────► lockDoor("firebase-boot-restore")
            │                 (state konsisten tertutup)
            │
            └─ lainnya ──► default LOCKED, log warning
```
