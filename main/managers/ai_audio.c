#include "globals.h"
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "ai_audio.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include <ctype.h>
#include <string.h>
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "minimp3.h"
#include "esp_crt_bundle.h"
#include <math.h>
#include "esp_timer.h"

// Potong berapa ms dari belakang audio TTS (watermark). Bisa diatur sekecil apapun,
// misal 300 buat 0.3 detik, 100 buat 0.1 detik.
#define TTS_CUT_MS 450

// Ganti sesuai domain Worker gabungan (STT + Gemini + TTS jadi satu) punya lu
#define ORCA_BRAIN_URL   "https://ai-brain.andyxd1955.workers.dev"
#define ORCA_AUTH_TOKEN  "orca-secret-123"

// === VAD (voice activity detection) tuning ===
// Sesuaikan angka RMS ini kalau kepicu kesenggol noise, atau kepotong kecepetan
#define VAD_CHUNK_MS         100   // ukuran potongan yang dicek tiap kali (ms)
#define VAD_START_RMS        700   // RMS minimal buat dianggap "mulai ngomong"
#define VAD_STOP_RMS         400   // RMS di bawah ini dianggap "diem"
#define VAD_SILENCE_HANG_MS  700   // berapa lama diem berturut-turut sebelum dianggap "selesai ngomong"
#define VAD_MAX_RECORD_SEC   8     // batas maksimal durasi rekam (jaga-jaga biar gak infinite)

static const char *TAG = "AI_AUDIO";
extern bool requireWakeWord;

i2s_chan_handle_t tx_chan = NULL;
i2s_chan_handle_t rx_chan = NULL;

// speaker_busy: true selagi audio TTS lagi diputer. Mic gak akan mulai dengerin
// sampe ini balik false, biar gak nangkep suara sendiri (feedback/echo).
static volatile bool speaker_busy = false;

// ============================================================
// HELPER: WRITE ALL
// ============================================================
static bool http_write_all(esp_http_client_handle_t client, const char *data, int len) {
    int written = 0;
    while (written < len) {
        int w = esp_http_client_write(client, data + written, len - written);
        if (w <= 0) {
            ESP_LOGE(TAG, "Write GAGAL byte %d/%d", written, len);
            return false;
        }
        written += w;
    }
    return true;
}

// ============================================================
// HELPER: URL DECODE (buat baca header X-Ucapan/X-Teks dari Worker)
// ============================================================
static void url_decode(char *dst, const char *src, size_t dst_size) {
    if (!dst || !src || dst_size == 0) return;
    size_t di = 0;
    for (size_t i = 0; src[i] && di < dst_size - 1; i++) {
        if (src[i] == '%' && isxdigit((unsigned char)src[i+1]) && isxdigit((unsigned char)src[i+2])) {
            char hex[3] = { src[i+1], src[i+2], 0 };
            dst[di++] = (char) strtol(hex, NULL, 16);
            i += 2;
        } else if (src[i] == '+') {
            dst[di++] = ' ';
        } else {
            dst[di++] = src[i];
        }
    }
    dst[di] = '\0';
}

// ============================================================
// I2S INIT
// ============================================================
void init_i2s_audio(void) {
    ESP_LOGI(TAG, "Init I2S Audio...");

    // TX - SPEAKER (MAX98357A: wajib format I2S/Philips standar, bukan MSB)
    i2s_chan_config_t tx_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    tx_cfg.dma_desc_num = 12;   // dari default ~6, nambah bantalan biar gak underrun/pecah
    tx_cfg.dma_frame_num = 480; // dari default ~240
    ESP_ERROR_CHECK(i2s_new_channel(&tx_cfg, &tx_chan, NULL));
    i2s_std_config_t tx_std = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, .bclk = I2S_SPK_BCLK,
            .ws   = I2S_SPK_LRC,     .dout = I2S_SPK_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { false, false, false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &tx_std));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    // RX - MIC (stereo, ambil slot kiri)
    i2s_std_slot_config_t mic_slot = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
    mic_slot.slot_mask = I2S_STD_SLOT_LEFT;

    i2s_chan_config_t rx_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&rx_cfg, NULL, &rx_chan));
    i2s_std_config_t rx_std = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = mic_slot,
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, .bclk = I2S_MIC_SCK,
            .ws   = I2S_MIC_WS,      .dout = I2S_GPIO_UNUSED,
            .din  = I2S_MIC_SD,
            .invert_flags = { false, false, false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &rx_std));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
    ESP_LOGI(TAG, "I2S OK.");
}

