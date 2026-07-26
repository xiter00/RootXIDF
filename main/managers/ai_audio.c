#include "globals.h"
#include <stdio.h>
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
#include "groq_cert.h"
#include <math.h>
#include "gemini_cert.h"
#include "freetts_cert.h"
#include "api_keys.h" 



// Potong berapa detik dari belakang audio freetts (watermark)
#define TTS_CUT_SECONDS 3

static const char *TAG = "AI_AUDIO";
extern bool requireWakeWord;

i2s_chan_handle_t tx_chan = NULL;
i2s_chan_handle_t rx_chan = NULL;

// ============================================================
// HELPER WRITE
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
// I2S INIT
// ============================================================
void init_i2s_audio(void) {
    ESP_LOGI(TAG, "Init I2S Audio...");

    // TX - SPEAKER
    i2s_chan_config_t tx_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_cfg, &tx_chan, NULL));
    i2s_std_config_t tx_std = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
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
// FREE TTS
// ============================================================
void play_freetts(const char *text) {
    ESP_LOGI(TAG, "TTS: %s", text);

    char safe_text[512] = {0};
    int si = 0;
    for (int i = 0; text[i] && si < 510; i++) {
        if (text[i] == '"') { safe_text[si++] = '\\'; safe_text[si++] = '"'; }
        else safe_text[si++] = text[i];
    }

    // === STEP 1: POST untuk dapet file_id ===
    char body[600];
    snprintf(body, sizeof(body),
        "{\"text\":\"%s\",\"voice\":\"id-ID-ArdiNeural\",\"rate\":\"+0%%\",\"pitch\":\"+0Hz\"}",
        safe_text);

    esp_http_client_config_t cfg1 = {
        .url = "https://freetts.org/api/tts",
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000,
        .cert_pem = FREETTS_ROOT_CA,
        .auth_type = HTTP_AUTH_TYPE_NONE,
    };

    esp_http_client_handle_t c1 = esp_http_client_init(&cfg1);
    esp_http_client_set_header(c1, "Content-Type", "application/json");

    char file_id[64] = {0};
    int body1_len = strlen(body);

    if (esp_http_client_open(c1, body1_len) != ESP_OK) {
        ESP_LOGE(TAG, "FreeTTS open gagal");
        esp_http_client_cleanup(c1);
        return;
    }

    if (!http_write_all(c1, body, body1_len)) {
        ESP_LOGE(TAG, "FreeTTS write gagal");
        esp_http_client_cleanup(c1);
        return;
    }

    int len1 = esp_http_client_fetch_headers(c1);
    int buf1_size = (len1 > 0) ? len1 : 4096;
    char *resp1 = malloc(buf1_size + 1);
    if (resp1) {
        int total1 = 0, n1;
        while ((n1 = esp_http_client_read(c1, resp1 + total1, buf1_size - total1)) > 0)
            total1 += n1;
        resp1[total1] = '\0';
        ESP_LOGI(TAG, "FreeTTS resp: %s", resp1);
        cJSON *j = cJSON_Parse(resp1);
        if (j) {
            cJSON *fid = cJSON_GetObjectItem(j, "file_id");
            if (fid && fid->valuestring)
                strlcpy(file_id, fid->valuestring, sizeof(file_id));
            cJSON_Delete(j);
        }
        free(resp1);
    }
    esp_http_client_cleanup(c1);

    if (strlen(file_id) == 0) {
        ESP_LOGE(TAG, "Gagal dapet file_id dari FreeTTS");
        return;
    }
    ESP_LOGI(TAG, "file_id: %s", file_id);

    // === STEP 2: GET audio pake file_id ===
    char audio_url[128];
    snprintf(audio_url, sizeof(audio_url), "https://freetts.org/api/audio/%s", file_id);

    esp_http_client_config_t cfg2 = {
        .url = audio_url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 15000,
        .cert_pem = FREETTS_ROOT_CA,
        .auth_type = HTTP_AUTH_TYPE_NONE,
    };

    esp_http_client_handle_t c2 = esp_http_client_init(&cfg2);

    if (esp_http_client_open(c2, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Gagal open audio URL");
        esp_http_client_cleanup(c2); return;
    }

    int content_len = esp_http_client_fetch_headers(c2);
    int buf_size = (content_len > 0) ? content_len : 512000;

    uint8_t *mp3_buf = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    if (!mp3_buf) {
        ESP_LOGE(TAG, "PSRAM habis buat TTS buffer");
        esp_http_client_cleanup(c2); return;
    }

    int total = 0, n;
    while ((n = esp_http_client_read(c2, (char*)mp3_buf+total, buf_size-total)) > 0)
        total += n;
    esp_http_client_cleanup(c2);

    ESP_LOGI(TAG, "Audio downloaded: %d bytes", total);

    int cut_bytes = TTS_CUT_SECONDS * 16000;
    int play_len = total - cut_bytes;
    if (play_len <= 0) {
        ESP_LOGW(TAG, "Audio terlalu pendek, play semua");
        play_len = total;
    }
    ESP_LOGI(TAG, "Play %d bytes, potong %d bytes terakhir", play_len, cut_bytes);

    mp3dec_t mp3d; mp3dec_init(&mp3d);
    mp3dec_frame_info_t fi;
    int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    int pos = 0;
    while (pos < play_len) {
        int s = mp3dec_decode_frame(&mp3d, mp3_buf+pos, play_len-pos, pcm, &fi);
        if (s > 0) {
            size_t bw;
            i2s_channel_write(tx_chan, pcm, s*2, &bw, 2000);
        }
        if (fi.frame_bytes > 0) pos += fi.frame_bytes;
        else break;
    }

    heap_caps_free(mp3_buf);
}

// ============================================================
// GEMINI
// ============================================================
void tanya_gemini(const char *q) {
    ESP_LOGI(TAG, "→ Gemini: %s", q);


        char url[400];
snprintf(url, sizeof(url),
    "https://generativelanguage.googleapis.com/v1beta/models/gemini-flash-lite-latest:generateContent?key=%s",
    GEMINI_API_KEY);

    char safe_q[512] = {0};
    int si = 0;
    for (int i = 0; q[i] && si < 510; i++) {
        if (q[i] == '"')       { safe_q[si++] = '\\'; safe_q[si++] = '"'; }
        else if (q[i] == '\\') { safe_q[si++] = '\\'; safe_q[si++] = '\\'; }
        else if (q[i] == '\n') { safe_q[si++] = '\\'; safe_q[si++] = 'n'; }
        else safe_q[si++] = q[i];
    }

    const char *tpl =
        "{\"system_instruction\":{\"parts\":[{\"text\":\"Lu adalah asisten AI hacker bernama Nova. "
        "Kamu dibuat oleh Andyy. "
        "Akun github creator adalah github.com/xiter00. "
        "Balas WAJIB pakai format JSON murni dengan key aksi dan ucapan. "
        "Pilihan aksi: standby, ir_blaster, wifi_scan, deauth, wakeword_off, wakeword_on. "
        "Obrolan biasa pilih standby. "
        "Jawab jelas dan singkat, tapi jangan terlalu singkat. "
        "Jika konteks pertanyaan butuh penjelasan panjang, sesuaikan panjang jawabannya. "
        "Minta matikan wake word pilih wakeword_off. "
        "Minta aktifkan wake word pilih wakeword_on.\"}]}},"
        "\"generationConfig\":{\"responseMimeType\":\"application/json\"},"
        "\"contents\":[{\"parts\":[{\"text\":\"%s\"}]}]}";

    char *body = malloc(2048);
    if (!body) { ESP_LOGE(TAG, "malloc gagal"); return; }
    snprintf(body, 2048, tpl, safe_q);

    // === DEBUG LENGKAP: cek segala hal SEBELUM kirim, gak makan limit API ===
    {
        int klen = strlen(GEMINI_API_KEY);
        unsigned long khash = 5381;
        for (int i = 0; i < klen; i++) khash = ((khash << 5) + khash) + (unsigned char)GEMINI_API_KEY[i];
        ESP_LOGI(TAG, "DEBUG key_len=%d key_hash=%lu key_prefix=%.12s key_suffix=%s",
                 klen, khash, GEMINI_API_KEY, GEMINI_API_KEY + (klen > 6 ? klen - 6 : 0));

        int bad_char_found = 0;
        for (int i = 0; i < klen; i++) {
            unsigned char ch = (unsigned char)GEMINI_API_KEY[i];
            if (ch < 0x21 || ch > 0x7E) {
                ESP_LOGE(TAG, "DEBUG key BYTE ANEH di index %d = 0x%02X", i, ch);
                bad_char_found = 1;
            }
        }
        if (!bad_char_found) ESP_LOGI(TAG, "DEBUG semua byte key printable ASCII, aman");

        ESP_LOGI(TAG, "DEBUG url_len=%d url=%s", (int)strlen(url), url);
        ESP_LOGI(TAG, "DEBUG body_len=%d body=%s", (int)strlen(body), body);
    }

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000,
        .cert_pem = GEMINI_ROOT_CA,
        .auth_type = HTTP_AUTH_TYPE_NONE,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
    };

    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    esp_http_client_set_header(c, "Content-Type", "application/json");

    ESP_LOGI(TAG, "DEBUG method=%d timeout=%d buf=%d buf_tx=%d",
             (int)cfg.method, cfg.timeout_ms, cfg.buffer_size, cfg.buffer_size_tx);

    // === Pakai open/write/read (sama kek Groq), BUKAN perform ===
    // perform otomatis handle auth challenge (Bearer) → error di ESP HTTP client
    int body_len = strlen(body);
    esp_err_t open_err = esp_http_client_open(c, body_len);
    if (open_err != ESP_OK) {
        ESP_LOGE(TAG, "Gemini open gagal, err=0x%x (%s)", open_err, esp_err_to_name(open_err));
        free(body);
        esp_http_client_cleanup(c);
        return;
    }
    ESP_LOGI(TAG, "DEBUG open OK, socket siap kirim body_len=%d", body_len);

    if (!http_write_all(c, body, body_len)) {
        ESP_LOGE(TAG, "Gemini write body gagal");
        free(body);
        esp_http_client_cleanup(c);
        return;
    }
    ESP_LOGI(TAG, "DEBUG write body sukses, %d byte terkirim", body_len);

    int len = esp_http_client_fetch_headers(c);
    int status = esp_http_client_get_status_code(c);
    ESP_LOGI(TAG, "Gemini status: %d | len: %d | chunked: %d",
             status, len, esp_http_client_is_chunked_response(c));

    // Gemini kadang chunked (len = -1), alokasi buffer fallback
    int buf_size = (len > 0) ? len : 8192;
    {
        char *buf = malloc(buf_size + 1);
        if (buf) {
            int total_read = 0, n;
            while ((n = esp_http_client_read(c, buf + total_read, buf_size - total_read)) > 0)
                total_read += n;
            buf[total_read] = '\0';
            len = total_read;

            if (status == 200) {
                cJSON *resp = cJSON_Parse(buf);
                if (resp) {
                    cJSON *candidates = cJSON_GetObjectItem(resp, "candidates");
                    cJSON *cand0      = cJSON_GetArrayItem(candidates, 0);
                    cJSON *content    = cJSON_GetObjectItem(cand0, "content");
                    cJSON *rparts     = cJSON_GetObjectItem(content, "parts");
                    cJSON *rpart0     = cJSON_GetArrayItem(rparts, 0);
                    cJSON *tnode      = cJSON_GetObjectItem(rpart0, "text");

                    if (tnode && tnode->valuestring) {
                        ESP_LOGI(TAG, "Gemini reply: %s", tnode->valuestring);

                        char *reply = tnode->valuestring;
                        char *json_start = strstr(reply, "{");
                        char *json_end   = strrchr(reply, '}');
                        if (json_start && json_end && json_end > json_start) {
                            *(json_end + 1) = '\0';
                            reply = json_start;
                        }

                        cJSON *cmd = cJSON_Parse(reply);
                        if (cmd) {
                            cJSON *aksi   = cJSON_GetObjectItem(cmd, "aksi");
                            cJSON *ucapan = cJSON_GetObjectItem(cmd, "ucapan");
                            if (aksi && ucapan) {
                                ESP_LOGI(TAG, "AKSI=%s UCAPAN=%s",
                                    aksi->valuestring, ucapan->valuestring);
                                play_freetts(ucapan->valuestring);
                                if      (!strcmp(aksi->valuestring, "wakeword_off")) requireWakeWord = false;
                                else if (!strcmp(aksi->valuestring, "wakeword_on"))  requireWakeWord = true;
                                else if (!strcmp(aksi->valuestring, "ir_blaster"))   ESP_LOGI(TAG, "IR!");
                                else if (!strcmp(aksi->valuestring, "wifi_scan")) {
                                    ESP_LOGI(TAG, "SCAN!");
                                    appMode = 1;
                                    scannerState = 1;
                                    triggerScan = true;
                                    scanDone = false;
                                    cursorInScanner = 0;
                                    scrollPosScanner = 0;
                                }
                            }
                            cJSON_Delete(cmd);
                        } else {
                            ESP_LOGE(TAG, "Gemini bales bukan JSON: %s", reply);
                        }
                    }
                    cJSON_Delete(resp);
                }
            } else {
                ESP_LOGE(TAG, "Gemini status error: %d | resp: %s", status, buf);
            }
            free(buf);
        } else {
            ESP_LOGE(TAG, "malloc buf Gemini gagal");
        }
    }

    free(body);
    esp_http_client_cleanup(c);
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
// REKAM + STT
// ============================================================
void mulai_rekam_dan_stt(void) {
    uint32_t sz32 = 16000 * 4 * 4;
    uint32_t sz16 = 16000 * 2 * 4;

    int32_t *raw = heap_caps_malloc(sz32, MALLOC_CAP_SPIRAM);
    int16_t *pcm = heap_caps_malloc(sz16, MALLOC_CAP_SPIRAM);
    if (!raw || !pcm) {
        ESP_LOGE(TAG, "PSRAM habis!");
        free(raw); free(pcm); return;
    }

    // === REKAM ===
    size_t bytes_read = 0, total = 0;

    char *throwaway = malloc(16000 * 4 / 10);
    if (throwaway) {
        i2s_channel_read(rx_chan, throwaway, 16000 * 4 / 10, &bytes_read, 1000);
        free(throwaway);
    }

    while (total < sz32) {
        if (i2s_channel_read(rx_chan, (char*)raw+total, sz32-total, &bytes_read, 1000) != ESP_OK) break;
        total += bytes_read;
        vTaskDelay(1);
    }

    // === KONVERSI 32→16 BIT ===
    int n_samples = total / 4;
    for (int i = 0; i < n_samples; i++) {
        int32_t v = raw[i] >> 13;
        if (v >  32767) v =  32767;
        if (v < -32768) v = -32768;
        pcm[i] = (int16_t)v;
    }
    size_t pcm_bytes = n_samples * 2;
    free(raw);

    ESP_LOGI(TAG, "PCM: %d bytes | s[0]=%d s[100]=%d s[1000]=%d",
        (int)pcm_bytes, pcm[0], pcm[100], pcm[1000]);

    // === CEK RMS ===
    float rms = 0;
    for (int i = 0; i < n_samples; i++) rms += (float)pcm[i] * pcm[i];
    rms = sqrtf(rms / n_samples);
    ESP_LOGI(TAG, "RMS: %.1f", rms);

    if (rms < 500) {
        ESP_LOGI(TAG, "Sepi, skip.");
        free(pcm); return;
    }

    // === GENERATE WAV HEADER ===
    char wav_hdr[44];
    generate_wav_header(wav_hdr, pcm_bytes, 16000);

    // === MULTIPART SETUP ===
    const char *boundary = "----RootXBoundary12345";
    char head[256], tail[768];
    snprintf(head, sizeof(head),
        "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n", boundary);
    snprintf(tail, sizeof(tail),
        "\r\n--%s\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\nwhisper-large-v3\r\n"
        "--%s\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\nid\r\n"
        "--%s\r\nContent-Disposition: form-data; name=\"temperature\"\r\n\r\n0\r\n"
        "--%s\r\nContent-Disposition: form-data; name=\"prompt\"\r\n\r\nNova, Nova, ESP32, asisten AI, gw, lu, kek gini, obrolan kasual, nyalain, matiin\r\n"
        "--%s--\r\n",
        boundary, boundary, boundary, boundary, boundary);

    // === KIRIM KE GROQ ===
    uint32_t payload_len = strlen(head) + 44 + pcm_bytes + strlen(tail);
    ESP_LOGI(TAG, "Payload: %d bytes", (int)payload_len);

    esp_http_client_config_t cfg = {
        .url = "https://api.groq.com/openai/v1/audio/transcriptions",
        .method = HTTP_METHOD_POST,
        .timeout_ms = 30000,
        .cert_pem = GROQ_ROOT_CA,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);

    char auth[128];
    snprintf(auth, sizeof(auth), "Bearer %s", GROQ_API_KEY);
    esp_http_client_set_header(client, "Authorization", auth);

    char ctype[64];
    snprintf(ctype, sizeof(ctype), "multipart/form-data; boundary=%s", boundary);
    esp_http_client_set_header(client, "Content-Type", ctype);

    if (esp_http_client_open(client, payload_len) != ESP_OK) {
        ESP_LOGE(TAG, "Open Groq gagal");
        esp_http_client_cleanup(client); free(pcm); return;
    }

    bool ok = true;
    ok &= http_write_all(client, head, strlen(head));
    ok &= http_write_all(client, wav_hdr, 44);
    ok &= http_write_all(client, (char*)pcm, pcm_bytes);
    ok &= http_write_all(client, tail, strlen(tail));

    if (!ok) {
        ESP_LOGE(TAG, "Upload gagal");
        esp_http_client_cleanup(client); free(pcm); return;
    }
    ESP_LOGI(TAG, "Upload OK: %d bytes", (int)payload_len);

    // === BACA HASIL STT ===
    int clen = esp_http_client_fetch_headers(client);
    ESP_LOGI(TAG, "Groq content_length: %d", clen);

    if (clen > 0) {
        char *resp = malloc(clen + 1);
        if (resp) {
            esp_http_client_read(client, resp, clen);
            resp[clen] = '\0';
            ESP_LOGI(TAG, "STT raw: %s", resp);

            cJSON *j = cJSON_Parse(resp);
            if (j) {
                cJSON *t = cJSON_GetObjectItem(j, "text");
                if (t && t->valuestring) {
                    char *teks = t->valuestring;
                    ESP_LOGI(TAG, "Ngomong: [%s]", teks);

                    // Lowercase buat cek wake word
                    char lower[256];
                    int li = 0;
                    for (int ci = 0; teks[ci] && li < 255; ci++)
                        lower[li++] = tolower((unsigned char)teks[ci]);
                    lower[li] = '\0';

                    // Filter halusinasi
                    const char *blacklist[] = {
                        "terima kasih telah menonton",
                        "terima kasih sudah menonton",
                        "subscribe", "jangan lupa like", "terima kasih", "terimakasih"
                    };
                    bool halusinasi = false;
                    for (int b = 0; b < 6; b++) {
                        if (strcasestr(lower, blacklist[b])) { halusinasi = true; break; }
                    }

                    if (halusinasi) {
                        ESP_LOGW(TAG, "Halusinasi, skip.");
                    } else if (requireWakeWord) {
                        // Cek "nova" sebagai kata utuh, bukan substring
                        bool wakeword_found = false;
                        char *p = lower;
                        while ((p = strstr(p, "nova")) != NULL) {
                            bool before_ok = (p == lower) || !isalpha((unsigned char)*(p-1));
                            bool after_ok  = !isalpha((unsigned char)*(p+4));
                            if (before_ok && after_ok) { wakeword_found = true; break; }
                            p++;
                        }
                        if (wakeword_found) {
                            ESP_LOGI(TAG, "Wake word terdeteksi!");
                            tanya_gemini(teks);
                        } else {
                            ESP_LOGI(TAG, "Bukan manggil gw. Diabaikan.");
                        }
                    } else {
                        tanya_gemini(teks);
                    }
                }
                cJSON_Delete(j);
            }
            free(resp);
        }
    } else {
        ESP_LOGE(TAG, "Groq respon kosong");
    }

    esp_http_client_cleanup(client);
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
            mulai_rekam_dan_stt();
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}
