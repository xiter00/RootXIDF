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

#define GEMINI_API_KEY "AQ.Ab8RN6Kw_7M0reGotMRh1ZQx9Xhz6Nj6QA_K3Fw4pI1f5zE3_Q"
#define GROQ_API_KEY "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"

static const char *TAG = "AI_AUDIO";
extern bool requireWakeWord;

i2s_chan_handle_t tx_chan = NULL;
i2s_chan_handle_t rx_chan = NULL;

// ==========================================
// HELPER: WRITE SEMUA BYTE, JANGAN SETENGAH
// ==========================================
static bool http_write_all(esp_http_client_handle_t client, const char *data, int len) {
    int written = 0;
    while (written < len) {
        int w = esp_http_client_write(client, data + written, len - written);
        if (w <= 0) {
            ESP_LOGE(TAG, "!!! WRITE GAGAL di byte %d/%d, return=%d", written, len, w);
            return false;
        }
        written += w;
        ESP_LOGD(TAG, "Write progress: %d/%d bytes", written, len);
    }
    ESP_LOGI(TAG, "Write OK: %d bytes terkirim", written);
    return true;
}

void init_i2s_audio(void) {
    ESP_LOGI(TAG, "Inisialisasi I2S Audio...");

    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));

    i2s_std_config_t tx_std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_SPK_BCLK,
            .ws   = I2S_SPK_LRC,
            .dout = I2S_SPK_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &tx_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
    ESP_LOGI(TAG, "I2S Speaker Berhasil Aktif!");

    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, &rx_chan));

    i2s_std_config_t rx_std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_MIC_SCK,
            .ws   = I2S_MIC_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = I2S_MIC_SD,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &rx_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
    ESP_LOGI(TAG, "I2S Mic Berhasil Aktif!");
}

void set_ai_audio_hardware(bool state) {
    if (state) {
        i2s_channel_enable(tx_chan);
        i2s_channel_enable(rx_chan);
        ESP_LOGI(TAG, "Hardware Audio: ON");
    } else {
        i2s_channel_disable(tx_chan);
        i2s_channel_disable(rx_chan);
        ESP_LOGI(TAG, "Hardware Audio: OFF");
    }
}

void url_encode(const char *src, char *dest) {
    const char *hex = "0123456789ABCDEF";
    while (*src) {
        if (isalnum((unsigned char)*src) || *src == '-' || *src == '_' || *src == '.' || *src == '~') {
            *dest++ = *src;
        } else if (*src == ' ') {
            *dest++ = '%'; *dest++ = '2'; *dest++ = '0';
        } else {
            *dest++ = '%';
            *dest++ = hex[(*src >> 4) & 15];
            *dest++ = hex[*src & 15];
        }
        src++;
    }
    *dest = '\0';
}

void play_google_tts(const char *text) {
    ESP_LOGI(TAG, "TTS: %s", text);
    char encoded_text[600];
    url_encode(text, encoded_text);

    char url[800];
    snprintf(url, sizeof(url),
        "http://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&tl=id&q=%s",
        encoded_text);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) { ESP_LOGE(TAG, "Gagal bikin HTTP Client TTS"); return; }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err == ESP_OK) {
        esp_http_client_fetch_headers(client);

        // Buffer lebih besar buat stream MP3 yang butuh data cukup buat decode
        uint8_t *mp3_buf = malloc(8192);
        if (!mp3_buf) { esp_http_client_cleanup(client); return; }

        mp3dec_t mp3d;
        mp3dec_init(&mp3d);
        mp3dec_frame_info_t frame_info;
        int16_t pcm_buffer[MINIMP3_MAX_SAMPLES_PER_FRAME];

        int total_in_buf = 0;
        int read_len;

        while (1) {
            // Isi buffer sampai penuh atau EOF
            read_len = esp_http_client_read(client,
                (char*)mp3_buf + total_in_buf,
                8192 - total_in_buf);
            if (read_len <= 0) break;
            total_in_buf += read_len;

            // Decode frame dari buffer
            int samples = mp3dec_decode_frame(&mp3d, mp3_buf, total_in_buf, pcm_buffer, &frame_info);
            if (samples > 0) {
                size_t bytes_written;
                i2s_channel_write(tx_chan, pcm_buffer, samples * 2, &bytes_written, 2000);
                // Geser sisa data ke awal buffer
                int consumed = frame_info.frame_bytes;
                total_in_buf -= consumed;
                memmove(mp3_buf, mp3_buf + consumed, total_in_buf);
            }
        }
        free(mp3_buf);
    } else {
        ESP_LOGE(TAG, "Gagal konek TTS");
    }
    esp_http_client_cleanup(client);
}

