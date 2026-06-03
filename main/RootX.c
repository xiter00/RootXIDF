#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h" // Wajib biar bisa tembus HTTPS GitHub tanpa error SSL


// --- HEADER LU ---
#include "globals.h"
#include "photo_data.h"

// Hardcode versi firmware lu saat ini (100 = v1.0.0)
#define VERSION_SAAT_INI 102

// URL mentah (RAW) langsung tembak ke file lu di GitHub
#define URL_VERSION  "https://raw.githubusercontent.com/xiter00/RTXUP/main/vr.txt"
#define URL_FIRMWARE "https://raw.githubusercontent.com/xiter00/RTXUP/main/core.bin"


// --- DEKLARASI FUNGSI DARI MANAGER LAIN ---

extern void loopWiFi(void *pvParameters);
extern void task_display(void *pvParameters);
extern void init_ir_system(void);

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

    // 1. TUNGGU SAMPAI WIFI KONEK (Ngecek tiap 2 detik)
    while (!isWiFiConnected) {
        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }

    // 2. KALO UDAH LOLOS DARI WHILE DI ATAS, BERARTI WIFI UDAH NYAMBUNG!
    ESP_LOGI("OTA", "WiFi Konek! Gas ngecek GitHub...");
    
    while (1) {
        if (isWiFiConnected) {
            esp_http_client_config_t config = {
                .url = URL_VERSION,
                .crt_bundle_attach = esp_crt_bundle_attach, 
            };
            esp_http_client_handle_t client = esp_http_client_init(&config);
            esp_err_t err = esp_http_client_open(client, 0);

            if (err == ESP_OK) {
                esp_http_client_fetch_headers(client);
                char buffer[10] = {0};
                esp_http_client_read(client, buffer, sizeof(buffer)-1);
                
                int versi_github = atoi(buffer); 
                ESP_LOGI("OTA", "Versi ESP32: %d | Versi GitHub: %d", VERSION_SAAT_INI, versi_github);

                if (versi_github > VERSION_SAAT_INI) {
                    ESP_LOGI("OTA", "Update ditemukan! Memulai download...");

                    esp_http_client_config_t ota_client_config = {
                        .url = URL_FIRMWARE,
                        .crt_bundle_attach = esp_crt_bundle_attach,
                        .keep_alive_enable = true,
                    };
                    esp_https_ota_config_t ota_config = {
                        .http_config = &ota_client_config,
                    };

                    esp_err_t ret = esp_https_ota(&ota_config);
                    if (ret == ESP_OK) {
                        ESP_LOGI("OTA", "SUKSES! RootX akan Reboot sekarang...");
                        vTaskDelay(pdMS_TO_TICKS(1000));
                        esp_restart(); 
                    } else {
                        ESP_LOGE("OTA", "Gagal Update! Code: %d", ret);
                    }
                } else {
                    ESP_LOGI("OTA", "RootX sudah versi terbaru.");
                }
            } else {
                ESP_LOGE("OTA", "Gagal konek ke GitHub. Cek internet.");
            }
            esp_http_client_cleanup(client);
        }
        
        // Tidur 1 menit sebelum ngecek versi lagi
        vTaskDelay(pdMS_TO_TICKS(10000)); 
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
    
    xTaskCreatePinnedToCore(loopWiFi, "TaskWiFi", 16384, NULL, 1, &TaskWiFi, 0);

    // --- 3. HACK AUTO-CONNECT WIFI BUAT DEV (Suntik Variabel Global Lu) ---
    // GANTI SAMA NAMA WIFI & PASSWORD RUMAH LU!


    // KASIH NAFAS 2 DETIK BIAR MESIN WIFI SIAP DULU!
    vTaskDelay(pdMS_TO_TICKS(2000));

    // --- 3. HACK AUTO-CONNECT WIFI BUAT DEV ---
    strcpy(connSSID, "NOT MASTAH"); 
    strcpy(inputPassword, "yangbrorasakan");
    vTaskDelay(pdMS_TO_TICKS(100));
    triggerConnect = true; 


    strcpy(connSSID, "NOT MASTAH"); 
    strcpy(inputPassword, "yangbrorasakan");
    triggerConnect = true; // Ini bakal mancing loopWiFi lu buat ngeksekusi koneksi

    // --- 4. JALANIN MESIN OTA ---
    // (Tadi lu salah ketik ota_satpam_task, gw ganti jadi task_cek_ota)
    xTaskCreatePinnedToCore(task_cek_ota, "task_ota", 8192, NULL, 5, NULL, 0);
}
