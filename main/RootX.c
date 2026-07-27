#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h" 
#include "globals.h"
#include "photo_data.h"
#include "esp_spiffs.h"
#include "ai_audio.h"
#include "esp_netif.h"
#include "esp_event.h"

// Hardcode versi firmware lu saat ini (00 = v1.0.0)
#define VERSION_SAAT_INI 136

// URL mentah (RAW) langsung tembak ke file lu di GitHub
#define URL_VERSION  "https://raw.githubusercontent.com/xiter00/RTXUP/main/vr.txt"
#define URL_FIRMWARE "https://raw.githubusercontent.com/xiter00/RTXUP/main/core.bin"


// --- DEKLARASI FUNGSI DARI MANAGER LAIN ---
extern void start_webserver(void);

extern void loopWiFi(void *pvParameters);
extern void task_display(void *pvParameters);
extern void init_ir_system(void);
void perform_ota_manual(void);
// ==========================================
// THE BYPASSER: JIMAT SAKTI DEAUTH & BEACON
// ==========================================
int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) { 
    return 0; // Loloskan semua paket 0xC0 & 0x80 tanpa dicek!
}


// --- DEFINISI VARIABEL GLOBAL ---
// Di bagian definisi variabel atas
bool triggerTrack = false;

// Di dalam while(1)
// --- VARIABEL KONEKSI WIFI (BIAR LINKER GAK NGAMUK) ---
// --- VARIABEL ENGINE GAME ---
extern int baca_highscore_dino();
extern void simpan_highscore_dino(int hs);
extern int baca_highscore_snake();
extern void simpan_highscore_snake(int hs);

float rawScore = 0;
int dinoScore = 0, dinoHighScore = -1;
int dinoY = 36;        // Posisi tanah baru buat Dino 24px
float dinoVy = 0;      
bool isJumping = false;
int obstacleX = 128, obstacleY = 32, obstacleType = 0; // 0=Kaktus1, 1=Kaktus2, 2=Burung
float gameSpeed = 3.0; 
int dinoState = 0, endTimer = 0;      
int skyX = 128; // Posisi matahari/bulan

// Posisi Bintang (Latar Belakang)
int starX[5] = {20, 50, 80, 100, 120};
int starY[5] = {5, 15, 10, 20, 8};

// Definisi asli variabel Evil Twin
bool isEvilTwin = false;
int evilTwinState = 0; 
char stolenPassword[64] = "";
bool triggerEvilTwin = false;


int tetrisState = 0;
int tetrisScore = 0;
int tetrisHighScore = -1;
bool isTetrisInitialized = false;

// --- VARIABEL AI AUDIO ---
bool aiAudioEnabled = false;
bool requireWakeWord = true; // Defaultnya wajib ngomong "Halo RootX"



char inputPassword[64] = {0};
int cursorPass = 0;
int statusKoneksi = 0;
bool isWiFiConnected = false;
char connSSID[33] = {0};
int connCH = 0;
int connRSSI = 0;
bool triggerConnect = false;
bool triggerDisconnect = false;

int snakeDir = 0;
int snakeState = 0;
int snakeScore = 0;
int snakeHighScore = -1;
bool isSnakeInitialized = false;

int batteryPercent = 0;
int dinoLimit = 500;   

int deauthProgress = 0;
bool adaTargetSta = false;
bool isDeauthSta = false;
bool inSubMenu = false;
int currentMenu = 0;
int currentSub = 0;
int topMenu = 0;
WiFiData listWiFi[30];
StationInfo listStation[30];
int brightnessValue = 150;
int spamState = 0; 
bool isSpamming = false;
int aktifModeSpam = 0;
bool spamUdahSetup = false;
bool deauthUdahSetup = false;
int scannerState = 0; 
int scannerStateSta = 0; 
uint32_t popUpTimer = 0; 
bool triggerScan = false; 
bool triggerScanSta = false; 
bool scanDone = false;    
bool scanStaDone = false;    
int totalWiFi = 0;
int totalStation = 0;
int cursorInScanner = 0; 
int cursorInScanSta = 0; 
int scrollPosScanner = 0;
int targetLockedIdx = -1;
int contextCursor = 0;
StationInfo targetSta;
WiFiData targetTerkunci; 
bool adaTarget = false;  
int deauthState = 0;
bool isDeauthing = false;
bool sedang_scan = false;
int appMode = 0;

