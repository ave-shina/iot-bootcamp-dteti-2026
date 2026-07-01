# Proyek Wokwi — Bootcamp IoT Topik 3 (ESP32 Firebase All-in-One)

Satu proyek Wokwi tunggal yang mendemonstrasikan **Firebase Realtime Database (RTDB)** bi-directional lengkap dalam 1 ESP32: sebagai **state publisher** (sensor DHT22) sekaligus **stream subscriber** (kontrol akses pintu via Firebase Console / web dashboard).

> **Hardware & wiring IDENTIK dengan Topik 2 (MQTT).** Perbedaan hanya pada layer komunikasi: ganti MQTT broker dengan Firebase RTDB.

---

## Arsitektur

```
                    ┌──────────────────┐
                    │  ESP32 (Wokwi)   │
                    │  All-in-One      │
                    │                  │
   DHT22 ──►  setJSON /sensor/01  ──┐  │
                                    │  │
   Stream ──►  /kontrol/pintu  ◄────┤  │
                    │               │  │
                    │  relay + LED  │  │
                    └──────┬────────┘  │
                           │           │
                           ▼           ▼
                  ┌────────────────────────────┐
                  │  Firebase RTDB             │
                  │  console.firebase.google.com│
                  │                            │
                  │  /sensor/01    (state)     │
                  │  /kontrol/pintu (cmd)      │
                  │  /status/pintu (state)     │
                  │  /logs/pintu   (audit)     │
                  └─────────────┬──────────────┘
                                │
                  read/write    │    onValue()
                                ▼
                  ┌─────────────────────────────┐
                  │  Web Dashboard (Part 4)     │
                  │  HTML + Firebase JS SDK     │
                  │  - gauge suhu + chart       │
                  │  - button "Buka Pintu"      │
                  │  - audit log akses pintu    │
                  └─────────────────────────────┘
```

---

## Komponen & Pin (IDENTIK dengan Topik 2)

| Komponen                | Pin ESP32                       | Fungsi                              |
| ----------------------- | ------------------------------- | ----------------------------------- |
| DHT22                   | VCC 3V3 / GND / SDA **GPIO 4**  | Sensor suhu + kelembapan            |
| Relay module            | **GPIO 27** (VCC 5V, GND)       | Simulasi solenoid door lock         |
| LED "pintu" (kuning)    | **GPIO 26** (R220)              | Indikator pintu terbuka             |
| LED publish (biru)      | **GPIO 18** (R220)              | Blink tiap setJSON sensor           |
| LED status (hijau)      | **GPIO 12** (R220)              | Indikator Firebase ready            |

---

## Path RTDB (mapping dari Topik 2 MQTT topic)

| Topik 2 (MQTT)                  | Topik 3 (RTDB path)    | Operasi Firebase                          | Retain/Setara |
| -------------------------------- | ---------------------- | ----------------------------------------- | ------------- |
| `bootcamp/sensor/01`             | `/sensor/01`           | `Firebase.RTDB.setJSON()`                 | Overwrite state |
| `bootcamp/kontrol/pintu` (sub)   | `/kontrol/pintu`       | `Firebase.RTDB.beginStream()` + callback  | Subscribe    |
| `bootcamp/status/pintu` (pub)    | `/status/pintu`        | `Firebase.RTDB.setString()`               | Retained state |
| _(MQTT tidak punya)_             | `/logs/pintu/<auto-id>`| `Firebase.RTDB.pushJSON()`                | Append audit log |

> 💡 **`set`** = overwrite (state terakhir). **`push`** = append auto-ID (historis/audit). **`beginStream`** = subscribe realtime perubahan path.

---

## Setup Wajib Sebelum Menjalankan

### 1. Buat Project Firebase

1. Buka https://console.firebase.google.com → sign in dengan akun Google.
2. **Add project** → nama: `bootcamp-iot-2026-<nama>`.
3. Catat **Project ID** (mis. `bootcamp-iot-2026-andi`).
4. (Opsional) Disable Google Analytics → simplify setup.
5. Tunggu ~30 detik → Continue.

### 2. Aktifkan Realtime Database

1. Sidebar → **Build → Realtime Database**.
2. **Create Database** → region: `United States (us-central1)`.
3. Security rules: **Start in test mode** (permissive 30 hari).
4. Enable → catat **Database URL**:
   ```
   https://bootcamp-iot-2026-andi-default-rtdb.firebaseio.com/
   ```

