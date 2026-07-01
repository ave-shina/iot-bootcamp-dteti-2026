// Wajib di-include agar simbol Arduino (Serial, analogRead, map, delay,
// ledcAttach, ledcWrite, dll) tersedia saat file dipakai sebagai .cpp.
// Di .ino, include ini di-generate otomatis oleh Arduino IDE — tidak perlu
// ditulis manual. Tapi untuk kompatibilitas .ino & .cpp, letakkan tetap aman.
#include <Arduino.h>

/*
 * Bootcamp IoT - Topik 1 (Pertemuan Online #1)
 * Studi Kasus 1: SENSOR + ADC + AKTUATOR (PWM)
 *
 * Sensor  : Potensiometer (analog)  -> GPIO 34 (input-only, ADC1_CH6)
 * Aktuator: LED (PWM, meredup/terang) -> GPIO 13
 *
 * Konsep yang didemonstrasikan:
 *   1. Cara sensor analog menghasilkan tegangan 0-3.3V.
 *   2. Cara ADC di ESP32 mengubah tegangan itu jadi angka 0-4095 (12-bit).
 *   3. Cara angka ADC dipetakan ke sinyal PWM (0-255) untuk aktuator (LED).
 *
 * Flow: PUTAR POT -> ADC berubah -> PWM LED berubah -> LED meredup/terang
 *
 * Catatan teknis:
 *   - ESP32 Arduino Core 3.x mengganti API LEDC menjadi lebih sederhana:
 *       ledcAttach(pin, freq, resolution)
 *       ledcWrite(pin, duty)
 *     (Core 2.x lama menggunakan ledcSetup + ledcAttachPin + ledcWrite dengan channel.)
 */

/* =========================================================================
 * CARA KERJA: BAGAIMANA SINYAL ANALOG DAPAT MENGUBAH KECERAHAN LED?
 * =========================================================================
 *
 * Ada 4 tahapan rantai proses (Analog -> Digital -> Processing -> PWM):
 *
 * 1) SISI SENSOR (Analog) --------------------------------------------------
 *    Potensiometer adalah pembagi tegangan (voltage divider). Saat knop
 *    diputar, resistansi internal berubah, sehingga tegangan pada pin
 *    "wiper" (tengah) berubah LINIER dari 0V sampai 3.3V.
 *    Tegangan ini bersifat ANALOG (kontinu, nilai nyata, tak terbatas
 *    resolusinya) — mikrokontroler TIDAK bisa membacanya langsung.
 *
 * 2) ADC (Analog-to-Digital Converter) -------------------------------------
 *    Blok ADC di ESP32 "mengambil sampel" tegangan analog tersebut dan
 *    membandingkannya dengan VRef (3.3V). Hasilnya dibulatkan ke angka
 *    diskrit. Karena resolusi di-set 12-bit, rentangnya = 2^12 = 4096
 *    level (0..4095).
 *       Rumus:  ADC = (V_in / 3.3) * 4095
 *       Contoh: 0V   -> 0
 *               1.65V -> ~2048   (setengah)
 *               3.3V -> 4095
 *    Sekarang sinyal sudah berupa ANGKA di dalam program.
 *
 * 3) PEMROSESAN (Mapping) ---------------------------------------------------
 *    Nilai ADC 0..4095 itu lalu dipetakan ke skala duty cycle PWM 0..255:
 *       pwm = map(adc, 0, 4095, 0, 255)
 *    Tujuannya: menyederhanakan ke 8-bit karena PWM LED hanya butuh 256
 *    level kecerahan — sudah cukup halus untuk mata manusia.
 *
 * 4) SISI AKTUATOR (PWM) ---------------------------------------------------
 *    LED tidak bisa "diredupkan" dengan menurunkan tegangan secara analog
 *    dari GPIO (pin digital hanya HIGH=3.3V atau LOW=0V). Solusinya:
 *    nyalakan dan matikan LED SANGAT CEPAT (ribuan kali per detik).
 *    Rasio ON/(ON+OFF) disebut DUTY CYCLE.
 *       - Duty 100% -> LED selalu ON  -> TERANG MAKSIMAL
 *       - Duty 50%  -> 50% ON         -> SETENGAH TERANG
 *       - Duty 0%   -> selalu OFF     -> PADAM
 *    Karena frekuensi tinggi (5 kHz), mata manusia merasakannya sebagai
 *    kecerahan rata-rata, bukan kedipan. Inilah prinsip PWM (Pulse Width
 *    Modulation). Makin besar nilai pwm (duty), makin lama LED ON per
 *    periode, sehingga makin TERANG.
 *
 * RANGKAIAN ALUR:
 *   Putar POT -> V_in berubah -> ADC berubah -> pwm = map(adc) ->
 *   ledcWrite(pwm) -> duty cycle PWM berubah -> LED meredup/terang.
 * =========================================================================
 */

// ---------- Definisi Pin ----------
#define PIN_POT 34   // Makro: ganti setiap teks "PIN_POT" dengan 34 saat kompilasi.
                     // GPIO 34 = ADC1_CH6, pin INPUT-ONLY (tidak bisa output/PWM),
                     // aman dipakai untuk membaca tegangan analog dari potensiometer.