TaskHandle_t TaskWiFi;

// --- FUNGSI SETUP JOYSTICK C-MURNI ---


// ==========================================
// JANTUNG UTAMA FIRMWARE LU (Pengganti Setup & Loop)
// ==========================================
// ==========================================
// TASK OTA OTOMATIS (Loop tiap 1 menit)
// ==========================================
void task_cek_ota(void *pvParameter) {
    ESP_LOGI("OTA", "Mesin OTA nyala! Menunggu WiFi terkoneksi...");

    while (!isWiFiConnected) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    ESP_LOGI("OTA", "WiFi Konek! Mesin OTA Super Cepat siap...");
    

    while (1) {
        if (isWiFiConnected) {
            // ==========================================
            // 1. CEK VERSI (vr.txt) VIA GITHUB API
            // ==========================================
            char url_version[128];
            snprintf(url_version, sizeof(url_version),
                     "https://api.github.com/repos/xiter00/RTXUP/contents/vr.txt");

            esp_http_client_config_t config_version = {
                .url = url_version,
                .method = HTTP_METHOD_GET,
                .crt_bundle_attach = esp_crt_bundle_attach,
                .user_agent = "RootX_OTA_Checker",
            };

            esp_http_client_handle_t client_version = esp_http_client_init(&config_version);
            if (!client_version) {
                ESP_LOGE("OTA", "Gagal init client version");
                continue;
            }

            // Set header agar API mengembalikan konten file (bukan JSON)
            esp_http_client_set_header(client_version, "Accept", "application/vnd.github.v3.raw");

            esp_err_t err = esp_http_client_open(client_version, 0);
            int versi_github = -1;
            if (err == ESP_OK) {
                esp_http_client_fetch_headers(client_version);
                char buffer[16] = {0};
                int len = esp_http_client_read(client_version, buffer, sizeof(buffer)-1);
                if (len > 0) {
                    versi_github = atoi(buffer);
                    ESP_LOGI("OTA", "Versi via API: %d | Lokal: %d", versi_github, VERSION_SAAT_INI);
                }
            } else {
                ESP_LOGE("OTA", "Gagal konek ke GitHub API untuk cek versi.");
            }
            esp_http_client_cleanup(client_version);

            // ==========================================
            // 2. JIKA ADA UPDATE, DOWNLOAD core.bin JUGA VIA API
            // ==========================================
           if (versi_github > VERSION_SAAT_INI) {
    ESP_LOGI("OTA", "UPDATE DITEMUKAN! Gas download core.bin via API manual...");
    perform_ota_manual();

            } else {
                ESP_LOGI("OTA", "RootX sudah versi terbaru.");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10000)); // Cek tiap 5 detik
    }
}

