# Arsitektur & Flow — Topik 2 (MQTT All-in-One)

> Dokumentasi arsitektur sistem dan alur program untuk project ESP32 MQTT All-in-One (Sensor DHT22 + Kontrol Pintu via dashboard Node-RED).

---

## Daftar Isi

1. [Arsitektur Sistem](#1-arsitektur-sistem)
2. [Komponen Hardware](#2-komponen-hardware)
3. [Komponen Software](#3-komponen-software)
4. [Topik MQTT](#4-topik-mqtt)
5. [Alur Program](#5-alur-program)
6. [State Machine Pintu](#6-state-machine-pintu)
7. [Skenario End-to-End](#7-skenario-end-to-end)
8. [Perbandingan dengan Topik 3](#8-perbandingan-dengan-topik-3)

---

## 1. Arsitektur Sistem

### 1.1 Diagram High-Level

```
   ┌──────────────────────────────────────────────────────────────┐
   │                       ESP32 (Wokwi)                          │
   │                       All-in-One                             │
   │                                                              │
   │   DHT22 ───►  baca suhu/lembap ───► publish sensor (JSON)    │
   │                                                              │
   │   PubSubClient (MQTT client) ──► subscribe kontrol/pintu     │
   │       │                          publish status/pintu        │
   │       │                                                      │
   │   Relay (GPIO 27) ◄── digitalWrite(HIGH/LOW)                 │
   │   LED biru (GPIO 18) ◄── blink tiap publish                 │
   │   LED hijau (GPIO 12) ◄── indikator MQTT connected           │
   │   LED kuning (GPIO 26) ◄── indikator pintu terbuka          │
   └─────────────────────────┬────────────────────────────────────┘
                             │
                             │ WiFi (Wokwi-GUEST)
                             │ MQTT over TCP (port 1883, plaintext)
                             ▼
   ┌──────────────────────────────────────────────────────────────┐
   │              test.mosquitto.org (broker publik)              │
   │                                                              │
   │   • Eclipse Foundation managed                              │
   │   • No auth, no TLS                                         │
   │   • Multi-client, retained message support                  │
   │   • Topic-based routing                                      │
   │                                                              │
   │   Topik yang dipakai:                                        │
   │     bootcamp/sensor/01    (ESP32 publish)                    │
   │     bootcamp/kontrol/pintu (ESP32 subscribe, dashboard pub)  │
   │     bootcamp/status/pintu (ESP32 publish, retained)          │
   └─────────────────────────┬────────────────────────────────────┘
                             │
              ┌──────────────┴───────────────┐
              │                              │
              ▼                              ▼
   ┌─────────────────────┐       ┌─────────────────────┐
   │  MQTT Explorer      │       │  Node-RED Dashboard │
   │  (debug tool)       │       │  localhost:1880/ui  │
   │                     │       │                     │
   │  - Lihat raw JSON   │       │  - Gauge suhu       │
   │  - Manual publish   │       │  - Chart historis   │
   │  - Subscribe #      │       │  - Button admin     │
   └─────────────────────┘       │  - Audit log text   │
                                 └─────────────────────┘
```

### 1.2 Layer Arsitektur

```
   LAYER 4: DASHBOARD          Node-RED (visual flow, JS-based)
        ▲                          │
        │ MQTT pub/sub             │
        ▼                          │
   LAYER 3: BROKER                test.mosquitto.org (Mosquitto)
        ▲                          │
        │ TCP 1883                 │
        ▼                          │
   LAYER 2: TRANSPORT             WiFi (Wokwi-GUEST) + PubSubClient
        ▲                          │
        │                          │
        ▼                          │
   LAYER 1: EDGE DEVICE           ESP32 + DHT22 + relay + 3 LED
                                   (firmware C/C++ via Wokwi)
```

---

## 2. Komponen Hardware

| Komponen                | Pin ESP32                | Fungsi                            | Kategori     |
| ----------------------- | ------------------------ | --------------------------------- | ------------ |
| DHT22                   | VCC 3V3 / GND / SDA **GPIO 4** | Sensor suhu + kelembapan     | Input        |
| Relay module            | **GPIO 27** (VCC 5V, GND) | Simulasi solenoid door lock       | Output       |
| LED "pintu" (kuning)    | **GPIO 26** (R220)       | Indikator pintu terbuka           | Output       |
| LED publish (biru)      | **GPIO 18** (R220)       | Blink tiap publish sensor         | Output       |
| LED status (hijau)      | **GPIO 12** (R220)       | Indikator MQTT connected          | Output       |

**Total GPIO dipakai**: 5 pin (1 DHT + 1 relay + 3 LED)

---

## 3. Komponen Software

### 3.1 Struktur File Modular

```
topic-2/wokwi/
├── sketch.c               ← MAIN: setup() + loop() saja
├── config.h     .cpp      ← Konstanta & globals (WiFi, broker, topics, GPIO)
├── mqtt_handler.h .cpp    ← API MQTT + door control (setup, reconnect,
│                            publish, onMessage, setupOutputs, unlockDoor)
├── diagram.json            ← Wokwi wiring
├── libraries.txt           ← PubSubClient + ArduinoJson + DHT
└── README.md               ← Dokumentasi penggunaan
```

### 3.2 Library Yang Dipakai

| Library                | Author     | Fungsi                              | Flash Footprint |
| ---------------------- | ---------- | ----------------------------------- | --------------- |
| `WiFi.h`               | ESP32 core | WiFi stack                          | bawaan          |
| `PubSubClient.h`       | Knolleary  | MQTT client (publish + subscribe)   | ~5 KB           |
| `ArduinoJson.h`        | bblanchon  | Serialize/deserialize JSON payload  | ~10 KB          |
| `DHT.h`                | Adafruit   | Driver sensor DHT22                 | ~5 KB           |

### 3.3 Dependency Graph Antar-Modul

```
   sketch.c
      │
      ├──► config.h
      │
      └──► mqtt_handler.h ──► config.h
```

**Catatan**: Setelah keypad dihapus, hanya ada 2 modul: `config` (konstanta/globals) dan `mqtt_handler` (semua logika MQTT + door control). Struktur lebih ramping.

---

## 4. Topik MQTT

### 4.1 Struktur 3 Topik

```
   bootcamp/
   ├── sensor/
   │   └── 01              ──► ESP32 publish JSON DHT22 (QoS 0, retain=false)
   ├── kontrol/
   │   └── pintu           ──► Dashboard publish "UNLOCK" (QoS 1, retain=false)
   │                            ESP32 subscribe
   └── status/
       └── pintu           ──► ESP32 publish audit/state (QoS 1, retain=true)
```

### 4.2 Payload per Topik

| Topik                     | Arah             | Payload                                                                 | Retain | QoS |
| ------------------------- | ---------------- | ----------------------------------------------------------------------- | ------ | --- |
| `bootcamp/sensor/01`      | ESP → broker     | `{"id":"sensor_01","suhu":28.5,"lembap":72.0,"ts":4521,"rssi":-58}`     | ❌      | 0   |
| `bootcamp/kontrol/pintu`  | broker → ESP     | `"UNLOCK"` (dari dashboard)                                             | ❌      | 1   |
| `bootcamp/status/pintu`   | ESP → broker     | `online` / `offline` (LWT) / `UNLOCKED` / `LOCKED` / `ADMIN_REMOTE`     | ✅      | 1   |

### 4.3 Konsep MQTT Penting

- **Retained message**: broker simpan pesan terakhir. Subscriber baru langsung dapat state terakhir.
- **LWT (Last Will Testament)**: bila ESP disconnect mendadak, broker publish "offline" ke status topic otomatis.
- **QoS 0** (sensor): fire-and-forget, cepat, boleh hilang.
- **QoS 1** (kontrol & status): guaranteed delivery, minimal sampai 1x.

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
      ├─ 2. setupOutputs()             ← mqtt_handler (pinMode semua GPIO)
      ├─ 3. dht.begin()                ← init sensor DHT22
      ├─ 4. setupWiFi()                ← mqtt_handler (blocking, ~20s timeout)
      │      └─ WiFi STA mode → connect Wokwi-GUEST
      ├─ 5. setupMQTT()                ← mqtt_handler
      │      ├─ client.setServer(broker, 1883)
      │      ├─ client.setKeepAlive(60)
      │      ├─ client.setSocketTimeout(10)
      │      └─ client.setCallback(onMessage)    ⬅ daftarkan callback
      │
      ▼
   loop() (RESTART dari sini bila reboot)
```

### 5.2 Main Loop Flow (dijalankan TERUS-MENERUS)

```
   loop()
      │
      ├─ client.connected() ?
      │     │
      │     ├─ TIDAK ──► reconnect()          (throttled 5 detik)
      │     │               ├─ client.connect(clientID, ..., LWT)
      │     │               ├─ client.subscribe(kontrol/pintu)
      │     │               ├─ client.publish(status/pintu, "online", retained)
      │     │               └─ LED hijau ON
      │     │
      │     └─ YA ──► client.loop()           ⚠ WAJIB: pompa callback MQTT
      │                    │
      │                    └─► publishSensor()    (self-throttled 5 detik)
      │                          ├─ dht.read()
      │                          ├─ build JSON
      │                          ├─ client.publish(sensor/01)
      │                          └─ LED biru blink
      │
      └─ (selesai — loop kembali ke atas)
```

### 5.3 Event Flow: Admin Dashboard Override

```
   Operator klik button "Buka Pintu" di Node-RED dashboard
      │
      ▼
   Node-RED publish "UNLOCK" ke bootcamp/kontrol/pintu (QoS 1)
      │
      ▼
   Broker terima, route ke semua subscriber
      │
      ▼
   ESP32 client.loop() pompa pesan
      │
      ▼
   onMessage(topic, payload, length) ter-trigger
      │
      ├─ Konversi byte[] → null-terminated string
      ├─ Routing per topic
      └─ Topic == kontrol/pintu & payload == "UNLOCK"?
            │
            └─ YA ──► publishStatus("ADMIN_REMOTE")
                     └─ unlockDoor("mqtt-admin")
                           ├─ relay + LED kuning HIGH
                           ├─ publishStatus("UNLOCKED")
                           ├─ delay(3000)
                           ├─ relay + LED kuning LOW
                           └─ publishStatus("LOCKED")
```

### 5.4 Data Flow: Sensor Publish (5 detik)

```
   Timer millis() - lastPublish >= 5000 ms
      │
      ▼
   publishSensor() [di mqtt_handler.cpp]
      │
      ├─ 1. dht.readTemperature()  ──► 28.5 °C
      ├─ 2. dht.readHumidity()     ──► 72.0 %
      │
      ├─ 3. Build JSON via ArduinoJson:
      │      {
      │        "id":    "sensor_01",
      │        "suhu":   28.5,
      │        "lembap": 72.0,
      │        "ts":     4521,
      │        "rssi":   -58
      │      }
      │
      ├─ 4. client.publish("bootcamp/sensor/01", json)
      │      └─ QoS 0, retain=false (fire-and-forget)
      │
      ├─ 5. LED biru blink (HIGH 30ms → LOW)
      │
      └─ 6. Serial log: ">> Publish sensor OK (75 bytes): ..."
```

### 5.5 Data Flow: LWT Offline Detection

```
   Skenario: ESP32 tiba-tiba mati / WiFi putus

   1. ESP32 disconnect dari broker tanpa DISCONNECT packet
        │
        ▼
   2. Broker tidak terima keepalive dalam 1.5x keepalive period (60s × 1.5 = 90s)
        │
        ▼
   3. Broker publish LWT message:
        topic:   bootcamp/status/pintu
        payload: "offline"
        retain:  true
        QoS:     1
        │
        ▼
   4. Semua subscriber (Node-RED dashboard) terima "offline"
        │
        ▼
   5. Dashboard tampilkan "🔴 offline" di audit log
        (LED hijau di ESP32 mati karena client.connected() = false)
```

---

## 6. State Machine Pintu

### 6.1 State Diagram

```
   ┌─────────────────┐
   │   LOCKED        │ ◄── default state (saat boot)
   │   (idle)        │
   └────────┬────────┘
            │
            │ trigger: MQTT "UNLOCK" diterima
            │
            ▼
   ┌─────────────────┐
   │   UNLOCKED      │
   │   relay HIGH    │ ── publishStatus("ADMIN_REMOTE")
   │   LED kuning ON │ ── publishStatus("UNLOCKED")
   │   delay(3000)   │
   └────────┬────────┘
            │
            │ setelah 3 detik
            ▼
   ┌─────────────────┐
   │   LOCKED        │
   │   (kembali)     │ ── publishStatus("LOCKED")
   └─────────────────┘
```

### 6.2 Audit Event Sequence

```
   Trigger via dashboard (satu-satunya cara):
     1. publishStatus("ADMIN_REMOTE")
     2. publishStatus("UNLOCKED")    ── 3 detik ──
     3. publishStatus("LOCKED")
```

---

## 7. Skenario End-to-End

### 7.1 Demo Lengkap (urutan rekomendasi)

| Step | Aksi                                    | Expected Result                                          |
| ---- | --------------------------------------- | -------------------------------------------------------- |
| 1    | Start Wokwi simulation                  | Serial: "MQTT connected" + LED hijau ON                  |
| 2    | Buka Node-RED dashboard `:1880/ui`      | Gauge suhu muncul, update tiap 5 detik                   |
| 3    | Klik button "🔑 Buka Pintu"             | Relay + LED kuning aktif 3 detik; audit `ADMIN_REMOTE`   |
| 4    | Tunggu 3 detik                          | Audit berlanjut: `UNLOCKED` → `LOCKED`                   |
| 5    | Stop simulation mendadak                | Node-RED terima "offline" (LWT retained)                 |
| 6    | Restart simulation                      | Audit `online` kembali; sensor publish lanjut            |

### 7.2 Skenario Failure Modes

| Skenario                | Behavior ESP32                                           |
| ----------------------- | -------------------------------------------------------- |
| WiFi putus              | `reconnect()` throttled 5 detik; LED hijau OFF           |
| Broker unreachable      | `client.state()` = -2 / -4; auto-retry tiap 5 detik      |
| Client ID duplikat      | Broker kick terus → ubah `MQTT_CLIENT_ID` jadi unik      |
| DHT22 baca NaN          | Skip cycle publish (tidak crash)                         |

---

## 8. Perbandingan dengan Topik 3

| Aspek                    | Topik 2 (MQTT)                       | Topik 3 (Firebase RTDB)               |
| ------------------------ | ------------------------------------ | ------------------------------------- |
| Backend                  | `test.mosquitto.org:1883`            | Firebase Console (Google)             |
| Auth                     | ❌ Anonim (broker publik)             | API Key + Security Rules              |
| Library                  | `PubSubClient`                       | `Firebase ESP Client` (Mobizt)        |
| Subscribe                | `client.subscribe(topic)` + callback | `beginStream(path)` + callback        |
| Publish state            | `client.publish(topic, msg)`         | `RTDB.setString/setJSON(path, val)`   |
| Audit log (append)       | ❌ Tidak native                       | ✅ `pushJSON` auto-generate ID         |
| Retained state           | ✅ Flag retain di publish             | ✅ Default behavior (state disimpan)   |
| LWT (offline detect)     | ✅ Native MQTT feature                | ⚠️ Butuh Cloud Function                |
| Realtime push ke web     | MQTT.js + WebSocket                   | ✅ Native via Firebase JS SDK          |
| Setup awal               | Langsung jalan (no config)            | Perlu daftar project + dapatkan API Key |
| Hardware                 | **IDENTIK** (DHT22 + relay + 3 LED)   |                                        |
| GPIO pin                 | **IDENTIK** (4, 27, 26, 18, 12)       |                                        |
| Struktur file modular    | **IDENTIK** (config + mqtt_handler / firebase_handler)               |

> **Insight**: Hardware dan logic bisnis (door control) SAMA. Yang berbeda hanya layer komunikasi. Modular code memungkinkan swap MQTT ↔ Firebase tanpa ubah logic bisnis.

---

## Lampiran: Referensi Cepat

### A. Konstanta Penting (`config.cpp`)

```cpp
const char* WIFI_SSID     = "Wokwi-GUEST";
const char* MQTT_BROKER   = "test.mosquitto.org";
const uint16_t MQTT_PORT  = 1883;
const char* TOPIC_SENSOR  = "bootcamp/sensor/01";
const char* TOPIC_KONTROL = "bootcamp/kontrol/pintu";
const char* TOPIC_STATUS  = "bootcamp/status/pintu";
String      MQTT_CLIENT_ID = "esp32_all_in_01";
const unsigned long PUBLISH_INTERVAL_MS = 5000;
```

### B. Pseudo-code Loop Utama

```python
while True:
    if not mqtt.connected():
        reconnect()              # throttled 5s
    else:
        mqtt.loop()              # pompa callback
        if elapsed(last_publish) >= 5s:
            publish_sensor()     # self-throttled
```

### C. Decision Tree Akses Pintu

```
   pesan MQTT masuk di TOPIC_KONTROL
      │
      ├─ payload == "UNLOCK" ──► unlockDoor("mqtt-admin")
      │                            ├─ publishStatus("ADMIN_REMOTE")
      │                            ├─ publishStatus("UNLOCKED")
      │                            ├─ delay(3000)
      │                            └─ publishStatus("LOCKED")
      │
      └─ payload lain ──► log "Perintah tidak dikenal" (no action)
```