void set_ai_audio_hardware(bool state) {
    if (state) {
        i2s_channel_enable(tx_chan);
        i2s_channel_enable(rx_chan);
        ESP_LOGI(TAG, "Audio HW: ON");
    } else {
        i2s_channel_disable(tx_chan);
        i2s_channel_disable(rx_chan);
        ESP_LOGI(TAG, "Audio HW: OFF");
    }
}

// ============================================================
// DECODE + PLAY MP3 (dipanggil setelah audio dari Worker berhasil didownload)
// ============================================================
static void decode_and_play_mp3(uint8_t *mp3_buf, int total) {
    int cut_bytes = (TTS_CUT_MS * 16000) / 1000;
    int play_len = total - cut_bytes;
    if (play_len <= 0) play_len = total;

    mp3dec_t mp3d; mp3dec_init(&mp3d);
    mp3dec_frame_info_t fi;
    int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    int pos = 0;
    bool clk_set = false;
    int current_hz = 0;

    // nyalain speaker cuma pas mau ngomong, biar gak nangkep noise pas idle ("tek tek tek")
    speaker_busy = true;
    i2s_channel_enable(tx_chan);

    while (pos < play_len) {
        int s = mp3dec_decode_frame(&mp3d, mp3_buf+pos, play_len-pos, pcm, &fi);
        if (s > 0) {
            if (!clk_set || fi.hz != current_hz) {
                i2s_std_clk_config_t new_clk = I2S_STD_CLK_DEFAULT_CONFIG(fi.hz);
                i2s_channel_disable(tx_chan);
                i2s_channel_reconfig_std_clock(tx_chan, &new_clk);
                i2s_channel_enable(tx_chan);
                current_hz = fi.hz;
                clk_set = true;
            }

            // fade-out kalau ini frame terakhir, biar gak "nembak"/pop pas berhenti
            if (pos + fi.frame_bytes >= play_len) {
                int total_samples = s * fi.channels;
                int fade_len = total_samples < 200 ? total_samples : 200;
                for (int i = 0; i < fade_len; i++) {
                    float g = 1.0f - ((float)i / fade_len);
                    int idx = total_samples - fade_len + i;
                    pcm[idx] = (int16_t)(pcm[idx] * g);
                }
            }

            size_t bw;
            i2s_channel_write(tx_chan, pcm, s * fi.channels * sizeof(int16_t), &bw, 2000);
        }
        if (fi.frame_bytes > 0) pos += fi.frame_bytes;
        else break;
    }

    // tunggu DMA beneran abis ngeluarin sisa buffer sebelum dimatiin,
    // biar gak ada audio lama yang "nyangkut" dan kebawa muter pas ngomong berikutnya
    vTaskDelay(pdMS_TO_TICKS(300));
    i2s_channel_disable(tx_chan);
    speaker_busy = false;
    heap_caps_free(mp3_buf);
}

// ============================================================
// WAV HEADER
// ============================================================
void generate_wav_header(char *h, uint32_t dataSize, uint32_t sr) {
    uint32_t fs = dataSize + 36, br = sr * 2;
    uint8_t hdr[44] = {
        'R','I','F','F',
        fs&0xff,(fs>>8)&0xff,(fs>>16)&0xff,(fs>>24)&0xff,
        'W','A','V','E','f','m','t',' ',
        16,0,0,0, 1,0, 1,0,
        sr&0xff,(sr>>8)&0xff,(sr>>16)&0xff,(sr>>24)&0xff,
        br&0xff,(br>>8)&0xff,(br>>16)&0xff,(br>>24)&0xff,
        2,0, 16,0,
        'd','a','t','a',
        dataSize&0xff,(dataSize>>8)&0xff,(dataSize>>16)&0xff,(dataSize>>24)&0xff
    };
    memcpy(h, hdr, 44);
}

