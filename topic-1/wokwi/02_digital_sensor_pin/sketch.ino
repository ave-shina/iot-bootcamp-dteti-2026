/*
 * Bootcamp IoT - Topik 1 (Pertemuan Online #1)
 * Studi Kasus 2: SENSOR DIGITAL (Keypad matrix) + AKTUATOR (LED)
 *
 * Sensor   : Keypad 4x4 (matrix scanning)
 * Wiring   : ESP32 ─── breadboard ─── keypad
 * Aktuator : LED hijau (nyala saat PIN benar)
 *
 * Konsep yang didemokan:
 *   1. Sensor DIGITAL = data berupa keystroke diskrit (bukan kontinu).
 *   2. Matrix scanning: 4 rows × 4 cols = 16 tombol cukup 8 pin.
 *   3. Breadboard sebagai hub wiring (wiremapping fisik yang rapi).
 *   4. Mikrokontroler sebagai "otak" → string matching PIN.
 *   5. Aktuator merespon hasil logika: LED menyala jika PIN benar.
 *
 * Flow: TEKAN TOMBOL → kumpulkan 4 digit → bandingkan → LED ON/OFF
 *
 * Cara interaksi di Wokwi:
 *   - Klik keypad: 1 → 2 → 3 → 4   → LED HIJAU nyala 3 detik
 *   - Klik kombinasi lain           → LED tetap mati (akses ditolak)
 *   - Tekan '*' untuk reset input
 *
 * PIN yang benar: 1234
 */

/* =========================================================================
 * CARA KERJA: BAGAIMANA KEYPAD MATRIX 4x4 DIKETAHUI TOMBOL YANG DITEKAN?
 * =========================================================================
 *
 * Ini studi kasus SENSOR DIGITAL — berbeda dengan potensiometer yang analog
 * kontinu. Keypad mengirim "events" diskrit: tidak ada tombol ditekan, atau
 * ada satu tombol ditekan. Tidak ada nilai tengah.
 *
 * --- Tantangan wiring ---------------------------------------------------
 * Keypad 4x4 punya 16 tombol. Kalau setiap tombol butuh 1 pin, perlu 16
 * pin GPIO — boros! Solusinya: susun tombol dalam MATRIX 4 baris × 4 kolom.
 * Dengan matrix, cukup butuh 4 + 4 = 8 pin (hemang 50%).
 *
 * --- Struktur internal matrix -------------------------------------------
 * Tiap tombol adalah push-button yang menghubungkan SATU BARIS dengan
 * SATU KOLOM saat ditekan. Visualisasi:
 *
 *           Col1   Col2   Col3   Col4
 *          (GPIO   (GPIO  (GPIO  (GPIO
 *           32)    33)    25)    26)
 *            │      │      │      │
 *   Row1     ●──1───●──2───●──3───●──A     <- (GPIO 27)
 *   (27)     │      │      │      │
 *   Row2     ●──4───●──5───●──6───●──B     <- (GPIO 14)
 *   (14)     │      │      │      │
 *   Row3     ●──7───●──8───●9────●──C     <- (GPIO 12)
 *   (12)     │      │      │      │
 *   Row4     ●──*───●──0───●──#───●──D     <- (GPIO 13)
 *   (13)
 *
 *   Tiap "●" adalah titik potensial connection.
 *   Saat tombol '5' ditekan, Row2 terhubung ke Col2.
 *
 * --- Cara mikrokontroler membaca: MATRIX SCANNING -----------------------
 * Library Keypad melakukan ini berulang kali di latar belakang:
 *
 *   Step 1: Set SEMUA row pins menjadi OUTPUT LOW, semua col pins INPUT_PULLUP.
 *   Step 2: Loop melalui setiap row satu per satu:
 *             - Aktifkan row N (jadikan OUTPUT LOW), row lain HIGH (idle).
 *             - Baca SEMUA col pins.
 *             - Col yang terbaca LOW = ada tombol ditekan di row N tsb.
 *   Step 3: Dari kombinasi (row, col), ketahui tombol mana yang ditekan.
 *
 *   Contoh: tombol '5' ditekan.
 *     Saat library scan Row2 (GPIO 14 = LOW):
 *       - Col1 (32) HIGH (tidak terhubung)
 *       - Col2 (33) LOW  ← terhubung via tombol '5'!
 *       - Col3 (25) HIGH
 *       - Col4 (26) HIGH
 *     Library simpulkan: (row=1, col=1) -> lihat tabel keys -> '5'
 *
 * --- Event-driven --------------------------------------------------------
 * Library menyimpan state sebelumnya. Saat tombol baru ditekan (transisi
 * dari "tidak ditekan" ke "ditekan"), ia mengembalikan karakter lewat
 * keypad.getKey(). Ini disebut "edge detection" — hanya trigger sekali
 * per penekanan, tidak peduli berapa lama ditahan.
 *
 * --- Beda dengan analog (potensiometer) ----------------------------------
 *   Analog (sketch 01): baca kontinu tiap 100 ms, dapat angka 0..4095.
 *   Digital (sketch 02): baca event "ada tombol ditekan", dapat karakter.
 *
 * =========================================================================
 */