void tanya_gemini(const char* pertanyaan_user) {
    ESP_LOGI(TAG, "Kirim ke Gemini: %s", pertanyaan_user);

    char url[512];
    snprintf(url, sizeof(url),
        "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=%s",
        GEMINI_API_KEY);

    const char* template_payload =
    "{"
    "  \"system_instruction\": {"
    "    \"parts\": { \"text\": \"Lu adalah asisten AI hacker bernama RootX. "
    "Balas WAJIB pakai format JSON murni: {\\\"aksi\\\":\\\"...\\\", \\\"ucapan\\\":\\\"...\\\"}. "
    "Pilihan aksi: 'standby', 'ir_blaster', 'wifi_scan', 'deauth', 'wakeword_off', 'wakeword_on'. "
    "Jika obrolan biasa, pilih 'standby'. "
    "Jika user minta ngobrol bebas atau matikan wake word, pilih 'wakeword_off'. "
    "Jika user minta aktifkan wake word lagi, pilih 'wakeword_on'.\" }"
    "  },"
    "  \"contents\": [{"
    "    \"parts\": [{ \"text\": \"%s\" }]"
    "  }]"
    "}";

    char post_data[1024];
    snprintf(post_data, sizeof(post_data), template_payload, pertanyaan_user);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000,
        .skip_cert_common_name_check = true,
        .use_global_ca_store = false,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    ESP_LOGI(TAG, "Menunggu Gemini...");
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int len = esp_http_client_get_content_length(client);
        if (len <= 0) {
            ESP_LOGE(TAG, "Gemini balas kosong!");
            esp_http_client_cleanup(client);
            return;
        }
        char *response_buf = malloc(len + 1);
        if (!response_buf) { esp_http_client_cleanup(client); return; }
        esp_http_client_read(client, response_buf, len);
        response_buf[len] = '\0';

        cJSON *root = cJSON_Parse(response_buf);
        if (root) {
            cJSON *candidates  = cJSON_GetObjectItem(root, "candidates");
            cJSON *first_cand  = cJSON_GetArrayItem(candidates, 0);
            cJSON *content     = cJSON_GetObjectItem(first_cand, "content");
            cJSON *parts       = cJSON_GetObjectItem(content, "parts");
            cJSON *first_part  = cJSON_GetArrayItem(parts, 0);
            cJSON *text_node   = cJSON_GetObjectItem(first_part, "text");

            if (text_node && text_node->valuestring) {
                char *ai_reply = text_node->valuestring;
                ESP_LOGI(TAG, "Jawaban Mentah AI: %s", ai_reply);

                cJSON *ai_command = cJSON_Parse(ai_reply);
                if (ai_command) {
                    cJSON *aksi   = cJSON_GetObjectItem(ai_command, "aksi");
                    cJSON *ucapan = cJSON_GetObjectItem(ai_command, "ucapan");

                    if (aksi && ucapan) {
                        ESP_LOGI(TAG, "AKSI: %s | UCAPAN: %s",
                            aksi->valuestring, ucapan->valuestring);
                        play_google_tts(ucapan->valuestring);

                        if      (strcmp(aksi->valuestring, "ir_blaster")  == 0) ESP_LOGI(TAG, "IR Blaster!");
                        else if (strcmp(aksi->valuestring, "wifi_scan")   == 0) ESP_LOGI(TAG, "WiFi Scan!");
                        else if (strcmp(aksi->valuestring, "wakeword_off")== 0) {
                            requireWakeWord = false;
                            ESP_LOGI(TAG, "Wake word OFF");
                        }
                        else if (strcmp(aksi->valuestring, "wakeword_on") == 0) {
                            requireWakeWord = true;
                            ESP_LOGI(TAG, "Wake word ON");
                        }
                    }
                    cJSON_Delete(ai_command);
                } else {
                    ESP_LOGE(TAG, "AI balesannya bukan JSON murni!");
                }
            }
            cJSON_Delete(root);
        }
        free(response_buf);
    } else {
        ESP_LOGE(TAG, "Gagal ke Gemini: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

void generate_wav_header(char* wav_header, uint32_t waveDataSize, uint32_t sampleRate) {
    uint32_t fileSize = waveDataSize + 36;
    uint32_t byteRate = sampleRate * 2;
    uint8_t header[44] = {
        'R','I','F','F',
        fileSize&0xff,(fileSize>>8)&0xff,(fileSize>>16)&0xff,(fileSize>>24)&0xff,
        'W','A','V','E',
        'f','m','t',' ',
        16,0,0,0, 1,0, 1,0,
        sampleRate&0xff,(sampleRate>>8)&0xff,(sampleRate>>16)&0xff,(sampleRate>>24)&0xff,
        byteRate&0xff,(byteRate>>8)&0xff,(byteRate>>16)&0xff,(byteRate>>24)&0xff,
        2,0, 16,0,
        'd','a','t','a',
        waveDataSize&0xff,(waveDataSize>>8)&0xff,(waveDataSize>>16)&0xff,(waveDataSize>>24)&0xff
    };
    memcpy(wav_header, header, 44);
}

void mulai_rekam_dan_stt(void) {
    ESP_LOGI(TAG, "Mulai Ngerekam (4 Detik)...");

    uint32_t max_audio_bytes_32 = 16000 * 4 * 4;
    uint32_t max_audio_bytes_16 = 16000 * 2 * 4;

    int32_t* raw_buffer = heap_caps_malloc(max_audio_bytes_32, MALLOC_CAP_SPIRAM);
    if (!raw_buffer) { ESP_LOGE(TAG, "PSRAM habis (raw)"); return; }

    int16_t* audio_buffer = heap_caps_malloc(max_audio_bytes_16, MALLOC_CAP_SPIRAM);
    if (!audio_buffer) { ESP_LOGE(TAG, "PSRAM habis (audio)"); free(raw_buffer); return; }

    // === REKAM ===
    size_t bytes_read = 0, total_bytes = 0;
    while (total_bytes < max_audio_bytes_32) {
        esp_err_t ret = i2s_channel_read(rx_chan,
            (char*)raw_buffer + total_bytes,
            max_audio_bytes_32 - total_bytes,
            &bytes_read, 1000);
        if (ret != ESP_OK) break;
        total_bytes += bytes_read;
    }
    ESP_LOGI(TAG, "Rekam selesai: %d bytes raw", (int)total_bytes);

    // === KONVERSI 32→16 BIT ===
    int total_samples = total_bytes / 4;
    for (int i = 0; i < total_samples; i++) {
        int32_t val = raw_buffer[i] >> 14;
        if (val >  32767) val =  32767;
        if (val < -32768) val = -32768;
        audio_buffer[i] = (int16_t)val;
    }
    size_t final_bytes = total_samples * 2;
    free(raw_buffer);

    ESP_LOGI(TAG, "Konversi OK: %d bytes 16-bit", (int)final_bytes);
    ESP_LOGI(TAG, "Sample[0]=%d [100]=%d [1000]=%d",
        audio_buffer[0], audio_buffer[100], audio_buffer[1000]);

    // === CEK RMS ===
    float rms = 0;
    for (int i = 0; i < total_samples; i++)
        rms += (float)audio_buffer[i] * audio_buffer[i];
    rms = sqrtf(rms / total_samples);
    ESP_LOGI(TAG, "RMS Energy: %.1f", rms);

    if (rms < 500) {
        ESP_LOGI(TAG, "Terlalu sepi, skip.");
        free(audio_buffer);
        return;
    }

    // === SIAPKAN MULTIPART ===
    const char* boundary = "----RootXBoundary12345";

    char head[256];
    snprintf(head, sizeof(head),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n", boundary);

    char tail[512];
    snprintf(tail, sizeof(tail),
        "\r\n--%s\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
        "whisper-large-v3\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"language\"\r\n\r\n"
        "id\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"temperature\"\r\n\r\n"
        "0\r\n"
        "--%s--\r\n",
        boundary, boundary, boundary, boundary);

    char wav_header[44];
    generate_wav_header(wav_header, final_bytes, 16000);

    uint32_t total_payload_len = strlen(head) + 44 + final_bytes + strlen(tail);
    ESP_LOGI(TAG, "Total payload: %d bytes", (int)total_payload_len);

    // === KIRIM KE GROQ ===
    esp_http_client_config_t config = {
        .url = "https://api.groq.com/openai/v1/audio/transcriptions",
        .method = HTTP_METHOD_POST,
        .timeout_ms = 30000,
        .cert_pem = GROQ_ROOT_CA,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", GROQ_API_KEY);
    esp_http_client_set_header(client, "Authorization", auth_header);

    char content_type[64];
    snprintf(content_type, sizeof(content_type),
        "multipart/form-data; boundary=%s", boundary);
    esp_http_client_set_header(client, "Content-Type", content_type);

    esp_err_t open_err = esp_http_client_open(client, total_payload_len);
    if (open_err != ESP_OK) {
        ESP_LOGE(TAG, "Gagal open koneksi Groq: %s", esp_err_to_name(open_err));
        esp_http_client_cleanup(client);
        free(audio_buffer);
        return;
    }

    // === WRITE DENGAN LOOP (INI YANG BEDA) ===
    bool ok = true;
    ok &= http_write_all(client, head, strlen(head));
    ok &= http_write_all(client, wav_header, 44);
    ok &= http_write_all(client, (char*)audio_buffer, final_bytes);
    ok &= http_write_all(client, tail, strlen(tail));

    if (!ok) {
        ESP_LOGE(TAG, "Upload GAGAL, koneksi putus waktu kirim data.");
        esp_http_client_cleanup(client);
        free(audio_buffer);
        return;
    }
    ESP_LOGI(TAG, "Upload SUKSES total %d bytes.", (int)total_payload_len);

    // === BACA HASIL STT ===
    int content_length = esp_http_client_fetch_headers(client);
    ESP_LOGI(TAG, "Groq response content_length: %d", content_length);

    if (content_length > 0) {
        char* stt_response = malloc(content_length + 1);
        if (!stt_response) {
            ESP_LOGE(TAG, "Malloc response gagal");
            esp_http_client_cleanup(client);
            free(audio_buffer);
            return;
        }
        esp_http_client_read(client, stt_response, content_length);
        stt_response[content_length] = '\0';
        ESP_LOGI(TAG, "Hasil STT: %s", stt_response);

        cJSON *stt_json = cJSON_Parse(stt_response);
        if (stt_json) {
            cJSON *text_node = cJSON_GetObjectItem(stt_json, "text");
            if (text_node && text_node->valuestring) {
                char* teks = text_node->valuestring;
                ESP_LOGI(TAG, "Lu ngomong: %s", teks);

                if (requireWakeWord) {
                    if (strstr(teks, "RootX") || strstr(teks, "Root X") || strstr(teks, "rootx")) {
                        ESP_LOGI(TAG, "Wake word terdeteksi!");
                        tanya_gemini(teks);
                    } else {
                        ESP_LOGI(TAG, "Bukan wake word. Skip.");
                    }
                } else {
                    tanya_gemini(teks);
                }
            }
            cJSON_Delete(stt_json);
        }
        free(stt_response);
    } else {
        ESP_LOGE(TAG, "Groq respon kosong/error. content_length=%d", content_length);
    }

    esp_http_client_cleanup(client);
    free(audio_buffer);
}

extern bool aiAudioEnabled;

void ai_audio_task(void *pvParameters) {
    ESP_LOGI(TAG, "Task AI Audio Berjalan...");
    while (1) {
        if (aiAudioEnabled) {
            ESP_LOGI(TAG, "AI AKTIF: Mendengarkan...");
            mulai_rekam_dan_stt();
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}