#ifndef AI_AUDIO_H
#define AI_AUDIO_H

#include "driver/i2s_std.h"

// ==========================================
// PIN WIRING UNTUK MAX98357A (SPEAKER OUTPUT)
// ==========================================
#define I2S_SPK_BCLK   4  // BCLK (Bit Clock)
#define I2S_SPK_LRC    5  // LRC (Left/Right Clock / WS)
#define I2S_SPK_DOUT   6  // DIN (Data IN ke MAX)

// ==========================================
// PIN WIRING UNTUK INMP441 (MIC INPUT)
// ==========================================
#define I2S_MIC_SCK    19 // SCK (Serial Clock)
#define I2S_MIC_WS     20 // WS (Word Select / L/R Clock)
#define I2S_MIC_SD     21 // SD (Serial Data Out dari Mic)

// Handle untuk I2S (Output dan Input)
extern i2s_chan_handle_t tx_chan; // Handle buat Speaker
extern i2s_chan_handle_t rx_chan; // Handle buat Mic
// Fungsi utama AI

void tanya_gemini(const char* pertanyaan_user);
// Fungsi buat ngerekam dari Mic dan ubah jadi teks
void mulai_rekam_dan_stt(void);
// Task FreeRTOS buat nguping terus-terusan
void ai_audio_task(void *pvParameters);

// Deklarasi Fungsi
void init_i2s_audio(void);
void play_tts_audio(const uint8_t *audio_data, size_t audio_size);
// Fungsi buat matiin/hidupin hardware audio
void set_ai_audio_hardware(bool state);
// Fungsi buat nyuruh ESP32 ngomong
void play_google_tts(const char *text);

#endif