#include <Keypad.h>   // sertakan library Keypad agar class Keypad dan fungsi
                      // makeKeymap() tersedia. Library ini mengabstraksi
                      // matrix scanning sehingga kita cukup panggil getKey().

// === Konfigurasi Keypad 4x4 ===
const byte ROWS = 4;  // Konstanta bertipe byte (unsigned 8-bit, 0..255).
                      // Menyatakan jumlah BARIS keypad = 4.
const byte COLS = 4;  // Konstanta jumlah KOLOM keypad = 4.

// Tabel lookup: pemetaan (row, col) -> karakter yang ditampilkan di tombol.
// Harus PERSIS sama dengan fisik keypad (cek urutan label di Wokwi/datasheet).
// Indeks pertama = baris, indeks kedua = kolom.
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },   // Row 1 (atas): 1 2 3 A
  { '4', '5', '6', 'B' },   // Row 2      : 4 5 6 B
  { '7', '8', '9', 'C' },   // Row 3      : 7 8 9 C
  { '*', '0', '#', 'D' }    // Row 4 (bawah): * 0 # D
};

// Wiring via breadboard:
//   Keypad R1-R4 -> ESP32 GPIO 27, 14, 12, 13
//   Keypad C1-C4 -> ESP32 GPIO 32, 33, 25, 26
byte rowPins[ROWS] = { 27, 14, 12, 13 };  // Array GPIO yang terhubung ke
                                          // pin ROW keypad (R1..R4).
                                          // Index 0 = R1 -> GPIO 27, dst.
byte colPins[COLS] = { 32, 33, 25, 26 };  // Array GPIO yang terhubung ke
                                          // pin COLUMN keypad (C1..C4).
                                          // Index 0 = C1 -> GPIO 32, dst.

// Instansiasi objek Keypad bernama 'keypad'.
// - makeKeymap(keys)        : ubah array keys jadi struktur internal library.
// - rowPins / colPins       : array pin fisik untuk scanning.
// - ROWS / COLS             : dimensi matrix.
// Setelah baris ini, 'keypad' siap dipakai untuk membaca tombol.
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// === Aktuator ===
#define PIN_LED 18          // Makro: ganti teks "PIN_LED" dengan 18.
                            // GPIO 18 = output ke LED hijau sebagai indikator
                            // akses diberikan (PIN benar).

// === PIN tersimpan ===
const String CORRECT_PIN = "1234";  // PIN rahasia yang benar (4 digit).
                                    // String immutable, disimpan di flash.
const int    PIN_LENGTH  = 4;       // Panjang PIN yang diharapkan = 4 digit.
String inputPIN = "";               // Buffer untuk menyimpan digit yang
                                    // sudah diketik user saat ini.
                                    // Di-reset setelah verifikasi atau '*'.

// =========================================================================
// setup() dijalankan SATU KALI saat boot.
// Tujuan: inisialisasi Serial, mode pin LED, dan banner instruksi.
// =========================================================================
void setup() {
  Serial.begin(115200);             // Mulai komunikasi Serial @115200 baud
                                    // untuk debugging ke Serial Monitor.
  pinMode(PIN_LED, OUTPUT);         // Set GPIO 18 sebagai OUTPUT agar bisa
                                    // menyalakan/mematikan LED.
  digitalWrite(PIN_LED, LOW);       // Pastikan LED dalam kondisi PADAM di
                                    // awal (default state yang aman).

  Serial.println();                 // Cetak baris kosong sebagai pemisah.
  Serial.println("============================================");
  Serial.println("  Demo Sensor Digital: Keypad PIN + LED");
  Serial.println("============================================");   // Banner judul.
  Serial.println("Masukkan 4 digit PIN di keypad Wokwi.");    // Instruksi user.
  Serial.println("PIN yang benar: 1 2 3 4  (LED hijau akan nyala)");
  Serial.println("Tekan '*' untuk reset.");
  Serial.println();
}

