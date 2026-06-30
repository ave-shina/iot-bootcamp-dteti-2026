# Arsitektur & Flow — Topik 1 (Sensor & Aktuator)

> Dokumentasi arsitektur sistem dan alur program untuk dua studi kasus di Topik 1 (Pertemuan Online #1):
>
> - **01 — Sensor Analog + Aktuator PWM** (Potensiometer → LED meredup/terang)
> - **02 — Sensor Digital + Aktuator Binary** (Keypad matrix 4×4 → LED akses)

---

## Daftar Isi

1. [Arsitektur Sistem](#1-arsitektur-sistem)
2. [Komponen Hardware](#2-komponen-hardware)
3. [Komponen Software](#3-komponen-software)
4. [Pinout & Wiring](#4-pinout--wiring)
5. [Alur Program — Studi Kasus 01 (Analog)](#5-alur-program--studi-kasus-01-analog)
6. [Alur Program — Studi Kasus 02 (Digital)](#6-alur-program--studi-kasus-02-digital)
7. [State Machine PIN (Studi Kasus 02)](#7-state-machine-pin-studi-kasus-02)
8. [Skenario End-to-End](#8-skenario-end-to-end)
9. [Perbandingan 01 vs 02 (Analog vs Digital)](#9-perbandingan-01-vs-02-analog-vs-digital)

---

## 1. Arsitektur Sistem

### 1.1 Diagram High-Level — Studi Kasus 01 (Analog)

```
   ┌──────────────────────────────────────────────────────────────┐
   │                       ESP32 (Wokwi)                          │
   │                  DevKit V1 (30 pin)                          │
   │                                                              │
   │   Potensiometer ──► ADC (GPIO 34) ──► angka 0..4095          │
   │        (0..3.3V)                    (12-bit)                 │
   │                                                              │
   │   map(adc, 0, 4095, 0, 255) ──► duty cycle PWM (8-bit)       │
   │                                                              │
   │   LEDC (hardware PWM) ──► GPIO 13 ──► R 220Ω ──► LED         │
   │   freq 5 kHz, res 8-bit                  (kuning)            │
   └──────────────────────────────────────────────────────────────┘
                             │
                             │ Serial USB (115200 baud)
                             ▼
                  ┌────────────────────────┐
                  │   Serial Monitor (PC)  │
                  │   ADC / V / PWM log    │
                  └────────────────────────┘

   Rantai proses:  ANALOG → DIGITAL → MAPPING → PWM → CAHAYA
```

### 1.2 Diagram High-Level — Studi Kasus 02 (Digital)

```
   ┌──────────────────────────────────────────────────────────────┐
   │                       ESP32 (Wokwi)                          │
   │                  DevKit C V4                                │
   │                                                              │
   │   Keypad 4×4 (matrix) ──► 8 GPIO ──► Keypad library         │
   │     Rows: 27, 14, 12, 13        (matrix scanning)           │
   │     Cols: 32, 33, 25, 26                                     │
   │                          │                                   │
   │                          ▼                                   │
   │   keypad.getKey() ──► akumulasi 4 digit ──► checkPIN()       │
   │                                              │                │
   │   PIN == "1234"? ───── BENAR ──► digitalWrite(18, HIGH) 3s   │
   │                └──── SALAH ───► LED tetap mati               │
   │                                                              │
   │   GPIO 18 ──► R 1kΩ ──► LED merah (indikator akses)          │
   └──────────────────────────────────────────────────────────────┘
                             │
                             │ Serial USB (115200 baud)
                             ▼
                  ┌────────────────────────┐
                  │   Serial Monitor (PC)  │
                  │   tombol / input / hasil verifikasi
                  └────────────────────────┘

   Rantai proses:  KEYPAD → SCAN → KARAKTER → BUFFER → MATCH → LED
```

### 1.3 Layer Arsitektur (generik, kedua studi kasus)

```
   LAYER 3: AKTUATOR        LED (PWM untuk 01, binary untuk 02)
        ▲
        │ digitalWrite / ledcWrite
        │
   LAYER 2: PROCESSING      ESP32 (mapping ADC→PWM / string match PIN)
        ▲
        │ analogRead / keypad.getKey()
        │
   LAYER 1: SENSOR          Potensiometer (analog) / Keypad matrix (digital)

   ❌ Tidak ada layer transport (WiFi/MQTT/Firebase) di Topik 1.
      Topik 1 murni embedded I/O lokal — persiapan untuk Topik 2 & 3.
```

---

## 2. Komponen Hardware

### 2.1 Studi Kasus 01 — Sensor Analog

| Komponen              | Pin ESP32                       | Fungsi                           | Kategori |
| --------------------- | ------------------------------- | -------------------------------- | -------- |
| Potensiometer (Wokwi) | VCC 3V3 / GND / SIG **GPIO 34** | Sensor analog (pembagi tegangan) | Input    |
| LED kuning            | **GPIO 13** (R 220Ω)            | Aktuator PWM (kecerahan)         | Output   |

**Total GPIO dipakai**: 2 pin (1 analog input + 1 PWM output)

### 2.2 Studi Kasus 02 — Sensor Digital

| Komponen          | Pin ESP32                     | Fungsi                          | Kategori |
| ----------------- | ----------------------------- | ------------------------------- | -------- |
| Keypad matrix 4×4 | Rows: **GPIO 27, 14, 12, 13** | Sensor digital (keystroke)      | Input    |
|                   | Cols: **GPIO 32, 33, 25, 26** |                                 | Input    |
| LED merah         | **GPIO 18** (R 1kΩ)           | Aktuator binary (akses granted) | Output   |
| Breadboard half   | (hub wiring)                  | Wiremapping fisik yang rapi     | -        |

**Total GPIO dipakai**: 9 pin (8 keypad + 1 LED) — hemat 50% vs 16 pin individual

---

## 3. Komponen Software

### 3.1 Struktur File

```
topic-1/wokwi/
├── 01_sensor_aktuator_potensiometer/
│   ├── sketch.ino         ← Single-file: setup() + loop() + komentar edukasi
│   └── diagram.json       ← Wokwi wiring (ESP32 DevKit V1)
│
└── 02_digital_sensor_pin/
    ├── sketch.ino         ← Single-file: setup() + loop() + checkPIN()
    ├── diagram.json       ← Wokwi wiring (ESP32 DevKit C V4 + breadboard)
    └── libraries.txt      ← Keypad by Chris--A
```

> **Catatan**: Berbeda dengan Topik 2 & 3 yang sudah memakai struktur modular (config + handler), Topik 1 masih single-file `.ino` karena fokusnya adalah konsep dasar I/O. Komentar edukatif sangat lengkap di dalam kode.

### 3.2 Library Yang Dipakai

| Studi Kasus | Library         | Author   | Fungsi                                  | Flash Footprint |
| ----------- | --------------- | -------- | --------------------------------------- | --------------- |
| 01          | (tanpa library) | -        | `analogRead`, `ledcAttach`, `ledcWrite` | 0 (bawaan core) |
| 02          | `Keypad.h`      | Chris--A | Abstraksi matrix scanning 4×4           | ~3 KB           |

### 3.3 API ESP32 Core 3.x yang Dipakai

| API                          | Fungsi                                       | Diperkenalkan di               |
| ---------------------------- | -------------------------------------------- | ------------------------------ |
| `analogReadResolution(12)`   | Set resolusi ADC ke 12-bit (0..4095)         | Core 2.x+                      |
| `ledcAttach(pin, freq, res)` | Hubungkan pin ke hardware LEDC PWM generator | **Core 3.x** (breaking change) |
| `ledcWrite(pin, duty)`       | Set duty cycle PWM (0..2^res − 1)            | Core 3.x                       |

> **Migrasi Core 2.x → 3.x**: Core 2.x lama memakai `ledcSetup(channel, freq, res)` + `ledcAttachPin(pin, channel)` + `ledcWrite(channel, duty)` dengan manajemen channel manual. Core 3.x menyembunyikan channel, cukup sebut pin.

---

## 4. Pinout & Wiring

### 4.1 Studi Kasus 01 — Wiring Potensiometer + LED

```
   ESP32 DevKit V1                Potensiometer
   ┌───────────────┐              ┌─────────┐
   │           3V3 ├──────────────┤ VCC     │
   │           GND ├─────────┐    │         │
   │                │        └────┤ GND     │
   │           D34 ├──────────────┤ SIG     │  (ADC1_CH6, input-only)
   │                │              └─────────┘
   │           D13 ├──────┐
   │                │     │  R 220Ω
   │                │     └──────┐
   │           GND  │            │
   │           (GND.2)───────────┤ LED (kuning) Cathode
   │                            │
   └───────────────┘            LED Anode ← dari R
```

**Catatan pin**:

- **GPIO 34** adalah pin **input-only** (ADC1_CH6). Tidak bisa dipakai output/PWM — aman untuk membaca sensor.
- **GPIO 13** mendukung output PWM via LEDC.

### 4.2 Studi Kasus 02 — Matrix Mapping Keypad

```
                    Col1    Col2    Col3    Col4      ESP32 GPIO
                    (32)    (33)    (25)    (26)
                     │       │       │       │
   Row1 (27)  ─── ●──1───●──2───●──3───●──A
   Row2 (14)  ─── ●──4───●──5───●──6───●──B
   Row3 (12)  ─── ●──7───●──8───●──9───●──C
   Row4 (13)  ─── ●──*───●──0───●──#───●──D

   LED merah ← GPIO 18 (R 1kΩ) ← indikator akses granted
```

**Tabel wiring keypad → ESP32** (via breadboard):

| Pin Keypad | GPIO ESP32 | Tipe   |
| ---------- | ---------- | ------ |
| R1         | 27         | Row    |
| R2         | 14         | Row    |
| R3         | 12         | Row    |
| R4         | 13         | Row    |
| C1         | 32         | Column |
| C2         | 33         | Column |
| C3         | 25         | Column |
| C4         | 26         | Column |

---

## 5. Alur Program — Studi Kasus 01 (Analog)

### 5.1 Boot Flow

```
   POWER ON
      │
      ▼
   setup()
      │
      ├─ 1. Serial.begin(115200)       ← baudrate untuk Serial Monitor
      ├─ 2. analogReadResolution(12)   ← ADC 12-bit (0..4095)
      ├─ 3. ledcAttach(PIN_LED, 5000, 8) ← hubungkan GPIO 13 ke LEDC
      │      └─ frekuensi 5 kHz, resolusi 8-bit
      └─ 4. Serial.println(banner)
      │
      ▼
   loop() (diulang selamanya)
```

### 5.2 Main Loop Flow (diulang tiap 100 ms)

```
   loop()
      │
      ├─ 1. analogRead(PIN_POT)           → int adc (0..4095)
      │
      ├─ 2. Konversi tegangan:
      │      tegangan = (adc / 4095.0) * 3.3   → float (0.00 .. 3.30 V)
      │
      ├─ 3. Mapping ke duty PWM:
      │      pwm = map(adc, 0, 4095, 0, 255)   → int (0..255)
      │
      ├─ 4. ledcWrite(PIN_LED, pwm)       → duty cycle PWM diterapkan ke LED
      │      └─ makin besar pwm, makin terang LED
      │
      ├─ 5. Serial.print(ADC / V / PWM)   → observasi via Serial Monitor
      │
      └─ 6. delay(100)                    → throttle ~10 Hz
```

### 5.3 Rantai Sinyal Analog → Cahaya

```
   PUTAR POT
      │
      ▼ (mekanik)
   V_wiper berubah (0..3.3 V)   ← analog kontinu
      │
      ▼ (ADC sampling @ ESP32)
   adc = (V / 3.3) * 4095       ← diskrit 12-bit (0..4095)
      │
      ▼ (matematika map())
   pwm = (adc / 4095) * 255     ← diskrit 8-bit (0..255)
      │
      ▼ (hardware LEDC)
   sinyal kotak PWM 5 kHz
   duty cycle = pwm / 255
      │
      ▼ (LED + mata manusia)
   KECERAHAN rata-rata berubah  ← analog persepsi
```

### 5.4 Konfigurasi PWM yang Dipakai

| Parameter       | Nilai   | Alasan                                           |
| --------------- | ------- | ------------------------------------------------ |
| `pwmFreq`       | 5000 Hz | Tinggi agar mata tidak melihat kedipan (>100 Hz) |
| `pwmRes`        | 8-bit   | 256 level kecerahan — cukup halus untuk mata     |
| `analogReadRes` | 12-bit  | 4096 level — presisi tinggi dari ADC hardware    |

---

## 6. Alur Program — Studi Kasus 02 (Digital)

### 6.1 Boot Flow

```
   POWER ON
      │
      ▼
   setup()
      │
      ├─ 1. Serial.begin(115200)
      ├─ 2. pinMode(PIN_LED, OUTPUT) + digitalWrite(PIN_LED, LOW)  ← safe default
      ├─ 3. (otomatis) Keypad library init saat objek dibuat
      └─ 4. Banner instruksi + hint PIN
      │
      ▼
   loop()
```

### 6.2 Main Loop Flow (event-driven polling)

```
   loop()
      │
      ├─ char key = keypad.getKey()   ← NON-BLOCKING
      │      │
      │      └─ return 0 (NO_KEY) jika tidak ada event → loop selesai, ulang
      │
      └─ if (key != 0):
            │
            ├─ Serial.print key
            │
            ├─ key == '*' ?
            │     └─ YA ─► reset inputPIN, LED OFF
            │
            ├─ key == '#' ?
            │     └─ YA ─► checkPIN() (manual submit)
            │
            ├─ inputPIN.length() < 4 ?
            │     ├─ YA ─► inputPIN += key (akumulasi digit)
            │     │        │
            │     │        └─ if length == 4 ──► checkPIN() (auto-submit)
            │     │
            │     └─ TIDAK ─► log "PIN sudah penuh"
            │
            └─ (selesai — loop kembali ke atas)
```

### 6.3 Matrix Scanning (di latar belakang library)

`` Library Keypad mengeksekusi tiap kali getKey() dipanggil:

Step 1: Set SEMUA row → OUTPUT LOW, SEMUA col → INPUT_PULLUP
Step 2: Loop tiap row N (0..3): - Aktifkan row N = LOW, row lain = HIGH (idle) - Baca SEMUA col - Col yang terbaca LOW = ada tombol ditekan di row N
Step 3: Mapping (row, col) → karakter via lookup table `keys[][]`

Contoh: tombol '5' ditekan.
Saat scan Row2 (GPIO 14 = LOW):
Col1 (32) HIGH ← tidak terhubung
Col2 (33) LOW ← terhubung via tombol '5'
Col3 (25) HIGH
Col4 (26) HIGH
Simpulan: (row=1, col=1) → keys[1][1] = '5'

```

### 6.4 Edge Detection

```

Library menyimpan state tombol sebelumnya.

State transition: IDLE ──► PRESSED → return karakter (SEKALI)
│
└─────► HELD (tidak return apa-apa)

Inilah "edge detection" — hanya trigger saat transisi IDLE→PRESSED,
tidak peduli berapa lama ditahan. Mencegah auto-repeat yang tidak diinginkan.

```

---

## 7. State Machine PIN (Studi Kasus 02)

### 7.1 State Diagram Buffer Input

```

┌───────────────────────────┐
│ IDLE (inputPIN == "") │ ◄── default state (boot / setelah reset/verifikasi)
│ panjang buffer = 0 │
└─────────────┬─────────────┘
│ digit ditekan (0-9, A-D)
▼
┌───────────────────────────┐
│ COLLECTING (1..3 digit) │ ── inputPIN += key
│ panjang 1, 2, atau 3 │ ── tampilkan progres di Serial
└─────────────┬─────────────┘
│ digit ke-4 ditekan (panjang jadi 4)
▼
┌───────────────────────────┐
│ VERIFY (auto-submit) │ ── checkPIN()
│ compare dengan "1234" │
└─────────────┬─────────────┘
│
┌────┴────┐
│ │
▼ ▼
┌───────────┐ ┌───────────┐
│ BENAR │ │ SALAH │
│ LED ON 3s │ │ LED mati │
└─────┬─────┘ └─────┬─────┘
│ │
└──────┬───────┘
│ inputPIN = "" (reset)
▼
kembali ke IDLE

````

### 7.2 Special Keys

| Key | Aksi                                                            |
| --- | --------------------------------------------------------------- |
| `*` | **Reset** — kosongkan buffer, LED OFF kapan saja                |
| `#` | **Submit manual** — panggil `checkPIN()` walau belum 4 digit    |
| `0`-`9`, `A`-`D` | Akumulasi digit sampai panjang 4, lalu auto-submit |

### 7.3 Tabel Lookup `keys[ROWS][COLS]`

```cpp
char keys[4][4] = {
  { '1', '2', '3', 'A' },   // Row 1
  { '4', '5', '6', 'B' },   // Row 2
  { '7', '8', '9', 'C' },   // Row 3
  { '*', '0', '#', 'D' }    // Row 4
};
````

> **WAJIB**: urutan di array harus PERSIS sama dengan label fisik keypad di Wokwi. Jika tidak, tombol yang ditekan tidak akan cocok dengan karakter yang terbaca.

---

## 8. Skenario End-to-End

### 8.1 Studi Kasus 01 — Demo Potensiometer

| Step | Aksi                            | Expected Result                                |
| ---- | ------------------------------- | ---------------------------------------------- | ------- | ----------------------------- |
| 1    | Start simulasi Wokwi            | Serial: `=== Demo Sensor + ADC + Aktuator ===` |
| 2    | Putar POT ke ujung kiri (0V)    | Serial: `ADC=0                                 | V=0.00V | PWM=0`, LED padam             |
| 3    | Putar POT ke tengah (1.65V)     | Serial: `ADC=2048                              | V=1.65V | PWM=128`, LED redup           |
| 4    | Putar POT ke ujung kanan (3.3V) | Serial: `ADC=4095                              | V=3.30V | PWM=255`, LED terang maksimal |
| 5    | Putar bolak-balik               | LED meredup/terang mengikuti secara real-time  |

### 8.2 Studi Kasus 02 — Demo Keypad PIN

| Step | Aksi                       | Expected Result                                              |
| ---- | -------------------------- | ------------------------------------------------------------ | ------------------------------ |
| 1    | Start simulasi Wokwi       | Serial: `Masukkan 4 digit PIN di keypad Wokwi`               |
| 2    | Klik keypad: 1 → 2 → 3 → 4 | Tiap digit tampil di Serial; LED merah menyala 3 detik       |
| 3    | Tunggu 3 detik             | LED otomatis mati; Serial: `Siap untuk input PIN berikutnya` |
| 4    | Klik keypad: 5 → 6 → 7 → 8 | Serial: `=> SALAH                                            | Akses ditolak. LED tetap mati` |
| 5    | Klik 1 → 2 → `*`           | Serial: `>> Input direset` (buffer kosong kembali)           |
| 6    | Klik 1 → 2 → 3 → 4 → 5     | Digit ke-5 diabaikan: `PIN sudah penuh. Tekan '#' atau '*'`  |

### 8.3 Failure Modes

| Skenario                        | Behavior                                                       |
| ------------------------------- | -------------------------------------------------------------- |
| PIN benar lalu tekan digit      | LED sudah mati setelah 3s, input baru mulai dari buffer kosong |
| Tekan `#` saat buffer kosong    | `checkPIN()` keluar early-return ("kosong, abaikan")           |
| Event `delay(3000)` berlangsung | `keypad.getKey()` tidak terbaca (BLOCKING). OK untuk demo      |
| ADC baca noise                  | Pembacaan tetap diskrit 0..4095 — PWM bisa "flicker" halus     |

---

## 9. Perbandingan 01 vs 02 (Analog vs Digital)

| Aspek                 | Studi Kasus 01 (Analog)                     | Studi Kasus 02 (Digital)               |
| --------------------- | ------------------------------------------- | -------------------------------------- |
| **Tipe sensor**       | Potensiometer (analog kontinu)              | Keypad matrix 4×4 (digital diskrit)    |
| **Tipe sinyal**       | Tegangan 0..3.3V (kontinu)                  | Keystroke event (diskrit)              |
| **Cara baca**         | `analogRead()` → ADC 12-bit                 | `keypad.getKey()` → edge detection     |
| **Resolusi data**     | 4096 level (0..4095)                        | 16 status (1 per tombol)               |
| **GPIO yang dipakai** | 2 (1 ADC + 1 PWM)                           | 9 (8 keypad + 1 LED)                   |
| **Aktuator**          | LED kuning via PWM (meredup/terang)         | LED merah via binary (ON/OFF)          |
| **API output**        | `ledcAttach` + `ledcWrite` (PWM)            | `pinMode` + `digitalWrite` (binary)    |
| **Pola loop**         | Polling periodik (100 ms)                   | Event-driven (saat ada key)            |
| **Library**           | (bawaan core)                               | `Keypad.h` (Chris--A)                  |
| **Use case nyata**    | Dimmer, volume, joystick, sensor intensitas | Pin ATM, access control, keypad menu   |
| **Komunikasi ke PC**  | Serial log nilai numerik                    | Serial log karakter + hasil verifikasi |

> **Insight**: Kedua studi kasus memperkenalkan dua sisi fundamental IoT edge device — sensor yang menghasilkan data kontinu (perlu ADC) vs diskrit (perlu debouncing/scanning). Aktuator pun juga dual: PWM (analog persepsi) vs binary (digital murni). Kombinasi keduanya = bekal untuk Topik 2 (MQTT) & Topik 3 (Firebase) di mana sensor + aktuator dihubungkan ke cloud.

---

## Lampiran: Referensi Cepat

### A. Konstanta Penting — Studi Kasus 01

```cpp
#define PIN_POT 34       // ADC1_CH6 (input-only)
#define PIN_LED 13        // LEDC PWM output
const int pwmFreq = 5000;  // 5 kHz
const int pwmRes  = 8;     // 8-bit (0..255)
// analogReadResolution(12);  // ADC 12-bit (0..4095)
```

### B. Konstanta Penting — Studi Kasus 02

```cpp
#define PIN_LED 18                // LED merah indikator
const String CORRECT_PIN = "1234"; // PIN rahasia
const int    PIN_LENGTH  = 4;      // panjang PIN
const byte ROWS = 4;
const byte COLS = 4;
byte rowPins[ROWS] = { 27, 14, 12, 13 };
byte colPins[COLS] = { 32, 33, 25, 26 };
```

### C. Pseudo-code Loop Utama

**Studi Kasus 01 (Analog):**

```python
while True:
    adc = read_adc(PIN_POT)             # 0..4095
    voltage = (adc / 4095.0) * 3.3      # float
    pwm = map(adc, 0, 4095, 0, 255)     # 0..255
    ledc_write(PIN_LED, pwm)
    print(adc, voltage, pwm)
    delay(100)                          # 10 Hz
```

**Studi Kasus 02 (Digital):**

```python
input_pin = ""
while True:
    key = keypad.get_key()              # non-blocking, 0 if none
    if key == 0:
        continue
    if key == '*':
        input_pin = ""; led_off()
    elif key == '#':
        verify(input_pin)
    elif len(input_pin) < 4:
        input_pin += key
        if len(input_pin) == 4:
            verify(input_pin)           # auto-submit
```

### D. Rumus Konversi Kunci

| Rumus                                   | Keterangan                  |
| --------------------------------------- | --------------------------- |
| `ADC = (V_in / 3.3) * 4095`             | Tegangan → nilai ADC 12-bit |
| `V = (adc / 4095.0) * 3.3`              | Nilai ADC → Tegangan (Volt) |
| `pwm = map(adc, 0, 4095, 0, 255)`       | ADC → duty cycle PWM 8-bit  |
| `duty_cycle = pwm / 255` (rentang 0..1) | Rasio ON sinyal PWM         |