// ============================================================
// REKAM + KIRIM KE ORCA-BRAIN (Worker gabungan: Groq STT + Gemini + FreeTTS)
// ============================================================
void rekam_dan_proses(void) {
    const int chunk_samples = 1600;              // 100ms @ 16kHz
    const int chunk_bytes   = chunk_samples * 4;  // raw 32-bit
    const int max_samples   = 16000 * VAD_MAX_RECORD_SEC;
    const int max_bytes     = max_samples * 4;

    int32_t *raw = heap_caps_malloc(max_bytes, MALLOC_CAP_SPIRAM);
    int16_t *pcm = heap_caps_malloc(max_samples * 2, MALLOC_CAP_SPIRAM);
    if (!raw || !pcm) {
        ESP_LOGE(TAG, "PSRAM habis!");
        free(raw); free(pcm); return;
    }

    // jangan mulai dengerin sampe speaker beneran mati, biar gak nangkep suara sendiri
    while (speaker_busy) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    int32_t chunk_buf[1600];
    size_t bytes_read = 0;

    // buang 1 chunk pertama (biasanya noise transient pas mic baru aktif)
    i2s_channel_read(rx_chan, chunk_buf, chunk_bytes, &bytes_read, 1000);

    // ring buffer 200ms terakhir, biar awal kata gak kepotong pas VAD baru trigger
    int32_t preroll[2][1600];
    bool preroll_filled[2] = { false, false };
    int preroll_idx = 0;

    bool triggered = false;
    int silence_ms = 0;
    uint32_t total_samples = 0;

    while (total_samples < (uint32_t)max_samples) {
        if (i2s_channel_read(rx_chan, chunk_buf, chunk_bytes, &bytes_read, 1000) != ESP_OK) break;
        int got = bytes_read / 4;
        if (got <= 0) continue;

        float sum = 0;
        for (int i = 0; i < got; i++) {
            int32_t v = chunk_buf[i] >> 13;
            if (v >  32767) v =  32767;
            if (v < -32768) v = -32768;
            sum += (float)v * v;
        }
        float chunk_rms = sqrtf(sum / got);

        if (!triggered) {
            memcpy(preroll[preroll_idx], chunk_buf, got * 4);
            preroll_filled[preroll_idx] = true;
            preroll_idx = (preroll_idx + 1) % 2;

            if (chunk_rms >= VAD_START_RMS) {
                ESP_LOGI(TAG, "Suara terdeteksi (RMS %.1f), mulai rekam...", chunk_rms);
                triggered = true;
                silence_ms = 0;

                // masukin 200ms pre-roll dulu (urut dari yang paling lama), biar awal kata gak ilang
                int order[2] = { preroll_idx, (preroll_idx + 1) % 2 };
                for (int k = 0; k < 2; k++) {
                    int idx = order[k];
                    if (preroll_filled[idx] && total_samples + 1600 <= (uint32_t)max_samples) {
                        memcpy(raw + total_samples, preroll[idx], 1600 * 4);
                        total_samples += 1600;
                    }
                }
                if (total_samples + got <= (uint32_t)max_samples) {
                    memcpy(raw + total_samples, chunk_buf, got * 4);
                    total_samples += got;
                }
            }
            continue;
        }

        if (total_samples + got > (uint32_t)max_samples) {
            got = max_samples - total_samples;
            if (got <= 0) break;
        }
        memcpy(raw + total_samples, chunk_buf, got * 4);
        total_samples += got;

        if (chunk_rms < VAD_STOP_RMS) {
            silence_ms += VAD_CHUNK_MS;
            if (silence_ms >= VAD_SILENCE_HANG_MS) {
                ESP_LOGI(TAG, "Diem %dms, berhenti rekam.", silence_ms);
                break;
            }
        } else {
            silence_ms = 0;
        }
    }

    if (!triggered || total_samples < (uint32_t)chunk_samples) {
        ESP_LOGI(TAG, "Gak ada suara terdeteksi, skip.");
        free(raw); free(pcm);
        return;
    }

    // === KONVERSI 32->16 BIT ===
    int n_samples = total_samples;
    for (int i = 0; i < n_samples; i++) {
        int32_t v = raw[i] >> 13;
        if (v >  32767) v =  32767;
        if (v < -32768) v = -32768;
        pcm[i] = (int16_t)v;
    }
    size_t pcm_bytes = n_samples * 2;
    free(raw);

    ESP_LOGI(TAG, "PCM: %d bytes (%.1f detik) | s[0]=%d s[100]=%d",
        (int)pcm_bytes, n_samples / 16000.0f, pcm[0], pcm[100 < n_samples ? 100 : 0]);

    // === CEK RMS KESELURUHAN (jaga-jaga) ===
    float rms = 0;
    for (int i = 0; i < n_samples; i++) rms += (float)pcm[i] * pcm[i];
    rms = sqrtf(rms / n_samples);
    ESP_LOGI(TAG, "RMS: %.1f", rms);

    if (rms < 500) {
        ESP_LOGI(TAG, "Sepi, skip.");
        free(pcm); return;
    }

    // === WAV HEADER ===
    char wav_hdr[44];
    generate_wav_header(wav_hdr, pcm_bytes, 16000);
    uint32_t payload_len = 44 + pcm_bytes;
    ESP_LOGI(TAG, "Payload: %d bytes", (int)payload_len);

    for (int attempt = 1; attempt <= 2; attempt++) {
        if (attempt > 1) ESP_LOGW(TAG, "Attempt ke-%d ke Orca-Brain (percobaan sebelumnya gagal/kepotong)", attempt);

        esp_http_client_config_t cfg = {
            .url = ORCA_BRAIN_URL,
            .method = HTTP_METHOD_POST,
            .timeout_ms = 60000,
            .buffer_size = 8192,
            .buffer_size_tx = 2048,
            .keep_alive_enable = true,
            .keep_alive_idle = 5,
            .keep_alive_interval = 5,
            .keep_alive_count = 3,
        };
        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        esp_http_client_set_header(client, "Content-Type", "audio/wav");
        esp_http_client_set_header(client, "X-Auth-Token", ORCA_AUTH_TOKEN);
        esp_http_client_set_header(client, "X-Wake-Required", requireWakeWord ? "true" : "false");

        int64_t t0 = esp_timer_get_time();
        esp_err_t err_open = esp_http_client_open(client, payload_len);
        int64_t t1 = esp_timer_get_time();
        ESP_LOGI(TAG, "[TIMING] open: %lld ms, err=%d", (t1-t0)/1000, err_open);
        if (err_open != ESP_OK) {
            ESP_LOGE(TAG, "Open Orca-Brain gagal");
            esp_http_client_cleanup(client);
            continue;
        }

        bool ok = true;
        ok &= http_write_all(client, wav_hdr, 44);
        ok &= http_write_all(client, (char*)pcm, pcm_bytes);
        int64_t t2 = esp_timer_get_time();
        ESP_LOGI(TAG, "[TIMING] write: %lld ms, ok=%d", (t2-t1)/1000, ok);
        if (!ok) {
            ESP_LOGE(TAG, "Upload ke Orca-Brain gagal");
            esp_http_client_cleanup(client);
            continue;
        }

        int content_len = esp_http_client_fetch_headers(client);
        int64_t t3 = esp_timer_get_time();
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "[TIMING] fetch_headers: %lld ms, len=%d, status=%d", (t3-t2)/1000, content_len, status);

        char *ctype_ptr = NULL;
        esp_http_client_get_header(client, "Content-Type", &ctype_ptr);
        bool is_audio = ctype_ptr && strstr(ctype_ptr, "audio") != NULL;

        if (status != 200) {
            ESP_LOGE(TAG, "Orca-Brain gagal, status: %d", status);
            int ebuf_size = (content_len > 0 && content_len < 2048) ? content_len : 2048;
            char *ebuf = malloc(ebuf_size + 1);
            if (ebuf) {
                int n2, etotal = 0;
                while ((n2 = esp_http_client_read(client, ebuf+etotal, ebuf_size-etotal)) > 0) etotal += n2;
                ebuf[etotal] = '\0';
                ESP_LOGE(TAG, "Detail: %s", ebuf);
                free(ebuf);
            }
            esp_http_client_cleanup(client);
            continue;
        }

        if (!is_audio) {
            // response JSON: "diabaikan" (no wakeword / teks kosong) atau error lain, gak ada audio
            int jbuf_size = (content_len > 0) ? content_len : 512;
            char *jbuf = malloc(jbuf_size + 1);
            if (jbuf) {
                int n2, jtotal = 0;
                while ((n2 = esp_http_client_read(client, jbuf+jtotal, jbuf_size-jtotal)) > 0) jtotal += n2;
                jbuf[jtotal] = '\0';
                ESP_LOGI(TAG, "Orca-Brain resp (json): %s", jbuf);
                free(jbuf);
            }
            esp_http_client_cleanup(client);
            free(pcm);
            return;
        }

        // === response audio/mpeg: ambil metadata header DULU sebelum baca body ===
        char *aksi_ptr = NULL, *ucapan_ptr = NULL, *teks_ptr = NULL;
        esp_err_t r1 = esp_http_client_get_header(client, "X-Aksi", &aksi_ptr);
        esp_err_t r2 = esp_http_client_get_header(client, "X-Ucapan", &ucapan_ptr);
        esp_err_t r3 = esp_http_client_get_header(client, "X-Teks", &teks_ptr);
        ESP_LOGI(TAG, "[HDR] X-Aksi: ret=%d ptr=%p val=\"%s\"", r1, aksi_ptr, aksi_ptr ? aksi_ptr : "(NULL)");
        ESP_LOGI(TAG, "[HDR] X-Ucapan: ret=%d ptr=%p val=\"%s\"", r2, ucapan_ptr, ucapan_ptr ? ucapan_ptr : "(NULL)");
        ESP_LOGI(TAG, "[HDR] X-Teks: ret=%d ptr=%p val=\"%s\"", r3, teks_ptr, teks_ptr ? teks_ptr : "(NULL)");

        char aksi_buf[32] = {0};
        char ucapan_buf[600] = {0};
        char teks_buf[300] = {0};
        if (aksi_ptr)   strncpy(aksi_buf, aksi_ptr, sizeof(aksi_buf)-1);
        if (ucapan_ptr) url_decode(ucapan_buf, ucapan_ptr, sizeof(ucapan_buf));
        if (teks_ptr)   url_decode(teks_buf, teks_ptr, sizeof(teks_buf));

        ESP_LOGI(TAG, "Ngomong: [%s]", teks_buf);
        ESP_LOGI(TAG, "AKSI=%s UCAPAN=%s", aksi_buf, ucapan_buf);

        int buf_size = (content_len > 0) ? content_len : 512000;
        uint8_t *mp3_buf = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
        if (!mp3_buf) {
            ESP_LOGE(TAG, "PSRAM habis buat audio buffer");
            esp_http_client_cleanup(client);
            free(pcm);
            return;
        }

        int mp3_total = 0, n;
        while ((n = esp_http_client_read(client, (char*)mp3_buf+mp3_total, buf_size-mp3_total)) > 0)
            mp3_total += n;
        esp_http_client_cleanup(client);

        ESP_LOGI(TAG, "Audio downloaded: %d bytes (expected %d)", mp3_total, content_len);

        if (content_len > 0 && mp3_total < content_len) {
            ESP_LOGW(TAG, "Audio kepotong (%d/%d bytes), coba ulang dari awal...", mp3_total, content_len);
            heap_caps_free(mp3_buf);
            continue; // retry seluruh request
        }

        // eksekusi aksi
        if      (!strcmp(aksi_buf, "wakeword_off")) requireWakeWord = false;
        else if (!strcmp(aksi_buf, "wakeword_on"))  requireWakeWord = true;
        else if (!strcmp(aksi_buf, "ir_blaster"))   ESP_LOGI(TAG, "IR!");
        else if (!strcmp(aksi_buf, "wifi_scan")) {
            ESP_LOGI(TAG, "SCAN!");
            appMode = 1;
            scannerState = 1;
            triggerScan = true;
            scanDone = false;
            cursorInScanner = 0;
            scrollPosScanner = 0;
        }

        decode_and_play_mp3(mp3_buf, mp3_total);
        free(pcm);
        return;
    }

    ESP_LOGE(TAG, "Gagal proses audio setelah beberapa percobaan.");
    free(pcm);
}

// ============================================================
// TASK
// ============================================================
extern bool aiAudioEnabled;

void ai_audio_task(void *pvParameters) {
    ESP_LOGI(TAG, "Task AI Audio jalan.");
    while (1) {
        if (aiAudioEnabled) {
            ESP_LOGI(TAG, "Mendengarkan...");
            rekam_dan_proses();
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}