// =========================================================================
// loop() dijalankan BERULANG-KALI selamanya.
// Bertugas: polling keypad -> akumulasi digit -> dispatch ke checkPIN().
// =========================================================================
void loop() {
  char key = keypad.getKey();   // Cek apakah ada tombol baru ditekan.
                                // Mengembalikan char (karakter) jika ada
                                // event penekanan, atau 0 (NO_KEY) jika tidak.
                                // NON-BLOCKING: langsung return walau tidak
                                // ada tombol — penting agar loop tetap responsif.

  if (key) {                    // 'if (key)' true berarti key != 0, ada event.
    Serial.print("Tombol ditekan: ");
    Serial.println(key);        // Log karakter yang ditekan (untuk debugging).

    if (key == '*') {
      // Reset input -----------------------------------------------
      inputPIN = "";                     // Kosongkan buffer input.
      digitalWrite(PIN_LED, LOW);        // Matikan LED (jika sebelumnya nyala).
      Serial.println(">> Input direset.\n");
    } else if (key == '#') {
      // Submit manual ---------------------------------------------
      checkPIN();                        // Panggil fungsi verifikasi
                                          // sekalipun belum 4 digit (memungkinkan
                                          // submit awal — walau akan ditolak
                                          // karena panjang tidak cocok).
    } else if (inputPIN.length() < PIN_LENGTH) {
      // Akumulasi digit biasa (0-9, A-D) --------------------------
      inputPIN += key;                   // Tambahkan karakter ke buffer (concat).
      Serial.print("   Input sementara: ");
      Serial.println(inputPIN);          // Tampilkan progres input user.

      // Auto-submit saat sudah 4 digit
      if (inputPIN.length() == PIN_LENGTH) {  // Kalau buffer sudah 4 karakter,
        checkPIN();                           // otomatis verifikasi tanpa perlu '#'.
      }
    } else {
      // Buffer sudah penuh tapi user masih nekan digit -------------
      Serial.println("   PIN sudah penuh. Tekan '#' atau '*'.");
      // Abaikan keystroke tambahan; user harus submit/reset dulu.
    }
  }
}

// =========================================================================
// checkPIN() — fungsi verifikasi PIN.
// Membandingkan inputPIN dengan CORRECT_PIN, lalu mengendalikan aktuator (LED).
// =========================================================================
void checkPIN() {
  Serial.print(">> Verifikasi: ");
  Serial.print(inputPIN);              // Tampilkan PIN yang akan diperiksa.

  if (inputPIN.length() == 0) {        // Kalau user tekan '#' tanpa input apa-apa,
    Serial.println(" (kosong, abaikan)");
    return;                            // keluar dari fungsi tanpa melanjutkan.
                                       // 'return' di void = keluar saja.
  }

  if (inputPIN == CORRECT_PIN) {       // Bandingkan string secara eksak (case-sensitive).
                                       // Operator == pada String membandingkan isi, bukan alamat.
    Serial.println("  => BENAR | Akses diberikan. LED MENYALA.");
    digitalWrite(PIN_LED, HIGH);       // Aktuator: NYALAKAN LED hijau.
    delay(3000);                       // Tahan kondisi ON selama 3000 ms (3 detik).
                                       // Catatan: delay() bersifat BLOCKING — selama
                                       // ini keypad tidak terbaca. OK untuk demo singkat.
    digitalWrite(PIN_LED, LOW);        // Matikan LED setelah 3 detik.
  } else {
    Serial.println("  => SALAH | Akses ditolak. LED tetap mati.");
    // (opsional: blink LED 2x untuk indikasi salah)   // TODO visual feedback untuk PIN salah.
  }

  inputPIN = "";                       // Reset buffer untuk PIN berikutnya.
  Serial.println("Siap untuk input PIN berikutnya.\n");
}