### 3. Dapatkan API Key (Web Config)

1. **Project Settings** (ikon gear) → **General** → scroll ke **"Your apps"**.
2. Klik ikon **`</>`** (Web app) → nickname: `bootcamp-web`.
3. Register app → copy `firebaseConfig` muncul:
   ```javascript
   const firebaseConfig = {
     apiKey:            "AIzaSyA1B2C3D4E5F6G7H8I9J0K_L_M_N_O_P_Q",
     databaseURL:       "https://bootcamp-iot-2026-andi-default-rtdb.firebaseio.com/",
     projectId:         "bootcamp-iot-2026-andi",
     // ...
   };
   ```
4. Catat **`apiKey`** dan **`databaseURL`**.

### 4. Update Sketch

Edit `config.cpp`:

```cpp
const char* DATABASE_URL = "https://bootcamp-iot-2026-andi-default-rtdb.firebaseio.com/";
const char* API_KEY      = "AIzaSyA1B2C3D4E5F6G7H8I9J0K_L_M_N_O_P_Q";
```

Ganti dengan URL + Key project Anda.

---

## Alur Demo

1. **Sensor publish** — tiap 5 detik, DHT22 dibaca → `PUT` JSON ke `/sensor/01` → LED biru blink.
2. **Kontrol pintu via Firebase** — di Firebase Console (atau web dashboard), set `/kontrol/pintu = "UNLOCK"` (buka) atau `"LOCK"` (kunci) → `pollKontrol()` mendeteksi perubahan → ESP buka/kunci relay (persistent, no auto-lock) + `pushAudit("ADMIN_REMOTE")` / `"ADMIN_LOCK")`. State pintu tertulis di `/status/pintu`; presence (online) terpisah di `/status/presence`.

---

## Cara Pakai

### 1. Setup Wokwi

1. Buka https://wokwi.com → **New Project Arduino**.
2. Ganti isi `diagram.json`, `sketch.cpp`, dan buat `libraries.txt` dari folder proyek ini.
3. Upload juga `config.h`, `config.cpp`, `firebase_handler.h`, `firebase_handler.cpp`.
4. Update `DATABASE_URL` + `API_KEY` di `config.cpp`.
5. **Start Simulation**.

### 2. Verifikasi Serial

Pastikan Serial Monitor (115200 baud) menampilkan:

```
============================================
  ESP32 + Firebase RTDB (Sensor + Pintu)
============================================
  Kontrol pintu via Firebase Console / web.
  Set /kontrol/pintu = "UNLOCK" untuk membuka.

Menghubungkan WiFi: Wokwi-GUEST
.....
✓ WiFi OK. IP: 10.0.0.42  RSSI: -55 dBm
...............
Firebase ready: YES
>> Status set: online
✓ Stream /kontrol/pintu subscribed (admin override ready).

Mulai publish sensor tiap 5 detik...

>> Write /sensor/01 OK (suhu=28.5 lembap=72.0)
```

### 3. Verifikasi di Firebase Console

Buka **Firebase Console → Realtime Database → Data**. Lihat node muncul & ter-update:

```
bootcamp-iot-2026-andi/
├── sensor/
│   └── 01/
│       ├── id:     "sensor_01"
│       ├── suhu:   28.5         ← update tiap 5 detik
│       ├── lembap: 72.0
│       ├── ts:     4521
│       └── rssi:   -58
├── status/
│   └── pintu: "online" / "LOCKED" / "UNLOCKED"
├── kontrol/
│   └── pintu: ""  ← set "UNLOCK" untuk admin override
└── logs/
    └── pintu/
        ├── -NXYZ12345: { event:"ADMIN_REMOTE", ts:4521, ... }
        └── -NXYZ12346: { event:"UNLOCKED", ... }
```

### 4. Test Kontrol Pintu

| Skenario       | Aksi                                          | Hasil Expected                                              |
| -------------- | --------------------------------------------- | ----------------------------------------------------------- |
| Buka via Console | Console: set `/kontrol/pintu = "UNLOCK"`      | Relay + LED kuning ON & tetap; audit `ADMIN_REMOTE`→`UNLOCKED` |
| Kunci via Console| Console: set `/kontrol/pintu = "LOCK"`        | Relay + LED kuning OFF; audit `ADMIN_LOCK`→`LOCKED`            |
| Buka/Kunci web   | Web dashboard: klik button UNLOCK / LOCK      | Sama seperti via Console                                       |
| Status pintu     | Lihat `/status/pintu`                         | Hanya `LOCKED`/`UNLOCKED` (bersih, retained)                   |
| Presence         | Lihat `/status/presence`                      | `online` (terpisah dari state pintu)                           |

