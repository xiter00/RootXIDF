
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"
#include "globals.h"
#include "photo_data.h"
#include "font7x13.h"
#include "photo_data.h"




void screen_draw_bitmap(uint8_t id, int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color) {
    int16_t byteWidth = (w + 7) / 8; 
    uint8_t byte = 0;
    for (int16_t j = 0; j < h; j++, y++) {
        for (int16_t i = 0; i < w; i++) {
            if (i & 7) byte <<= 1;
            else       byte   = bitmap[j * byteWidth + i / 8];
            
            // Nah, ini yang bikin gambar OLED 1-bit bisa muncul di layar warna!
            if (byte & 0x80) lcdDrawPixel(&dev, x + i, y, color); 
        }
    }
}


void screen_draw_bitmap_vertical(uint8_t id, int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color) {
    int16_t pages = (h + 7) / 8; 
    
    for (int16_t p = 0; p < pages; p++) {
        for (int16_t i = 0; i < w; i++) {

            uint8_t byte = bitmap[p * w + i];
            
          
            for (int8_t b = 0; b < 8; b++) {
                int16_t pixel_y = p * 8 + b;
                
                
                if (pixel_y < h) {
                  
                    if (byte & (1 << b)) {
                        lcdDrawPixel(&dev, x + i, y + pixel_y, color);
                    }
                }
            }
        }
    }
}




// ========================================================
// 1. Fungsi Tampilan Logo Saja
// ========================================================
void tampilkanLogoDulu() {
    lcdFillScreen(&dev, BLACK); 
    
    // Nampilin gambar foto 1-bit
    screen_draw_bitmap(0, 0, 0, my_photo_bmp, 128, 64, WHITE);
    
    lcdDrawFinish(&dev); 
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// ========================================================
// 2. Fungsi Intro Anime & Info Firmware
// ========================================================
void tampilkanIntroAnime() {
    // Animasi Glitch Garis
    for (int i = 0; i < 8; i++) {
        lcdFillScreen(&dev, BLACK);
        
        for (int j = 0; j < 10; j++) {
            int y = esp_random() % 64; 
            lcdDrawLine(&dev, 0, y, 128, y, WHITE); // Ganti ke garis ST7789
        }
        lcdDrawFinish(&dev);
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    lcdFillScreen(&dev, BLACK);
    
    // Foto Anime di Kiri (1-bit aman!)
    screen_draw_bitmap(0, 0, 0, foto_anime_64, 64, 64, WHITE);
    
    // Garis Pemisah Vertikal 
    lcdDrawLine(&dev, 65, 0, 65, 64, WHITE); // Ganti ke garis vertikal ST7789

    // Teks Firmware di Kanan (Manggil fungsi extern dari globals.h)
    rootx_print_text_c(68, 6, "[FIRMWARE]", WHITE, BLACK);
    rootx_print_text_c(67, 18, "Name:RootX", WHITE, BLACK);
    rootx_print_text_c(67, 28, "Ver :1.0.0", WHITE, BLACK);
    rootx_print_text_c(67, 38, "By  :Andyy", WHITE, BLACK);
    rootx_print_text_c(67, 48, "Mode:GOD", WHITE, BLACK);
    rootx_print_text_c(67, 56, "Stat:Opt", WHITE, BLACK);

    lcdDrawFinish(&dev);
    vTaskDelay(pdMS_TO_TICKS(2500));
}

// ========================================================
// Fungsi Bantu Efek Ngetik 
// ========================================================
void ketikTeks(const char* teks, int x, int y) {
    char hurufTemp[2] = {0, 0}; 
    int currentX = x;
    int panjang = strlen(teks);

    for (int i = 0; i < panjang; i++) {
        hurufTemp[0] = teks[i]; 
        
        rootx_print_text_c(currentX, y, hurufTemp, WHITE, BLACK);
        lcdDrawFinish(&dev);
        
        currentX += 8; // Ganti 6 jadi 8, karena lebar font ST7789 lu itu 8 pixel
        vTaskDelay(pdMS_TO_TICKS(25)); 
    }
}

// ========================================================
// 3. Fungsi Teks Splash (Hacker Booting)
// ========================================================
void tampilkanTeksSplash() {
    lcdFillScreen(&dev, BLACK);

    // Ketinggian Y ditambahin kelipatan 16 biar tulisan gak numpuk
    ketikTeks(">> Initializing...", 0, 10);
    vTaskDelay(pdMS_TO_TICKS(150));
    
    ketikTeks(">> Checking Flash...", 0, 26);
    vTaskDelay(pdMS_TO_TICKS(150));
    
    ketikTeks(">> Checking PSRAM...", 0, 42);
    vTaskDelay(pdMS_TO_TICKS(150));
    
    ketikTeks(">> ROOTX READY!!", 0, 58);
    
    vTaskDelay(pdMS_TO_TICKS(800));
}
