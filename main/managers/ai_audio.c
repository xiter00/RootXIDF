
#include <stdio.h>
#include <dirent.h>
#include "esp_log.h"
#include "ai_audio.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_http_server.h"
#include "esp_vfs_spiffs.h"
#include <ctype.h>
#include <string.h>
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "minimp3.h"
#include "esp_crt_bundle.h"
#include "groq_cert.h"
#include <math.h>
#include "esp_timer.h"

#define GEMINI_API_KEY "AQ.Ab8RN6Kw_7M0reGotMRh1ZQx9Xhz6Nj6QA_K3Fw4pI1f5zE3_Q"
#define GROQ_API_KEY "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"

static const char *TAG = "AI_AUDIO";
extern bool requireWakeWord;

i2s_chan_handle_t tx_chan = NULL;
i2s_chan_handle_t rx_chan = NULL;

// ============================================================
// FILE TRACKER
// ============================================================
#define MAX_FILES       6
#define DELETE_AFTER_MS 30000

typedef struct {
    char wav_path[40];
    char bin_path[40];
    int64_t created_ms;
    bool valid;
} AudioFile;

static AudioFile tracked[MAX_FILES];
static int file_idx = 0;
static portMUX_TYPE tracker_mux = portMUX_INITIALIZER_UNLOCKED;

static void track_file(const char *wav, const char *bin) {
    portENTER_CRITICAL(&tracker_mux);
    int slot = 0;
    int64_t oldest = INT64_MAX;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!tracked[i].valid) { slot = i; break; }
        if (tracked[i].created_ms < oldest) { oldest = tracked[i].created_ms; slot = i; }
    }
    strlcpy(tracked[slot].wav_path, wav, sizeof(tracked[slot].wav_path));
    strlcpy(tracked[slot].bin_path, bin, sizeof(tracked[slot].bin_path));
    tracked[slot].created_ms = esp_timer_get_time() / 1000;
    tracked[slot].valid = true;
    portEXIT_CRITICAL(&tracker_mux);
}

static void cleanup_old_files(void) {
    int64_t now = esp_timer_get_time() / 1000;
    portENTER_CRITICAL(&tracker_mux);
    for (int i = 0; i < MAX_FILES; i++) {
        if (!tracked[i].valid) continue;
        if ((now - tracked[i].created_ms) > DELETE_AFTER_MS) {
            remove(tracked[i].wav_path);
            remove(tracked[i].bin_path);
            ESP_LOGI("AI_AUDIO", "Auto-delete: %s %s",
                tracked[i].wav_path, tracked[i].bin_path);
            tracked[i].valid = false;
        }
    }
    portEXIT_CRITICAL(&tracker_mux);
}

// ============================================================
// WEB HANDLERS
// ============================================================

