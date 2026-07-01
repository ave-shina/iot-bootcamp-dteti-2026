# 🚀 Bootcamp IoT — DTETI 2026

> Kumpulan project **ESP32 IoT** progresif: dari sensor/aktuator lokal, konektivitas **MQTT**, sampai integrasi **Firebase Realtime Database**. Semua project disimulasikan di **[Wokwi](https://wokwi.com/)** — tanpa perlu hardware fisik.

---

## 📖 Tentang Repository

Repository ini berisi materi praktikum Bootcamp IoT (Departemen Teknik Elektro dan Teknologi Informasi — 2026). Materi disusun dalam **3 topik bertahap** yang membangun satu sama lain: mulai dari konsep dasar I/O embedded, lalu menambahkan layer komunikasi cloud, dan akhirnya migrasi antar-protokol dengan arsitektur modular.

Setiap topik punya dokumen `ARCHITECTURE.md` yang membahas **arsitektur sistem, alur program, state machine, skenario end-to-end, dan perbandingan antar topik** secara mendetail.

| Topik | Fokus | Sensor | Aktuator | Komunikasi |
| :---: | --- | --- | --- | --- |
| **1** | Sensor & Aktuator (I/O dasar) | Potensiometer, Keypad 4×4 | LED PWM, LED binary | ❌ (lokal, Serial) |
| **2** | MQTT All-in-One | DHT22 | Relay + 3 LED | `test.mosquitto.org` (MQTT) |
| **3** | Firebase RTDB (REST) | DHT22 | Relay + 3 LED | Firebase RTDB via HTTPS |

---

## 🗂️ Struktur Repository

```
iot-bootcamp-dteti-2026/
├── topic-1/                       # Sensor & Aktuator
│   ├── ARCHITECTURE.md
│   └── wokwi/
│       ├── 01_sensor_aktuator_potensiometer/   # Analog → PWM
│       └── 02_digital_sensor_pin/              # Keypad matrix → akses PIN
│
├── topic-2/                       # MQTT All-in-One
│   ├── ARCHITECTURE.md
│   ├── wokwi/                     # ESP32 modular (config + mqtt_handler)
│   └── node-red/                  # Flow dashboard Node-RED
│
└── topic-3/                       # Firebase RTDB (REST API)
    ├── ARCHITECTURE.md
    ├── wokwi/                     # ESP32 modular (config + firebase_handler)
    └── node-red/                  # Flow dashboard Node-RED
```

> **Catatan:** Topik 2 & 3 punya **hardware & logic bisnis identik** (DHT22 + relay + 3 LED + state machine pintu). Yang berbeda hanya layer komunikasi. Struktur kode modular membuat MQTT ↔ Firebase bisa ditukar tanpa mengubah logic bisnis.

---

## 🔬 Topik 1 — Sensor & Aktuator (I/O Dasar)

Pengenalan dua sisi fundamental edge device: sensor **analog kontinu** (perlu ADC) vs **digital diskrit** (perlu scanning/debouncing), serta aktuator **PWM** vs **binary**.

- **Studi Kasus 01** — Potensiometer → ADC 12-bit → mapping PWM 8-bit → kecerahan LED
- **Studi Kasus 02** — Keypad matrix 4×4 → akumulasi 4 digit → verifikasi PIN → LED akses (state machine + edge detection)

Konsep ESP32 Arduino Core 3.x yang dipakai: `ledcAttach()` / `ledcWrite()` (API baru menggantikan `ledcSetup` Core 2.x).

---

## 📡 Topik 2 — MQTT All-in-One

ESP32 bertindak sebagai **publisher** (sensor DHT22 → `bootcamp/sensor/01`) sekaligus **subscriber** (kontrol pintu dari dashboard Node-RED). Menggunakan broker publik `test.mosquitto.org`.

- **3 topik MQTT**: `sensor/01`, `kontrol/pintu`, `status/pintu`
- **Fitur**: retained message, LWT (Last Will & Testament) untuk deteksi offline, QoS 0/1
- **Dashboard Node-RED**: gauge suhu/kelembapan, chart historis, button buka pintu, audit log

---

## 🔥 Topik 3 — Firebase RTDB (REST API)

Migrasi dari MQTT ke **Firebase Realtime Database** — diakses via **REST API** (`HTTPClient` + `ArduinoJson`) alih-alih library Mobizt agar build Wokwi tetap cepat.

- **4 path RTDB**: `/sensor/01` (PUT), `/kontrol/pintu` (GET poll), `/status/pintu` (PUT), `/logs/pintu/*` (POST append)
- **State machine persisten**: pintu tetap di state terakhir tanpa auto-lock; **state survive reboot** lewat boot restoration
- **Audit trail**: `POST` auto-generate ID → history akses tersimpan permanen

---

## ⚡ Quick Start

Semua project berjalan di simulator **Wokwi** (browser) — tidak butuh hardware.

1. Buka folder topik → `wokwi/` → klik **`diagram.json`**
2. Buka di [wokwi.com](https://wokwi.com/) (atau ekstensi VS Code Wokwi)
3. Jalankan simulasi, amati output di **Serial Monitor** (115200 baud)

**Topik 3 (Firebase)** memerlukan setup tambahan:
- Buat project Firebase → salin `DATABASE_URL` ke `config.cpp`
- (Test mode: security rules permissive, URL tidak butuh `?auth=`)

Untuk dashboard Node-RED (Topik 2 & 3):
- Import `flow.json` dari folder `node-red/` → Deploy → buka `http://localhost:1880/ui`

---

## 🛠️ Tech Stack

| Kategori | Teknologi / Library |
| --- | --- |
| **Board** | ESP32 DevKit (Wokwi simulator) |
| **Bahasa** | C/C++ (Arduino framework) |
| **Sensor** | Potensiometer, Keypad 4×4, DHT22 |
| **Aktuator** | LED, Relay (solenoid door lock) |
| **MQTT** | `PubSubClient` (Knolleary), `test.mosquitto.org` |
| **Firebase** | `HTTPClient` + `WiFiClientSecure` + `ArduinoJson` (REST) |
| **Dashboard** | Node-RED (`node-red-dashboard`) |
| **JSON** | `ArduinoJson` (bblanchon) |

---

## 📚 Dokumentasi

Setiap topik didokumentasikan secara menyeluruh — baca file `ARCHITECTURE.md` di masing-masing folder untuk diagram high-level, wiring & pinout, alur boot/loop, state machine, skenario demo, dan tabel perbandingan.

- 📄 [`topic-1/ARCHITECTURE.md`](topic-1/ARCHITECTURE.md) — Analog vs Digital, ADC→PWM, matrix scanning
- 📄 [`topic-2/ARCHITECTURE.md`](topic-2/ARCHITECTURE.md) — MQTT pub/sub, retained, LWT, state machine pintu
- 📄 [`topic-3/ARCHITECTURE.md`](topic-3/ARCHITECTURE.md) — Firebase REST, polling vs stream, boot restoration

---

## 📝 Lisensi

Materi pembelajaran untuk keperluan edukasi Bootcamp IoT DTETI 2026. Bebas digunakan dan dimodifikasi untuk tujuan belajar.
