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
#include "esp_crt_bundle.h" // <--- WAJIB BUAT HTTPS
#include "groq_cert.h"


// MASUKIN API KEY GEMINI LU DI SINI COK!
#define GEMINI_API_KEY "AQ.Ab8RN6Kw_7M0reGotMRh1ZQx9Xhz6Nj6QA_K3Fw4pI1f5zE3_Q" 
#define GROQ_API_KEY "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"

static const char *TAG = "AI_AUDIO";

i2s_chan_handle_t tx_chan = NULL; // Speaker
i2s_chan_handle_t rx_chan = NULL; // Mic

void init_i2s_audio(void) {
    ESP_LOGI(TAG, "Inisialisasi I2S Audio...");

    // ==========================================
    // 1. SETUP I2S OUTPUT (MAX98357A - SPEAKER)
    // ==========================================
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));

    i2s_std_config_t tx_std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000), // Sample rate 16kHz (standar Google TTS)
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,    // MAX98357A gak butuh Master Clock
            .bclk = I2S_SPK_BCLK,
            .ws   = I2S_SPK_LRC,
            .dout = I2S_SPK_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &tx_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
    ESP_LOGI(TAG, "I2S Speaker Berhasil Aktif!");


    // ==========================================
    // 2. SETUP I2S INPUT (INMP441 - MIC)
    // ==========================================
    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, &rx_chan));

    i2s_std_config_t rx_std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000), // Sample rate 16kHz buat speech recognition
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO), // INMP441 biasanya butuh 32-bit slot
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_MIC_SCK,
            .ws   = I2S_MIC_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = I2S_MIC_SD,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &rx_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
    ESP_LOGI(TAG, "I2S Mic Berhasil Aktif!");
}

void set_ai_audio_hardware(bool state) {
    if (state) {
        // Nyalakan mesin I2S (Bangunkan Hardware)
        i2s_channel_enable(tx_chan);
        i2s_channel_enable(rx_chan);
        ESP_LOGI(TAG, "Hardware Audio: ON (Siap Mendengar)");
    } else {
        // Matikan mesin I2S (Tidurkan Hardware)
        i2s_channel_disable(tx_chan);
        i2s_channel_disable(rx_chan);
        ESP_LOGI(TAG, "Hardware Audio: OFF (Standby Mode)");
    }
}

// --- FUNGSI 1: URL ENCODER (Biar spasi jadi %20) ---
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