static esp_err_t serve_file(httpd_req_t *req, const char *path, const char *mime) {
    FILE *f = fopen(path, "rb");
    if (!f) { httpd_resp_send_404(req); return ESP_OK; }
    httpd_resp_set_type(req, mime);
    char buf[1024];
    int n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        httpd_resp_send_chunk(req, buf, n);
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t handler_wav(httpd_req_t *req) {
    const char *fname = strrchr(req->uri, '/');
    if (!fname || strlen(fname) < 2) { httpd_resp_send_404(req); return ESP_OK; }
    char path[48];
    snprintf(path, sizeof(path), "/spiffs/%s", fname + 1);
    return serve_file(req, path, "audio/wav");
}

static esp_err_t handler_bin(httpd_req_t *req) {
    const char *fname = strrchr(req->uri, '/');
    if (!fname || strlen(fname) < 2) { httpd_resp_send_404(req); return ESP_OK; }
    char path[48];
    snprintf(path, sizeof(path), "/spiffs/%s", fname + 1);
    return serve_file(req, path, "audio/wav"); // bin juga WAV, langsung play
}

static esp_err_t handler_index(httpd_req_t *req) {
    cleanup_old_files();

    char *body = malloc(10240);
    if (!body) { httpd_resp_send_500(req); return ESP_OK; }
    int pos = 0;

    // ---- HTML HEAD ----
    pos += snprintf(body + pos, 10240 - pos,
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>RootX Audio Debug</title>"
        "<style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{font-family:monospace;background:#0d0d0d;color:#ccc;padding:12px}"
        "h1{color:#0ff;font-size:18px;margin-bottom:4px}"
        ".info{color:#555;font-size:11px;margin-bottom:12px}"
        ".card{background:#1a1a1a;border:1px solid #2a2a2a;border-radius:8px;"
              "padding:12px;margin-bottom:12px}"
        ".card-title{color:#ff0;font-size:13px;margin-bottom:8px}"
        ".age{color:#f44;font-size:11px;float:right}"
        ".label{color:#0f0;font-size:11px;margin:8px 0 3px}"
        ".label2{color:#ff0;font-size:11px;margin:8px 0 3px}"
        "audio{width:100%%;margin-bottom:4px}"
        ".note{color:#555;font-size:10px;margin-top:4px}"
        ".empty{color:#444;text-align:center;padding:30px}"
        ".stat{color:#444;font-size:10px;margin-top:12px;border-top:1px solid #222;padding-top:8px}"
        ".rms{color:#0ff;font-size:11px}"
        "</style></head><body>"
        "<h1>&#x1F3A4; RootX Audio Debug</h1>"
        "<p class='info'>Auto-refresh 3 detik &bull; File hilang 30 detik setelah rekam</p>"
    );

    int64_t now = esp_timer_get_time() / 1000;
    int count = 0;

    portENTER_CRITICAL(&tracker_mux);
    // Tampilkan dari yang terbaru dulu
    for (int i = MAX_FILES - 1; i >= 0; i--) {
        if (!tracked[i].valid) continue;
        count++;

        int64_t age_s  = (now - tracked[i].created_ms) / 1000;
        int64_t del_in = (DELETE_AFTER_MS / 1000) - age_s;
        if (del_in < 0) del_in = 0;

        const char *wfname = strrchr(tracked[i].wav_path, '/');
        wfname = wfname ? wfname + 1 : tracked[i].wav_path;
        const char *bfname = strrchr(tracked[i].bin_path, '/');
        bfname = bfname ? bfname + 1 : tracked[i].bin_path;

        pos += snprintf(body + pos, 10240 - pos,
            "<div class='card'>"
            "<div class='card-title'>&#x1F4C4; %s "
            "<span class='age'>hapus dalam %llds</span></div>",
            wfname, (long long)del_in
        );

        // --- Audio dari MIC ---
        pos += snprintf(body + pos, 10240 - pos,
            "<div class='label'>&#x1F3A4; Audio dari MIC (raw WAV)</div>"
            "<audio controls preload='auto'>"
            "<source src='/wav/%s' type='audio/wav'></audio>"
            "<div class='note'>Ini suara yang direkam INMP441 sebelum dikirim ke Groq</div>",
            wfname
        );

        // --- Audio yang masuk Groq ---
        pos += snprintf(body + pos, 10240 - pos,
            "<div class='label2'>&#x1F4E4; Audio yang dikirim ke Groq (sama WAV-nya)</div>"
            "<audio controls preload='auto'>"
            "<source src='/bin/%s' type='audio/wav'></audio>"
            "<div class='note'>Kalau ini sama jelek dengan atas = masalah di mic/konversi<br>"
            "Kalau atas bagus tapi bawah rusak = masalah di upload/wrapping</div>",
            bfname
        );

        pos += snprintf(body + pos, 10240 - pos, "</div>");
    }
    portEXIT_CRITICAL(&tracker_mux);

    if (count == 0) {
        pos += snprintf(body + pos, 10240 - pos,
            "<div class='empty'>&#x23F3; Belum ada rekaman.<br>Tunggu AI aktif dan RMS &gt; 500</div>"
        );
    }

    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    pos += snprintf(body + pos, 10240 - pos,
        "<div class='stat'>SPIFFS: %dKB total | %dKB terpakai | %dKB sisa</div>"
        // Auto refresh tiap 3 detik
        "<script>setTimeout(()=>location.reload(),3000)</script>"
        "</body></html>",
        (int)(total / 1024), (int)(used / 1024), (int)((total - used) / 1024)
    );

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, body, pos);
    free(body);
    return ESP_OK;
}

static httpd_handle_t webserver = NULL;

static void start_webserver(void) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn   = httpd_uri_match_wildcard;
    cfg.server_port    = 80;
    cfg.max_uri_handlers = 8;

    if (httpd_start(&webserver, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Web server gagal start!"); return;
    }

    static const httpd_uri_t u_index = { .uri="/"      , .method=HTTP_GET, .handler=handler_index };
    static const httpd_uri_t u_wav   = { .uri="/wav/*" , .method=HTTP_GET, .handler=handler_wav   };
    static const httpd_uri_t u_bin   = { .uri="/bin/*" , .method=HTTP_GET, .handler=handler_bin   };

    httpd_register_uri_handler(webserver, &u_index);
    httpd_register_uri_handler(webserver, &u_wav);
    httpd_register_uri_handler(webserver, &u_bin);

    ESP_LOGI(TAG, "&#x1F310; Web server: http://192.168.1.10/");
}

