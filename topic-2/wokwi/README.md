# Proyek Wokwi — Bootcamp IoT Topik 2 (ESP32 MQTT All-in-One)

Satu proyek Wokwi tunggal yang mendemonstrasikan **MQTT bi-directional** lengkap dalam 1 ESP32: sebagai **publisher** (sensor DHT22) sekaligus **subscriber** (kontrol akses pintu via dashboard Node-RED).

---

## Arsitektur

```
                    ┌──────────────────┐
                    │  ESP32 (Wokwi)   │
                    │  All-in-One      │
                    │                  │
   DHT22 ──►  publish sensor  ─────┐   │
                                   │   │
   MQTT  ──►  subscribe kontrol ◄──┤   │
                    │              │   │
                    │  relay + LED │   │
                    └──────┬───────┘   │
                           │           │
                           ▼           ▼
                      ┌────────────────────┐
                      │  test.mosquitto.org│
                      │     (broker)       │
                      └─────────┬──────────┘
                                │
                  subscribe     │     publish
                                ▼
                  ┌─────────────────────────────┐
                  │  Node-RED dashboard         │
                  │  localhost:1880/ui          │
                  │  - gauge suhu + chart       │
                  │  - tombol Buka & Tutup     │
                  │  - audit log akses pintu    │
                  └─────────────────────────────┘
```

---

## Komponen & Pin

| Komponen                | Pin ESP32                       | Fungsi                              |
| ----------------------- | ------------------------------- | ----------------------------------- |
| DHT22                   | VCC 3V3 / GND / SDA **GPIO 4**  | Sensor suhu + kelembapan            |
| Relay module            | **GPIO 27** (VCC 5V, GND)       | Simulasi solenoid door lock         |
| LED "pintu" (kuning)    | **GPIO 26** (R220)              | Indikator pintu terbuka             |
| LED publish (biru)      | **GPIO 18** (R220)              | Blink tiap publish sensor           |
| LED status (hijau)      | **GPIO 12** (R220)              | Indikator MQTT connected            |

**Total GPIO dipakai**: 5 pin (1 DHT + 1 relay + 3 LED)

---

## Topik MQTT

| Topic                          | Arah        | Payload                                            | Retain |
| ------------------------------ | ----------- | -------------------------------------------------- | ------ |
| `bootcamp/sensor/01`           | ESP → broker| JSON `{"id","suhu","lembap","ts","rssi"}`          | ❌     |
| `bootcamp/kontrol/pintu`       | broker → ESP| `"UNLOCK"` / `"LOCK"` (dari dashboard)             | ❌     |
| `bootcamp/status/pintu`        | ESP → broker| `LOCKED` / `UNLOCKED` + audit `ADMIN_REMOTE` / `ADMIN_LOCK` | ✅ |
| `bootcamp/status/presence`     | ESP → broker| `online` / `offline` (LWT — connection presence)   | ✅     |

---

## Alur Demo

1. **Sensor publish** — tiap 5 detik, DHT22 dibaca → JSON dipublish ke `bootcamp/sensor/01` → LED biru blink.
2. **Kontrol pintu via dashboard** — operator klik "🔑 Buka Pintu" → ESP terima `UNLOCK` → relay + LED kuning ON & **tetap** (no auto-lock) → audit `ADMIN_REMOTE` lalu state `UNLOCKED`. Klik "🔒 Tutup Pintu" → `LOCK` → relay OFF, audit `ADMIN_LOCK` lalu `LOCKED`. State pintu dipublikasi retained di `status/pintu`; presence (online/offline) terpisah di `status/presence`.

---

## Cara Pakai

### 1. Setup Wokwi

1. Buka https://wokwi.com → **New Project Arduino**.
2. Ganti isi `diagram.json`, `sketch.cpp`, dan buat `libraries.txt` dari folder proyek ini.
3. Upload juga `config.h`, `config.cpp`, `mqtt_handler.h`, `mqtt_handler.cpp`.
4. **Start Simulation**.