### 5. Web Dashboard (Opsional, Materi Part 4)

Buat HTML sederhana dengan Firebase JS SDK:

```html
<!-- Button admin override -->
<button onclick="db.ref('kontrol/pintu').set('UNLOCK')">Buka Pintu</button>

<!-- Real-time gauge suhu -->
<script>
  db.ref('sensor/01').on('value', snap => {
    const data = snap.val();
    document.getElementById('suhu').textContent = data.suhu + ' °C';
  });

  // Audit log realtime
  db.ref('logs/pintu').limitToLast(10).on('child_added', snap => {
    console.log('Audit:', snap.val());
  });
</script>
```

---

## Perbandingan dengan Topik 2 (MQTT)

| Aspek                | Topik 2 (MQTT)                       | Topik 3 (Firebase RTDB)               |
| -------------------- | ------------------------------------ | ------------------------------------- |
| Broker/Backend       | `test.mosquitto.org:1883`            | Firebase Console (Google)             |
| Auth                 | ❌ Anonim (broker publik)             | API Key + Security Rules              |
| Library              | PubSubClient                         | Firebase ESP Client by Mobizt         |
| Subscribe            | `client.subscribe(topic)` + callback | `beginStream(path)` + callback        |
| Publish state        | `client.publish(topic, msg)`         | `RTDB.setString/setJSON(path, val)`   |
| Audit log (append)   | ❌ Tidak native (harus manual timestamp) | ✅ `pushJSON` auto-generate ID         |
| Retained             | ✅ Flag retain di publish             | ✅ Default behavior (state disimpan)   |
| LWT (offline detect) | ✅ Last Will Testament                | ⚠️ Perlu Cloud Function (tidak di demo)|
| Realtime push ke web | Harus pakai MQTT.js + WebSocket       | ✅ Native via Firebase JS SDK          |
| Skalabilitas         | Broker sendiri butuh setup            | ✅ Auto-scale (Google infrastructure)  |

---

## Troubleshooting

| Masalah                              | Penyebab & Solusi                                            |
| ------------------------------------ | ------------------------------------------------------------ |
| `Firebase ready: NO`                 | API_KEY / DATABASE_URL salah → cek ulang firebaseConfig      |
| `permission denied`                  | Security rules ketat → pakai Test mode (permissive 30 hari)  |
| `connection lost`                    | WiFi turun → tunggu `Firebase.reconnectWiFi(true)` auto      |
| `timeout`                            | RTDB tidak response → cek internet / region (pakai us-central1) |
| Stream callback tidak trigger        | Cek path sama persis (case-sensitive); restart setelah update rules |
| Button klik tapi relay mati          | Cek wiring GPIO 27 + VCC 5V relay module                     |
| Data di Console tidak muncul         | Cek `Firebase.ready() == true` sebelum operasi              |
| Serial stuck "Menghubungkan WiFi"    | SSID/pass salah; di Wokwi pakai `Wokwi-GUEST`                |

---

## Catatan Keamanan

⚠️ **Firebase Test Mode = permissive 30 hari.**
- Siapa saja dengan URL bisa baca/tulis data Anda.
- API_KEY **bukan rahasia** — keamanan ada di **security rules**.
- Setelah kelas: ganti rules ke authenticated-only atau **hapus project** via Project Settings → Delete.

**Security rules produksi yang disarankan:**
```json
{
  "rules": {
    "sensor":   { ".read": "auth != null", ".write": "auth != null" },
    "status":   { ".read": "auth != null", ".write": "auth != null" },
    "kontrol":  { ".read": "auth != null", ".write": "auth != null" },
    "logs":     { ".read": "auth != null", ".write": "auth != null" }
  }
}
```

---

## Relasi ke Materi Slide

- **Topik 3 Part 1:** Pengenalan Firebase & RTDB.
- **Topik 3 Part 2:** Setup project + security rules + dapatkan API Key.
- **Topik 3 Part 3:** ESP32 + Firebase ESP Client library (this project — setJSON/beginStream/pushJSON).
- **Topik 3 Part 4:** Web dashboard baca RTDB real-time + button kontrol admin override.