// ============================================================
// SPIFFS INIT
// ============================================================
static void init_spiffs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = MAX_FILES * 2 + 2,
        .format_if_mount_failed = true,
    };
    if (esp_vfs_spiffs_register(&conf) != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS gagal mount!"); return;
    }

    // Bersihin file lama dari sesi sebelumnya
    DIR *d = opendir("/spiffs");
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            char p[48];
            snprintf(p, sizeof(p), "/spiffs/%s", e->d_name);
            remove(p);
        }
        closedir(d);
    }

    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    ESP_LOGI(TAG, "SPIFFS OK: %dKB total, %dKB used", (int)(total/1024), (int)(used/1024));
}

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
    ESP_LOGI(TAG, "Init I2S + SPIFFS + WebServer...");

    init_spiffs();
    start_webserver();

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

    i2s_chan_config_t rx_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&rx_cfg, NULL, &rx_chan));
    i2s_std_config_t rx_std = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
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
// URL ENCODE
// ============================================================
void url_encode(const char *src, char *dest) {
    const char *hex = "0123456789ABCDEF";
    while (*src) {
        if (isalnum((unsigned char)*src)||*src=='-'||*src=='_'||*src=='.'||*src=='~')
            *dest++ = *src;
        else if (*src == ' ') { *dest++='%'; *dest++='2'; *dest++='0'; }
        else {
            *dest++='%';
            *dest++=hex[(*src>>4)&15];
            *dest++=hex[*src&15];
        }
        src++;
    }
    *dest = '\0';
}

// ============================================================
// GOOGLE TTS
// ============================================================
void play_google_tts(const char *text) {
    char encoded[600];
    url_encode(text, encoded);
    char url[800];
    snprintf(url, sizeof(url),
        "http://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&tl=id&q=%s",
        encoded);

    esp_http_client_config_t cfg = { .url=url, .method=HTTP_METHOD_GET, .timeout_ms=10000 };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return;

    if (esp_http_client_open(c, 0) == ESP_OK) {
        esp_http_client_fetch_headers(c);
        uint8_t *mp3_buf = malloc(8192);
        if (mp3_buf) {
            mp3dec_t mp3d; mp3dec_init(&mp3d);
            mp3dec_frame_info_t fi;
            int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
            int total = 0, n;
            while ((n = esp_http_client_read(c, (char*)mp3_buf+total, 8192-total)) > 0) {
                total += n;
                int s = mp3dec_decode_frame(&mp3d, mp3_buf, total, pcm, &fi);
                if (s > 0) {
                    size_t bw;
                    i2s_channel_write(tx_chan, pcm, s*2, &bw, 2000);
                    total -= fi.frame_bytes;
                    memmove(mp3_buf, mp3_buf + fi.frame_bytes, total);
                }
            }
            free(mp3_buf);
        }
    }
    esp_http_client_cleanup(c);
}