void perform_ota_manual(void) {
    ESP_LOGI("OTA", "Memulai OTA manual via GitHub API...");

    char url_corebin[128];
    snprintf(url_corebin, sizeof(url_corebin),
             "https://api.github.com/repos/xiter00/RTXUP/contents/core.bin");

    esp_http_client_config_t config = {
        .url = url_corebin,
        .method = HTTP_METHOD_GET,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_agent = "RootX_OTA_Manual",
        .timeout_ms = 30000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE("OTA", "Gagal init client");
        return;
    }

    // Header penting: Accept raw (binary)
    esp_http_client_set_header(client, "Accept", "application/vnd.github.v3.raw");

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "Gagal buka koneksi: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return;
    }

    // Baca header response
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0) {
        ESP_LOGE("OTA", "Content-Length tidak valid: %d", content_length);
        esp_http_client_cleanup(client);
        return;
    }

    ESP_LOGI("OTA", "Ukuran core.bin: %d bytes", content_length);

    // --- Mulai OTA ---
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE("OTA", "Gagal mendapatkan partition OTA");
        esp_http_client_cleanup(client);
        return;
    }

    esp_ota_handle_t ota_handle;
    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "esp_ota_begin gagal: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return;
    }

    // Buffer untuk download chunk
    char *buffer = (char *)malloc(4096);
    if (buffer == NULL) {
        ESP_LOGE("OTA", "RAM Habis! Gagal alokasi buffer.");
        esp_ota_abort(ota_handle);
        esp_http_client_cleanup(client);
        return;
    }
    int total_read = 0;
    
    // Bikin variabel buat nahan print log biar gak nyepam
    int last_print = 0; 

    while (total_read < content_length) {
        // TULIS ANGKA 4096 LANGSUNG! Jangan pake sizeof(buffer)
        int read_len = esp_http_client_read(client, buffer, 4096); 
        
        if (read_len < 0) {
            ESP_LOGE("OTA", "Error membaca data");
            break;
        }
        if (read_len == 0) break; // EOF

        err = esp_ota_write(ota_handle, buffer, read_len);
        if (err != ESP_OK) {
            ESP_LOGE("OTA", "Gagal menulis ke OTA: %s", esp_err_to_name(err));
            break;
        }
        total_read += read_len;
        
        // TRIK NGEBUT: Jangan nge-print tiap loop! Print tiap 40KB aja (biar UART gak macet)
        if (total_read - last_print >= 40960 || total_read == content_length) {
            ESP_LOGI("OTA", "Download Progress: %d / %d bytes", total_read, content_length);
            last_print = total_read;
        }
    }


    esp_http_client_cleanup(client);

    if (total_read == content_length) {
        err = esp_ota_end(ota_handle);
        if (err == ESP_OK) {
            err = esp_ota_set_boot_partition(update_partition);
            if (err == ESP_OK) {
                ESP_LOGI("OTA", "SUKSES! Firmware terbaru siap. Reboot...");
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart();
            } else {
                ESP_LOGE("OTA", "Gagal set boot partition: %s", esp_err_to_name(err));
            }
        } else {
            ESP_LOGE("OTA", "esp_ota_end gagal: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE("OTA", "Download tidak lengkap: %d / %d", total_read, content_length);
        esp_ota_abort(ota_handle);
    }
}
// ==========================================
// APP MAIN LU
// ==========================================
void app_main(void) {
    ESP_LOGI("RootX", "System Booting...");

    

    // --- 1. LAPOR KE BOOTLOADER KALAU OS AMAN (Biar Gak Rollback) ---
    esp_ota_mark_app_valid_cancel_rollback();
    
    // --- 2. INIT SISTEM ---
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };
    if (esp_vfs_spiffs_register(&conf) != ESP_OK) {
        ESP_LOGE("RootX", "Gagal mount SPIFFS! Font gak bakal kebaca!");
    } else {
        ESP_LOGI("RootX", "SPIFFS Mounted! Siap baca font!");
    }
    
    xTaskCreatePinnedToCore(task_display, "DisplayTask", 8192, NULL, 1, NULL, 1);

    extern bool init_sdcard(); 
    if (init_sdcard()) {
        ESP_LOGI("RootX", "Mantap, SD Card Jalan!");
    }

    init_ir_system(); 
        init_battery();
    init_i2s_audio(); // <--- TAMBAHIN INI BIAR MIC & SPEAKER STANDBY
    
    
    xTaskCreatePinnedToCore(loopWiFi, "TaskWiFi", 16384, NULL, 1, &TaskWiFi, 0);

    // --- 3. HACK AUTO-CONNECT WIFI BUAT DEV (Suntik Variabel Global Lu) ---
    // GANTI SAMA NAMA WIFI & PASSWORD RUMAH LU!


    // KASIH NAFAS 2 DETIK BIAR MESIN WIFI SIAP DULU!
    vTaskDelay(pdMS_TO_TICKS(2000));

    // --- 3. HACK AUTO-CONNECT WIFI BUAT DEV ---
    strcpy(connSSID, "AYYUBI"); 
    strcpy(inputPassword, "rumahabi123");
    vTaskDelay(pdMS_TO_TICKS(100));
    triggerConnect = true; 


    

    // --- 4. JALANIN MESIN OTA ---
    // (Tadi lu salah ketik ota_satpam_task, gw ganti jadi task_cek_ota)
    xTaskCreatePinnedToCore(task_cek_ota, "task_ota", 16384, NULL, 5, NULL, 0);


    // --- JALANIN TELINGA AI (PEKERJA BAYANGAN) ---
    // Pakai core 0 biar gak tabrakan sama task display (Core 1)
    xTaskCreatePinnedToCore(ai_audio_task, "ai_task", 32768, NULL, 4, NULL, 0); 
    
}