// --- FUNGSI 2: TEMBAK GOOGLE TTS ---
void play_google_tts(const char *text) {
    ESP_LOGI(TAG, "Menyiapkan suara untuk: %s", text);

    // 1. Siapin Buffer buat teks yang di-encode (Max 200 karakter biar aman)
    char encoded_text[600]; 
    url_encode(text, encoded_text);

    // 2. Rakit URL Sakti Google Translate
    char url[800];
    snprintf(url, sizeof(url), "http://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&tl=id&q=%s", encoded_text);

    // 3. Konfigurasi HTTP Client
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Gagal bikin HTTP Client");
        return;
    }

    // 4. Buka Koneksi dan Tarik Datanya
    esp_err_t err = esp_http_client_open(client, 0);
    if (err == ESP_OK) {
        esp_http_client_fetch_headers(client);
        
        char buffer[1024]; // Buffer sementara buat narik data
        int read_len;
        int total_read = 0;
        
        ESP_LOGI(TAG, "Mulai download stream audio...");
        
        // Looping narik file MP3 dari server Google
        while ((read_len = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
            total_read += read_len;
            



// ... (sebelumnya sama)

        // Variabel buat decoder
          // 1. Setup Decoder MP3 yang bener (Modern API)
        mp3dec_t mp3d;
        mp3dec_init(&mp3d);
        
        mp3dec_frame_info_t frame_info;
        int16_t pcm_buffer[MINIMP3_MAX_SAMPLES_PER_FRAME]; // Buffer PCM

        ESP_LOGI(TAG, "Mulai decode stream audio...");
        
        // 2. Loop baca data dan langsung kirim ke Speaker
        while ((read_len = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
            
            // decode_frame return jumlah sample yang berhasil di-decode
            int samples = mp3dec_decode_frame(&mp3d, (unsigned char*)buffer, read_len, pcm_buffer, &frame_info);
            
            if (samples > 0) {
                // PCM sudah jadi! Lempar ke I2S (Speaker)
                // samples * 2 karena per-sample itu 16-bit (2 byte)
                size_t bytes_written;
                i2s_channel_write(tx_chan, pcm_buffer, samples * 2, &bytes_written, 2000);
            }
        }

        
// ... (seterusnya sama)

        }
        ESP_LOGI(TAG, "Selesai! Total ditarik: %d bytes", total_read);
    } else {
        ESP_LOGE(TAG, "Gagal konek ke Google TTS");
    }

    esp_http_client_cleanup(client);
}


void tanya_gemini(const char* pertanyaan_user) {
    ESP_LOGI(TAG, "Mengirim ke otak Gemini: %s", pertanyaan_user);

    // 1. Siapin URL API Gemini (Pake Model 1.5 Flash biar ngebut)
    char url[256];
    snprintf(url, sizeof(url), "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=%s", GEMINI_API_KEY);

    // 2. Rakit Payload JSON (Instruksi Hacker + Pertanyaan Lu)
    // Perhatikan: Kita paksa AI buat balas pakai JSON murni tanpa gaya-gayaan (tanpa markdown).
    const char* template_payload = 
        "{"
        "  \"system_instruction\": {"
        "    \"parts\": { \"text\": \"Lu adalah asisten AI hacker bernama RootX. Lu ada di dalam alat genggam ESP32. Balas WAJIB pakai format JSON murni tanpa markdown: {\\\"aksi\\\":\\\"...\\\", \\\"ucapan\\\":\\\"...\\\"}. Pilihan aksi: 'standby', 'ir_blaster', 'wifi_scan', 'deauth'. Jika obrolan biasa, pilih 'standby'.\" }"
        "  },"
        "  \"contents\": [{"
        "    \"parts\": [{ \"text\": \"%s\" }]"
        "  }]"
        "}";

    // Bikin buffer buat nampung teks JSON yang mau dikirim
    char post_data[1024]; 
    snprintf(post_data, sizeof(post_data), template_payload, pertanyaan_user);

    // 3. Konfigurasi HTTP POST
        // 3. Konfigurasi HTTP POST
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000,
         .skip_cert_common_name_check = true,
         .use_global_ca_store = false,        // Jangan pake store global yang bikin `-0x7780`
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
       // <--- TAMBAHIN BARIS INI JUGA!
    };


    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // 4. Eksekusi Tembakan ke Server Google!
    ESP_LOGI(TAG, "Menunggu AI mikir...");
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int len = esp_http_client_get_content_length(client);
        char *response_buf = malloc(len + 1); // Alokasi RAM buat nangkep jawaban
        esp_http_client_read(client, response_buf, len);
        response_buf[len] = '\0';

        // ==========================================
        // 5. BEDAH OTAK AI (PARSING JSON)
        // ==========================================
        cJSON *root = cJSON_Parse(response_buf);
        if (root) {
            // Ini rute buat masuk ke sarang jawaban Gemini (ngegali struktur JSON-nya)
            cJSON *candidates = cJSON_GetObjectItem(root, "candidates");
            cJSON *first_candidate = cJSON_GetArrayItem(candidates, 0);
            cJSON *content = cJSON_GetObjectItem(first_candidate, "content");
            cJSON *parts = cJSON_GetObjectItem(content, "parts");
            cJSON *first_part = cJSON_GetArrayItem(parts, 0);
            cJSON *text_node = cJSON_GetObjectItem(first_part, "text");

            if (text_node && text_node->valuestring) {
                char *ai_reply = text_node->valuestring;
                ESP_LOGI(TAG, "Jawaban Mentah AI: %s", ai_reply);

                // Gemini ngerespon pake JSON (Sesuai paksaan kita). Sekarang kita parse JSON buatan dia!
                cJSON *ai_command = cJSON_Parse(ai_reply);
                if (ai_command) {
                    cJSON *aksi = cJSON_GetObjectItem(ai_command, "aksi");
                    cJSON *ucapan = cJSON_GetObjectItem(ai_command, "ucapan");

                    if (aksi && ucapan) {
                        ESP_LOGI(TAG, ">>> AKSI HARDWARE: %s", aksi->valuestring);
                        ESP_LOGI(TAG, ">>> KATA-KATA AI: %s", ucapan->valuestring);

                        // A. SURUH ALAT LU NGOMONG!
                        play_google_tts(ucapan->valuestring);

                        // B. SURUH ALAT LU JALANIN FITUR (IF/ELSE SAKTI)
                        if (strcmp(aksi->valuestring, "ir_blaster") == 0) {
                            // Contoh: Langsung nyalain TV-B-Gone!
                            // appMode = 18; tvbgoneState = 1; dll...
                            ESP_LOGI(TAG, "Mengeksekusi IR Blaster...");
                        } 
                        else if (strcmp(aksi->valuestring, "wifi_scan") == 0) {
                            // Contoh: Langsung loncat ke menu WiFi Scan
                            // appMode = 1; scannerState = 1; triggerScan = true;
                            ESP_LOGI(TAG, "Mengeksekusi WiFi Scanner...");
                        }
                    }
                    cJSON_Delete(ai_command);
                } else {
                    ESP_LOGE(TAG, "AI ngeyel, balesannya bukan JSON murni!");
                }
            }
            cJSON_Delete(root);
        }
        free(response_buf); // Jangan lupa balikin RAM biar gak memory leak!
    } else {
        ESP_LOGE(TAG, "Gagal nembak Gemini: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

// API KEY GROQ LU (Daftar gratis di console.groq.com)


// Fungsi buat bikin Header WAV (Biar server STT bisa baca audionya)
void generate_wav_header(char* wav_header, uint32_t waveDataSize, uint32_t sampleRate) {
    uint32_t fileSize = waveDataSize + 36;
    uint32_t byteRate = sampleRate * 2; // 16-bit mono

    uint8_t header[44] = {
        'R', 'I', 'F', 'F',
        fileSize & 0xff, (fileSize >> 8) & 0xff, (fileSize >> 16) & 0xff, (fileSize >> 24) & 0xff,
        'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ',
        16, 0, 0, 0,          // Subchunk1Size (16 for PCM)
        1, 0,                 // AudioFormat (1 for PCM)
        1, 0,                 // NumChannels (1 mono)
        sampleRate & 0xff, (sampleRate >> 8) & 0xff, (sampleRate >> 16) & 0xff, (sampleRate >> 24) & 0xff,
        byteRate & 0xff, (byteRate >> 8) & 0xff, (byteRate >> 16) & 0xff, (byteRate >> 24) & 0xff,
        2, 0,                 // BlockAlign
        16, 0,                // BitsPerSample
        'd', 'a', 't', 'a',
        waveDataSize & 0xff, (waveDataSize >> 8) & 0xff, (waveDataSize >> 16) & 0xff, (waveDataSize >> 24) & 0xff
    };
    memcpy(wav_header, header, 44);
}

// Fungsi Utama Perekam & STT
void mulai_rekam_dan_stt(void) {
    ESP_LOGI(TAG, "Mulai Ngerekam Suara (4 Detik)...");

    // 1. Alokasi RAM (PSRAM) buat nyimpen rekaman 4 detik (16kHz, 16-bit = ~128KB)
    int record_time_sec = 4;
    uint32_t max_audio_bytes = 16000 * 2 * record_time_sec;
    
    // WAJIB pakai heap_caps_malloc buat maksa pake PSRAM, RAM biasa bakal jebol!
    char* audio_buffer = heap_caps_malloc(max_audio_bytes, MALLOC_CAP_SPIRAM); 
    if (!audio_buffer) {
        ESP_LOGE(TAG, "PSRAM Habis Cok! Gagal alokasi buffer audio.");
        return;
    }

    // 2. Baca data dari INMP441 (Mic)
    size_t bytes_read = 0;
    i2s_channel_read(rx_chan, audio_buffer, max_audio_bytes, &bytes_read, 5000);
    ESP_LOGI(TAG, "Selesai ngerekam: %d bytes", bytes_read);

    // 3. Setup Boundary buat file upload (Multipart Form)
    const char* boundary = "----RootXBoundary12345";
    char head[256];
    snprintf(head, sizeof(head),
             "--%s\r\n"
             "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
             "Content-Type: audio/wav\r\n\r\n", boundary);

    char tail[128];
    snprintf(tail, sizeof(tail),
             "\r\n--%s\r\n"
             "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
             "whisper-large-v3\r\n"
             "--%s--\r\n", boundary, boundary);

    char wav_header[44];
    generate_wav_header(wav_header, bytes_read, 16000);

    uint32_t total_payload_len = strlen(head) + 44 + bytes_read + strlen(tail);

    
        esp_http_client_config_t config = {
        .url = "https://api.groq.com/openai/v1/audio/transcriptions",
        .method = HTTP_METHOD_POST,
        .timeout_ms = 15000,
        .cert_pem = groq_root_ca, // <-- Langsung pake KTP Google yang akurat
    };



    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", GROQ_API_KEY);
    esp_http_client_set_header(client, "Authorization", auth_header);
    
    char content_type[64];
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", boundary);
    esp_http_client_set_header(client, "Content-Type", content_type);

    // 5. Eksekusi Upload
    esp_http_client_open(client, total_payload_len);
    esp_http_client_write(client, head, strlen(head));         // Kirim pembuka form
    esp_http_client_write(client, wav_header, 44);             // Kirim Header WAV
    esp_http_client_write(client, audio_buffer, bytes_read);   // Kirim Data Audio Mentah
    esp_http_client_write(client, tail, strlen(tail));         // Kirim penutup form

    // 6. Tangkap Hasil Teks (STT)
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length > 0) {
        char* stt_response = malloc(content_length + 1);
        esp_http_client_read(client, stt_response, content_length);
        stt_response[content_length] = '\0';
        
        ESP_LOGI(TAG, "Hasil STT Mentah: %s", stt_response);

        // Parse JSON dari STT
        cJSON *stt_json = cJSON_Parse(stt_response);
        if (stt_json) {
            cJSON *text_node = cJSON_GetObjectItem(stt_json, "text");
            if (text_node && text_node->valuestring) {
                char* teks_omongan = text_node->valuestring;
                ESP_LOGI(TAG, "Lu ngomong: %s", teks_omongan);

                // ==========================================
                // LOGIKA "HALO ROOTX" (WAKE WORD)
                // ==========================================
                extern bool requireWakeWord; // Ambil variabel global dari globals.h

                if (requireWakeWord) {
                    // Cari kata "RootX" atau "Root X" (Case Insensitive sedikit)
                    if (strstr(teks_omongan, "RootX") || strstr(teks_omongan, "Root X") || strstr(teks_omongan, "rootx")) {
                        ESP_LOGI(TAG, "Wake word terdeteksi! Melempar ke Gemini...");
                        tanya_gemini(teks_omongan); 
                    } else {
                        ESP_LOGI(TAG, "Bukan manggil gw. Diabaikan.");
                    }
                } else {
                    // Kalau fiturnya dimatiin, langsung aja tembak ke Gemini
                    tanya_gemini(teks_omongan);
                }
            }
            cJSON_Delete(stt_json);
        }
        free(stt_response);
    } else {
        ESP_LOGE(TAG, "Gagal dapet respon STT.");
    }
    
    esp_http_client_cleanup(client);
    free(audio_buffer); // WAJIB FREE BIAR PSRAM GAK BOCOR!
}

// Pastiin lu udah nge-include freertos di bagian atas file:


// Ambil status ON/OFF dari input_system
extern bool aiAudioEnabled; 

void ai_audio_task(void *pvParameters) {
    ESP_LOGI(TAG, "Task AI Audio (Latar Belakang) Berjalan...");

    while (1) {
        // Alat HANYA ngerekam kalau menu AI Audio di-set ke ON
        if (aiAudioEnabled) {
            ESP_LOGI(TAG, "AI AKTIF: Mendengarkan suara sekitar...");
            
            // Panggil fungsi rekam 4 detik & lempar ke Groq yang tadi kita bikin
            mulai_rekam_dan_stt();
            
            // Kasih nafas 1 detik sebelum ngerekam lagi biar API Groq lu gak kena limit spam
            vTaskDelay(pdMS_TO_TICKS(1000)); 
        } else {
            // Kalau AI OFF, task ini cuma tidur sambil ngecek status tiap 1/2 detik
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}
