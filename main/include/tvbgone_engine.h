#ifndef TVBGONE_ENGINE_H
#define TVBGONE_ENGINE_H
#include <stdint.h>

// 1. TRIK SULAP MENGHILANGKAN ERROR AVR
#define PROGMEM 
 // Dikosongin, biar compiler ESP32 nge-skip kata ini
#define freq_to_timerval(x) (x) 
// Biarin angkanya tetep jadi Hz (misal 38400)

// 2. DEFINISI STRUCT IR CODE (Disesuaikan buat ESP32)
struct IrCode {
    uint16_t freq;            // Frekuensi (Hz), misal 38000
    uint8_t numpairs;         // Jumlah pasangan nyala/mati
    uint8_t bitcompression;   // Berapa bit per angka (biasanya 2 atau 4)
    const uint16_t *times;    // Tabel waktu (durasi kedip)
    uint8_t codes[];          // Array data yang udah dikompres (Flexible Array)
};

// Deklarasi fungsi mesin penerjemahnya (nanti kita buat di file .c)
extern void send_tvbgone_code(const struct IrCode *irCode);

#endif
