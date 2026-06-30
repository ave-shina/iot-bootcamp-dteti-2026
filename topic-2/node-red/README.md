# Node-RED Flow — Topik 2 (MQTT All-in-One)

File `topic-2-flow.json` adalah flow Node-RED siap-import untuk dashboard demo Topik 2.

---

## Isi Flow

```
┌─ 📜 SENSOR (subscribe bootcamp/sensor/01) ─────────────┐
│                                                          │
│  mqtt in ─► json ─┬─► gauge suhu                         │
│                   ├─► gauge kelembapan                   │
│                   ├─► chart suhu (5 menit)               │
│                   └─► chart kelembapan (5 menit)         │
└──────────────────────────────────────────────────────────┘

┌─ 🔑 KONTROL PINTU (button → publish UNLOCK) ────────────┐
│                                                          │
│  button "Buka Pintu"     ─► mqtt out                     │
│  button "Reset Perintah" ─► mqtt out                     │
│    (payload UNLOCK, QoS 1, retain=false)                 │
└──────────────────────────────────────────────────────────┘

┌─ 📋 AUDIT LOG (subscribe status/pintu) ─────────────────┐
│                                                          │
│  mqtt in ─► function (timestamp + emoji)                 │
│              ├─► text "Event Akses Terakhir"             │
│              └─► text "Status Terakhir"                  │
└──────────────────────────────────────────────────────────┘
```

---

## Cara Import

### 1. Install Node-RED (bila belum)

```bash
# Butuh Node.js 18+ terlebih dahulu
npm install -g node-red

# Jalankan
node-red

# Buka http://localhost:1880
```

### 2. Install Dashboard Nodes

Flow ini pakai `node-red-dashboard` (UI widgets: gauge/chart/button/text).

```bash
# Dari folder user data Node-RED (~/.node-red)
cd ~/.node-red
npm install node-red-dashboard

# Atau via Node-RED menu: Manage palette → Install → search "node-red-dashboard"
```

Restart Node-RED setelah install.

### 3. Import Flow

1. Buka Node-RED di browser: `http://localhost:1880`
2. Klik tombol menu (☰) di kanan atas → **Import**
3. Pilih tab **select a file to import**
4. Browse ke file `topic-2-flow.json` ini
5. Klik **Import**
6. Drag flow ke posisi yang diinginkan di canvas
7. Klik **Deploy** (tombol merah di kanan atas)

### 4. Buka Dashboard

Buka: `http://localhost:1880/ui`

Anda akan melihat 3 group:
- **Sensor DHT22** → gauge suhu/kelembapan + chart historis
- **Kontrol Pintu** → button buka pintu + reset
- **Audit Log Akses** → event akses realtime + status terakhir

---

## Verifikasi End-to-End

### Test 1: Sensor realtime
1. Start Wokwi simulation (project Topik 2)
2. Lihat gauge suhu di dashboard ter-update tiap 5 detik
3. LED biru di Wokwi blink tiap publish

### Test 2: Kontrol pintu via dashboard
1. Klik button **"🔑 Buka Pintu"** di dashboard
2. LED hijau + relay di Wokwi aktif 3 detik
3. Audit log di dashboard muncul: `👑 ADMIN_REMOTE`
4. Setelah 3 detik: `🔓 UNLOCKED` → `🔒 LOCKED`

---

## Customization

### Ganti prefix topic (hindari tabrakan peserta lain)

Edit 3 topic reference di flow:

| Node | Topic lama | Topic baru (contoh) |
|---|---|---|
| `mqtt_sensor_in` | `bootcamp/sensor/01` | `bootcamp/andi/sensor/01` |
| `mqtt_kontrol_out` + `button_unlock`/`button_lock` | `bootcamp/kontrol/pintu` | `bootcamp/andi/kontrol/pintu` |
| `mqtt_status_in` | `bootcamp/status/pintu` | `bootcamp/andi/status/pintu` |

**Jangan lupa update topic di `config.cpp` ESP32 juga** supaya sinkron.

### Ganti broker MQTT

Edit node broker config (id: `broker1`):
- `broker`: hostname broker (mis. `broker.hivemq.com`)
- `port`: 1883 (plaintext) atau 8883 (TLS)

---

## Troubleshooting

| Masalah | Solusi |
|---|---|
| `mqtt in` status "disconnected" | Cek internet; coba broker lain (`broker.hivemq.com`) |
| Dashboard 404 / blank | Install `node-red-dashboard` (lihat langkah 2) |
| Sensor gauge tidak update | Cek topic persis sama (case-sensitive) |
| Button klik tidak ada efek | Cek retain flag = `false` (jangan cache UNLOCK) |
| Audit log kosong | Cek ESP32 MQTT connected (LED hijau ON) |
| Banyak event tertumpuk | Itu wajar — `mqtt in` real-time, tiap publish muncul |

---

## Arsitektur Lengkap End-to-End

```
[Wokwi ESP32]                                  [Node-RED + Dashboard]
                                              localhost:1880
   DHT22 ──► JSON ──► publish              ┌──────────────────────────┐
              bootcamp/sensor/01 ─────►  │ mqtt in → gauge + chart   │
                                            └──────────────────────────┘
              bootcamp/status/pintu ────►  │ mqtt in → text (audit)    │
                                            └──────────────────────────┘
   Subscribe bootcamp/kontrol/pintu ◄────  │ button → mqtt out         │
              ▲                              └──────────────────────────┘
              │                                        ▲
              └────── broker: test.mosquitto.org ─────┘
                       (port 1883, no auth)
```

Selamat mencoba! Bila flow tidak jalan, cek tab **Debug** di Node-RED untuk lihat error.
