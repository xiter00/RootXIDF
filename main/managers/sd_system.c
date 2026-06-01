#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "globals.h"

// --- SETTING PIN SD CARD (SPI MODE) ---


sdmmc_card_t *card;
const char mount_point[] = MOUNT_POINT;
//==================
// 1. FUNGSI INISIALISASI (MOUNTING)
// ========================================================
bool init_sdcard() {
    esp_err_t ret;

    // Opsi mounting filesystem
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, // Kalo gagal/corrupt, format otomatis
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    ESP_LOGI("SD_CARD", "Inisialisasi SD Card...");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    
    // Config jalur kabel SPI
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE("SD_CARD", "Gagal inisialisasi bus SPI.");
        return false;
    }

    // Config Pin CS (Chip Select)
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI("SD_CARD", "Mounting filesystem...");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE("SD_CARD", "Gagal mount filesystem. Coba cek format SD Card lu.");
        } else {
            ESP_LOGE("SD_CARD", "Gagal inisialisasi SD Card (%s). Cek kabel!", esp_err_to_name(ret));
        }
        return false;
    }

    ESP_LOGI("SD_CARD", "SD Card OK! Kapasitas: %llu MB", ((uint64_t)card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
    return true;
}

// ========================================================
// 2. FUNGSI TULIS FILE (BUAT LOG/SAVE DATA)
// ========================================================
void tulis_file(const char *path, const char *data) {
    char full_path[64];
    snprintf(full_path, sizeof(full_path), "%s/%s", MOUNT_POINT, path);

    ESP_LOGI("SD_CARD", "Nulis ke file: %s", full_path);
    FILE *f = fopen(full_path, "a"); // "a" = append (nambahin teks di bawahnya)
    if (f == NULL) {
        ESP_LOGE("SD_CARD", "Gagal buka file buat nulis!");
        return;
    }
    fprintf(f, "%s\n", data);
    fclose(f);
    ESP_LOGI("SD_CARD", "Berhasil simpan data.");
}

// ========================================================
// 3. FUNGSI BACA FILE
// ========================================================
void baca_file(const char *path) {
    char full_path[64];
    snprintf(full_path, sizeof(full_path), "%s/%s", MOUNT_POINT, path);

    ESP_LOGI("SD_CARD", "Membaca file: %s", full_path);
    FILE *f = fopen(full_path, "r");
    if (f == NULL) {
        ESP_LOGE("SD_CARD", "File kaga ada, Cok!");
        return;
    }
    char line[128];
    while (fgets(line, sizeof(line), f) != NULL) {
        printf("%s", line);
    }
    fclose(f);
}

int baca_highscore_dino() {
    FILE *f = fopen("/sdcard/dinohi.txt", "r");
    if (f == NULL) return 0; // Kalo belum ada, anggap 0
    int hs = 0;
    fscanf(f, "%d", &hs);
    fclose(f);
    return hs;
}

void simpan_highscore_dino(int hs) {
    FILE *f = fopen("/sdcard/dinohi.txt", "w");
    if (f != NULL) {
        fprintf(f, "%d", hs);
        fclose(f);
    }
}

void simpan_highscore_snake(int hs) {
    FILE *f = fopen("/sdcard/shakehi.txt", "w");
    if (f != NULL) {
        fprintf(f, "%d", hs);
        fclose(f);
    }
}

int baca_highscore_snake() {
    FILE *f = fopen("/sdcard/snakehi.txt", "r");
    if (f == NULL) return 0; // Kalo belum ada, anggap 0
    int hs = 0;
    fscanf(f, "%d", &hs);
    fclose(f);
    return hs;
}

void simpan_highscore_tetris(int hs) {
    FILE *f = fopen("/sdcard/tetrishi.txt", "w");
    if (f != NULL) {
        fprintf(f, "%d", hs);
        fclose(f);
    }
}

int baca_highscore_tetris() {
    FILE *f = fopen("/sdcard/tetrishi.txt", "r");
    if (f == NULL) return 0; // Kalo belum ada, anggap 0
    int hs = 0;
    fscanf(f, "%d", &hs);
    fclose(f);
    return hs;
}