// ============================================================
// GEMINI
// ============================================================
void tanya_gemini(const char *q) {
    ESP_LOGI(TAG, "→ Gemini: %s", q);
    char url[512];
    snprintf(url, sizeof(url),
        "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=%s",
        GEMINI_API_KEY);

    const char *tpl =
        "{\"system_instruction\":{\"parts\":{\"text\":\"Lu adalah asisten AI hacker bernama RootX."
        "Balas WAJIB pakai format JSON murni: {\\\"aksi\\\":\\\"...\\\",\\\"ucapan\\\":\\\"...\\\"}."
        "Pilihan aksi: standby,ir_blaster,wifi_scan,deauth,wakeword_off,wakeword_on."
        "Obrolan biasa pilih standby."
        "Minta ngobrol bebas pilih wakeword_off."
        "Minta aktifkan wake word pilih wakeword_on.\"}},"
        "\"contents\":[{\"parts\":[{\"text\":\"%s\"}]}]}";

    char body[1024];
    snprintf(body, sizeof(body), tpl, q);

    esp_http_client_config_t cfg = {
        .url = url, .method = HTTP_METHOD_POST, .timeout_ms = 15000,
        .skip_cert_common_name_check = true,
        .use_global_ca_store = false,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    esp_http_client_set_header(c, "Content-Type", "application/json");
    esp_http_client_set_post_field(c, body, strlen(body));

    if (esp_http_client_perform(c) == ESP_OK) {
        int len = esp_http_client_get_content_length(c);
        if (len > 0) {
            char *buf = malloc(len + 1);
            esp_http_client_read(c, buf, len);
            buf[len] = '\0';
            cJSON *root = cJSON_Parse(buf);
            if (root) {
                cJSON *p0 = cJSON_GetArrayItem(
                    cJSON_GetObjectItem(
                        cJSON_GetObjectItem(
                            cJSON_GetArrayItem(cJSON_GetObjectItem(root,"candidates"),0),
                        "content"),
                    "parts"), 0);
                cJSON *tnode = cJSON_GetObjectItem(p0, "text");
                if (tnode && tnode->valuestring) {
                    cJSON *cmd = cJSON_Parse(tnode->valuestring);
                    if (cmd) {
                        cJSON *aksi   = cJSON_GetObjectItem(cmd, "aksi");
                        cJSON *ucapan = cJSON_GetObjectItem(cmd, "ucapan");
                        if (aksi && ucapan) {
                            ESP_LOGI(TAG, "AKSI=%s", aksi->valuestring);
                            play_google_tts(ucapan->valuestring);
                            if      (!strcmp(aksi->valuestring,"wakeword_off")) requireWakeWord = false;
                            else if (!strcmp(aksi->valuestring,"wakeword_on"))  requireWakeWord = true;
                            else if (!strcmp(aksi->valuestring,"ir_blaster"))   ESP_LOGI(TAG,"IR!");
                            else if (!strcmp(aksi->valuestring,"wifi_scan"))    ESP_LOGI(TAG,"SCAN!");
                        }
                        cJSON_Delete(cmd);
                    }
                }
                cJSON_Delete(root);
            }
            free(buf);
        }
    }
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
    cleanup_old_files();

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
    while (total < sz32) {
        if (i2s_channel_read(rx_chan, (char*)raw+total, sz32-total, &bytes_read, 1000) != ESP_OK) break;
        total += bytes_read;
    }
    ESP_LOGI(TAG, "Raw: %d bytes", (int)total);

    // === KONVERSI 32→16 BIT ===
    int n_samples = total / 4;
    for (int i = 0; i < n_samples; i++) {
        int32_t v = raw[i] >> 14;
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

    // === SAVE WAV (dari MIC, sebelum kirim) ===
    char wav_path[40], bin_path[40];
    int idx = file_idx++ % 999;
    snprintf(wav_path, sizeof(wav_path), "/spiffs/mic_%03d.wav", idx);
    snprintf(bin_path, sizeof(bin_path), "/spiffs/grq_%03d.wav", idx);

    FILE *fw = fopen(wav_path, "wb");
    if (fw) {
        fwrite(wav_hdr, 1, 44, fw);
        fwrite(pcm, 1, pcm_bytes, fw);
        fclose(fw);
        ESP_LOGI(TAG, "Saved mic WAV: %s", wav_path);
    } else {
        ESP_LOGE(TAG, "Gagal simpan WAV: %s", wav_path);
    }

    // === MULTIPART SETUP ===
    const char *boundary = "----RootXBoundary12345";
    char head[256], tail[512];
    snprintf(head, sizeof(head),
        "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n", boundary);
    snprintf(tail, sizeof(tail),
        "\r\n--%s\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\nwhisper-large-v3\r\n"
        "--%s\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\nid\r\n"
        "--%s\r\nContent-Disposition: form-data; name=\"temperature\"\r\n\r\n0\r\n"
        "--%s--\r\n",
        boundary, boundary, boundary, boundary);

    // === SAVE COPY PERSIS YANG DIKIRIM KE GROQ ===
    // Ini WAV header + PCM yang sama persis, buat bandingin
    FILE *fb = fopen(bin_path, "wb");
    if (fb) {
        fwrite(wav_hdr, 1, 44, fb);
        fwrite(pcm, 1, pcm_bytes, fb);
        fclose(fb);
        ESP_LOGI(TAG, "Saved groq copy: %s", bin_path);
    }

    // Track keduanya buat web UI
    track_file(wav_path, bin_path);

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
                    if (requireWakeWord) {
                        if (strstr(teks,"RootX")||strstr(teks,"Root X")||strstr(teks,"rootx"))
                            tanya_gemini(teks);
                        else ESP_LOGI(TAG, "Bukan wake word.");
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