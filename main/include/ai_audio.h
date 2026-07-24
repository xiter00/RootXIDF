#ifndef AI_AUDIO_H
#define AI_AUDIO_H

#include "driver/i2s_std.h"

// ==========================================
// PIN WIRING UNTUK MAX98357A (SPEAKER OUTPUT)
// ==========================================
// Pake sisa pin aman di sisi kiri/bawah board
#define I2S_SPK_BCLK   1  // BCLK (Bit Clock)
#define I2S_SPK_LRC    2  // LRC (Left/Right Clock / WS)
#define I2S_SPK_DOUT   6  // DIN (Data IN ke MAX)

// ==========================================
// PIN WIRING UNTUK INMP441 (MIC INPUT)
// ==========================================
// Pake sisa pin aman yang jauh dari jalur USB/UART
#define I2S_MIC_SCK    7  // SCK
#define I2S_MIC_WS     14 // WS
#define I2S_MIC_SD     21 // SD 

// Handle untuk I2S (Output dan Input)
extern i2s_chan_handle_t tx_chan; // Handle buat Speaker
extern i2s_chan_handle_t rx_chan; // Handle buat Mic

// ==========================================
// DEKLARASI FUNGSI AI & AUDIO
// ==========================================
void init_i2s_audio(void);
void set_ai_audio_hardware(bool state);
void play_tts_audio(const uint8_t *audio_data, size_t audio_size);
void play_google_tts(const char *text);

void mulai_rekam_dan_stt(void);
void tanya_gemini(const char* pertanyaan_user);
void ai_audio_task(void *pvParameters);

#endif