#define PIN_LED 13   // Makro: ganti "PIN_LED" dengan 13.
                     // GPIO 13 mampu output sinyal PWM (LEDC) untuk mengatur kecerahan LED.

// ---------- Konfigurasi Parameter PWM ----------
const int pwmFreq = 5000;   // Variabel konstan (integer): frekuensi PWM = 5000 Hz (5 kHz).
                            // Frekuensi = berapa kali sinyal PWM berosilasi tiap detik.
                            // 5 kHz cukup tinggi agar mata tidak melihat kedipan LED.
const int pwmRes  = 8;      // Resolusi PWM = 8-bit -> duty cycle punya 2^8 = 256 level (0..255).
                            // 0 = LED padam, 255 = LED menyala penuh.

// =========================================================================
// setup() dijalankan SATU KALI saat board boot / reset.
// Berisi inisialisasi: Serial, ADC, dan konfigurasi PWM pada pin LED.
// =========================================================================
void setup() {
  Serial.begin(115200);                        // Mulai komunikasi Serial ke PC pada baudrate 115200.
                                               // Digunakan untuk mengirim teks (debug) ke Serial Monitor.
  analogReadResolution(12);                    // Set resolusi ADC = 12-bit.
                                               // Artinya analogRead() akan mengembalikan nilai 0..4095
                                               // (bukan default 10-bit Arduino lama yang hanya 0..1023).

  // Setup PWM via LEDC (ESP32 Core 3.x API)
  ledcAttach(PIN_LED, pwmFreq, pwmRes);        // Hubungkan pin LED ke hardware LEDC PWM generator
                                               // dengan frekuensi (5000 Hz) dan resolusi (8-bit)
                                               // yang sudah didefinisikan di atas.
                                               // Sejak ESP32 Core 3.x, API disederhanakan jadi
                                               // cukup ledcAttach(pin, freq, res) — tanpa channel manual.

  Serial.println("=== Demo Sensor + ADC + Aktuator (LEDC PWM - Core 3.x) ===");
                                               // Cetak judul sekali sebagai penanda program sudah start.
}

// =========================================================================
// loop() dijalankan BERULANG-KALI selamanya (setelah setup selesai).
// Di sini terjadi pembacaan sensor, pemetaan, dan penulisan PWM tiap 100 ms.
// =========================================================================
void loop() {
  // 1. Baca sensor (ADC) ----------------------------------------------
  int adc = analogRead(PIN_POT);               // Baca tegangan analog di PIN_POT melalui ADC.
                                               // Hasilnya bilangan bulat 0..4095 (karena resolusi 12-bit).
                                               // adc=0 -> 0V, adc=4095 -> 3.3V.
  float tegangan = (adc / 4095.0) * 3.3;       // Konversi nilai ADC mentah menjadi satuan Volt.
                                               // Dibagi 4095.0 (Bukan 4095) agar pembagian PECAHAN
                                               // (tipe float), lalu dikalikan VRef 3.3V.
                                               // Hasil: 0.00 .. 3.30 Volt.

  // 2. Pemrosesan: petakan ADC (0-4095) ke duty PWM (0-255) -----------
  int pwm = map(adc, 0, 4095, 0, 255);         // map() melakukan interpolasi linier:
                                               // dari rentang [0..4095] ke rentang [0..255].
                                               // Contoh: adc=0 -> pwm=0 (padam),
                                               //         adc=2048 -> pwm=128 (setengah),
                                               //         adc=4095 -> pwm=255 (penuh).
                                               // Variabel 'pwm' ini adalah duty cycle yang akan
                                               // ditulis ke LED.

  // 3. Aktuator: tulis PWM ke LED via LEDC ----------------------------
  ledcWrite(PIN_LED, pwm);                     // Set duty cycle PWM pada PIN_LED = nilai 'pwm'.
                                               // Hardware LEDC akan men-generate sinyal kotak
                                               // dengan rasio HIGH = pwm/255.
                                               // Makin besar 'pwm', makin lama LED ON per periode
                                               // -> makin terang. Mata melihatnya sebagai
                                               // kecerahan rata-rata.

  // 4. Tampilkan ke Serial Monitor untuk observasi --------------------
  Serial.print("ADC=");                        // Cetak label "ADC=" tanpa pindah baris.
  Serial.print(adc);                           // Cetak nilai ADC mentah (0..4095).
  Serial.print("  |  V=");                     // Cetak pemisah + label "V=".
  Serial.print(tegangan, 2);                   // Cetak tegangan dengan 2 angka di belakang koma.
  Serial.print("V  |  PWM=");                  // Cetak satuan "V" + pemisah + label "PWM=".
  Serial.println(pwm);                         // Cetak nilai pwm (0..255) lalu pindah baris baru.

  delay(100);                                  // Tunda 100 ms sebelum loop berikutnya.
                                               // Tujuan: pembacaan stabil (~10x per detik)
                                               // dan Serial Monitor tidak terlalu cepat scroll.
}