### 2. Verifikasi Serial

Pastikan Serial Monitor (115200 baud) menampilkan:

```
============================================
  ESP32 MQTT ALL-IN-ONE (Sensor + Pintu)
============================================
  Kontrol pintu via dashboard Node-RED.
  'Buka Pintu' = UNLOCK, 'Tutup Pintu' = LOCK.

Menghubungkan WiFi: Wokwi-GUEST
.....
✓ WiFi OK. IP: 10.0.0.123  RSSI: -55 dBm
Reconnect MQTT... OK
  Subscribed: bootcamp/kontrol/pintu

>> Publish sensor OK (75 bytes): {"id":"sensor_01","suhu":28.5,...}
>> Publish sensor OK (75 bytes): ...
```

### 3. Test Sensor (Publisher)

- Buka **MQTT Explorer** → connect `test.mosquitto.org:1883`.
- Subscribe `bootcamp/sensor/#` → lihat JSON masuk tiap 5 detik.

### 4. Test Kontrol Pintu (Subscriber)

| Skenario           | Aksi                                       | Hasil Expected                                                |
| ------------------ | ------------------------------------------ | ------------------------------------------------------------- |
| Buka via dashboard | Klik "🔑 Buka Pintu" di Node-RED            | Relay + LED kuning ON & tetap; audit `ADMIN_REMOTE`→`UNLOCKED`|
| Tutup via dashboard| Klik "🔒 Tutup Pintu" di Node-RED           | Relay + LED kuning OFF; audit `ADMIN_LOCK`→`LOCKED`           |
| Manual via MQTT    | Publish `UNLOCK`/`LOCK` ke `kontrol/pintu` | Sama seperti di atas                                          |
| State retained     | Buka dashboard baru / restart broker       | Widget Status Pintu langsung dapat `LOCKED`/`UNLOCKED`        |
| LWT offline        | Stop simulasi mendadak                     | Broker publish `offline` ke `status/presence` (retained)      |

### 5. Node-RED Dashboard

Lihat `node-red/README.md` untuk setup Node-RED + import flow.

---

## Troubleshooting

| Masalah                          | Penyebab & Solusi                                            |
| -------------------------------- | ------------------------------------------------------------ |
| `client.state()` = -2 / -4       | Broker unreachable → cek WiFi / firewall                     |
| DHT22 baca NaN                   | Cek wiring 3V3/GND/SDA; pastikan `dht.begin()` di `setup()` |
| Button klik tapi relay mati      | Cek wiring GPIO 27 + VCC 5V relay module                     |
| Tidak ada callback dipanggil     | Lupa `client.setCallback()` atau `client.loop()`            |
| Payload corrupt / parsing error  | Null-terminator lupa → `message[length]='\0'`               |
| Broker kick terus                | Client ID duplikat → ubah `MQTT_CLIENT_ID` jadi unik         |
| MQTT Explorer blank              | Cek koneksi internet; coba broker HiveMQ publik              |

---

## Catatan Keamanan

⚠️ **Broker `test.mosquitto.org` = PUBLIK tanpa auth.**
- Siapa saja bisa subscribe topic Anda → **jangan** kirim data sensitif.
- Siapa saja bisa publish `UNLOCK`/`LOCK` → **bisa membuka/mengunci pintu Anda dari jauh!**
- Untuk produksi → broker sendiri (Mosquitto/HiveMQ) + auth + TLS (port 8883) + ACL.

---

## Relasi ke Materi Slide

- **Topik 2 Part 1:** Teori MQTT (pub/sub, topic, QoS, broker).
- **Topik 2 Part 2:** ESP32 MQTT publish — bagian sensor DHT22 di sketch ini.
- **Topik 2 Part 3:** Node-RED dashboard subscribe → gauge + chart.
- **Topik 2 Part 4:** Bi-directional kontrol aktuator — bagian admin override via dashboard.
