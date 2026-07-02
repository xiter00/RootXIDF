#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "globals.h"
#include "photo_data.h"
#include <math.h>
#include "esp_log.h"
#include <dirent.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "driver/ledc.h"
#include <math.h>
#include <sys/statvfs.h> 
#include "esp_random.h"
#include "esp_spiffs.h"
#include "iconSmall.h"    
#include "iconmenu.h"    
#include "font7x13.h"     
#include "font7x13B.h"     
#include "background.h"



// --- WRAPPER SAKTI BUAT FONT ST7789 ---



// Jembatan buat nulis teks pakai font ST7789
// --- WRAPPER SAKTI BUAT FONT ST7789 ---
// WAJIB array [2] sesuai dokumentasi nopnop2002
// --- WRAPPER SAKTI BUAT FONT ST7789 ---
// WAJIB array [2] sesuai dokumentasi nopnop2002
FontxFile fx16G[2]; // Ukuran 8x16 (Kecil)
FontxFile fx24G[2]; // Ukuran 12x24 (Sedang)
FontxFile fx32G[2]; // Ukuran 16x32 (Gede)
uint32_t input_millis() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void rootx_print_text_c(int x, int y, const char* str, uint16_t fg, uint16_t bg) {
    lcdDrawCustomString(&dev, fontC7x13, x, y, (char*)str, fg, bg); 
}
void rootx_print_text_cb(int x, int y, const char* str, uint16_t fg, uint16_t bg) {
    lcdDrawCustomString(&dev, fontC7x13B, x, y, (char*)str, fg, bg); 
}

void rootx_print_text_kecil(int x, int y, const char* str, uint16_t fg, uint16_t bg) {
    if (bg != fg) lcdSetFontFill(&dev, bg);
else lcdUnsetFontFill(&dev);
    lcdDrawString(&dev, fx16G, x, y, (uint8_t*)str, fg); 
}

// 2. Fungsi Text Sedang (12x24)
void rootx_print_text_sedang(int x, int y, const char* str, uint16_t fg, uint16_t bg) {
    if (bg != fg) lcdSetFontFill(&dev, bg);
else lcdUnsetFontFill(&dev);
    lcdDrawString(&dev, fx24G, x, y, (uint8_t*)str, fg);
}

// 3. Fungsi Text Gede (16x32)
void rootx_print_text_gede(int x, int y, const char* str, uint16_t fg, uint16_t bg) {
    if (bg != fg) lcdSetFontFill(&dev, bg);
else lcdUnsetFontFill(&dev);
    lcdDrawString(&dev, fx32G, x, y, (uint8_t*)str, fg);
}


void renderFileExplorer(void);

extern void handleJoystick(void);
extern void tampilkanLogoDulu(void);
extern void tampilkanIntroAnime(void);
extern void tampilkanTeksSplash(void);
void tampilkanMenuLogo(void);
void tampilkanMenuUtama(void);
extern int baca_highscore_tetris();
extern void simpan_highscore_tetris(int hs);
void renderAboutScreen(void);
void renderRebootScreen(void);
void tampilkanMenuLogo(void);
void tampilkanMenuUtama(void);
void tampilkanWifiScanner(void);
void tampilkanDeauthScreen(void);
void tampilkanBrightness(void);
void tampilkanSpamScreen(const char* judul, const char* subTeks);
void renderSdManager(void);
void tampilkanStationScanner(void);
void tampilkanTrackScreen(void);
void tampilkandeauthsta(void);
void tampilkanEvilTwinScreen(void);

void tampilkanMenuIR(void);
void tampilkanMenuSavedIR(void);

void renderTvBGone(void);


bool introDone = false;


void init_joystick() {
    int pins[] = {PIN_UP, PIN_DOWN, PIN_LEFT, PIN_RIGHT, PIN_OK};
    for(int i = 0; i < 5; i++) {
        gpio_set_direction(pins[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(pins[i], GPIO_PULLUP_ONLY);
    }
}


#define BG_COLOR      rgb565(5, 5, 5)       // #050505
#define GRID_COLOR    rgb565(13, 2, 5)       // grid sangat redup
#define PINK_COLOR    rgb565(255, 30, 90)    // #ff1e5a
#define CYAN_COLOR    rgb565(0, 255, 255)    // #00ffff
#define WHITE_COLOR   rgb565(255, 255, 255)
#define GRAY_COLOR    rgb565(128, 128, 128)
#define BLACK_COLOR   rgb565(0, 0, 0)

#define MENU_ACTIVE_BG_START  rgb565(205, 25, 73)   // hasil blending pink 80% + bg
#define MENU_ACTIVE_BG_END    rgb565(30, 8, 14)     // hasil blending pink 10% + bg
#define OUTER_RING_COLOR      rgb565(77, 9, 27)     // pink 30%






void drawBackground(void) {
    // Copy langsung ke frame buffer
    memcpy(dev._frame_buffer, background, sizeof(uint16_t) * 32400);
    
}
void task_display(void *pvParameters) {
init_joystick();
// Ganti angka pin ini sesuai wiring SPI ST7789 lu (MOSI, SCLK, CS, D
    spi_master_init(&dev, LCD_SDA, LCD_SCL, LCD_CS, LCD_DC, LCD_RES, LCD_BLK);
        // 1. INISIALISASI SESUAI UKURAN ASLI PABRIK (Biar gak semut!)
    lcdInit(&dev, 135, 240, 52, 40); 
    
    // 2. HACK ROTASI HARDWARE (Nembak Register MADCTL)
    spi_master_write_command(&dev, 0x36);    
    spi_master_write_data_byte(&dev, 0x70); // 0x70 = Putar 90 Derajat (Landscape)

    // 3. TUKER LOGIKA DIMENSI DI OTAK ESP32
    // Biar fungsi nggambar kotak tahu kalau layarnya sekarang lagi tiduran
    dev._width = 240;
    dev._height = 135;
    dev._offsetx = 40; 
    dev._offsety = 53;

    // Bersihin layar pakai warna hitam
    lcdFillScreen(&dev, BLACK);

    lcdEnableFrameBuffer(&dev);                
    
    // Atur konfigurasi Timer PWM
ledc_timer_config_t ledc_timer = {
    .speed_mode       = LEDC_LOW_SPEED_MODE,
    .timer_num        = LEDC_TIMER_0,
    .duty_resolution  = LEDC_TIMER_8_BIT, // 8-bit artinya nilai 0-255 (pas buat lu)
    .freq_hz          = 5000,             // Frekuensi 5kHz biar layar ga kedip-kedip di kamera
    .clk_cfg          = LEDC_AUTO_CLK
};
ledc_timer_config(&ledc_timer);

// Atur konfigurasi Channel ke pin Backlight (misal GPIO 13)
ledc_channel_config_t ledc_channel = {
    .speed_mode     = LEDC_LOW_SPEED_MODE,
    .channel        = LEDC_CHANNEL_0,
    .timer_sel      = LEDC_TIMER_0,
    .intr_type      = LEDC_INTR_DISABLE,
    .gpio_num       = 13,                 // <--- SESUAIIN SAMA PIN BL LU
    .duty           = 150,                // Kecerahan awal pas baru nyala
    .hpoint         = 0
};
ledc_channel_config(&ledc_channel);


    // Load ke-3 ukuran font bawaan library dari folder font di memori internal
    InitFontx(fx16G, "/spiffs/ILGH16XB.FNT", ""); // 8x16
    InitFontx(fx24G, "/spiffs/ILGH24XB.FNT", ""); // 12x24
    InitFontx(fx32G, "/spiffs/ILGH32XB.FNT", ""); // 16x32

    lcdFillScreen(&dev, BLACK);

    lcdDrawFinish(&dev);
    ESP_LOGI("RootX", "ST7789 Frame Buffer & FontX Ready!");


    tampilkanLogoDulu();
    tampilkanIntroAnime();
    tampilkanTeksSplash();
    introDone = true;
    for(;;) {
      
handleJoystick(); 

        if (appMode == 0) {
            if (inSubMenu == false) tampilkanMenuLogo();
            else tampilkanMenuUtama();
        } 
        else if (appMode == 1) {
            tampilkanWifiScanner(); 
        } 
        else if (appMode == 2) {
            tampilkanDeauthScreen(); 
        } 
        else if (appMode == 3) { 
            tampilkanBrightness();
        } 
        else if (appMode == 4) {
            if (aktifModeSpam == 1) tampilkanSpamScreen("BEACON SPAM", "Start Spam?");
            else if (aktifModeSpam == 2) tampilkanSpamScreen("RICKROLL", "Start Spam?");
        } else if (appMode == 5) { 
            tampilkanStationScanner();
        } else if (appMode == 6) { // Kita kasih mode 6 buat Track
    tampilkanTrackScreen();
} else if (appMode == 7) {
tampilkandeauthsta();
} else if (appMode == 8) {
tampilkanEvilTwinScreen();
} else if (appMode == MODE_IR_SNIFFER) {    // <--- TAMBAHIN INI
            tampilkanMenuIR();
        } else if (appMode == MODE_SAVED_REMOTE) {  // <--- TAMBAHIN INI
            tampilkanMenuSavedIR();
        } else if (appMode == 14) {
        renderAboutScreen(); 
        } else if (appMode == 15) {
        renderRebootScreen();
        } else if (appMode == 16) {       // <--- TAMBAHIN INI (SD MANAGER)
            renderSdManager();            // <--- TAMBAHIN INI
        } else if (appMode == 17) {       // <--- TAMBAHIN INI
            renderFileExplorer();         // <--- TAMBAHIN INI
        } else if (appMode == 18) {       // <--- TAMBAHIN INI
            renderTvBGone();              // <--- TAMBAHIN INI
        }


        // Kasih jeda dikit biar gak rakus CPU (kira-kira 30 FPS)
        vTaskDelay(pdMS_TO_TICKS(33)); 
    }
}


// Inisialisasi bintang pertama kali
extern void screen_draw_bitmap(uint8_t id, int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color);
extern void screen_draw_bitmap_vertikal(uint8_t id, int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color);

uint32_t millis() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static float visualY = 24.0; // Variabel buat simpen posisi kursor sementara

void drawSmartSelection(int targetY) {
    // 0.3 itu kecepatan luncur, makin kecil makin lambat/smooth
    visualY += (targetY - visualY) * 0.3; 
    lcdDrawFillRect(&dev, 0, (int)visualY, 128, (int)visualY + 18, WHITE);
}

// Fungsi animasi wave


// Fungsi bounce buat icon
int getBounce(int speed, int range) {
    return (int)(sin(millis() / (float)speed) * range);
}

// Fungsi loading bar yang bener
void drawLoadingBar(int x, int y, int w, int h, int progress) {
    lcdDrawRect(&dev, x, y, x + w, y + h, WHITE);
    int fillW = (w * progress) / 100;
    if (fillW > w) fillW = w;
    lcdDrawFillRect(&dev, x, y, x + fillW, y + h, WHITE);
    
    int offset = (millis() / 50) % 20;
    for(int i = -20; i < fillW; i += 15) {
        int lineX = x + i + offset;
        if(lineX > x && lineX < x + fillW) {
            // Kalau ssd1306_draw_line gak ada, pake vline aja buat efek
            for(int j=0; j<h; j++) lcdDrawPixel(&dev, lineX, y+j, BLACK);
        }
    }
}
// Fungsi gambar bintang gerak (Starfield)


// Deklarasi bitmap solver yang ada di boot_system.c biar file ini bisa make juga

// ==========================================
// FUNGSI PEMBANTU PENGGANTI ARDUINO
// ==========================================


long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// ==========================================
// DATA MENU 
// ==========================================
const unsigned char* iconListWiFi[] = {
ics_scan,
ics_sniff,
ics_spam,
ics_wifi
};

const unsigned char* iconListBLE[]  = {
ics_scan,
ics_apple,
ics_android
};

const unsigned char* iconListIR[]   = {
ics_ir,
ics_tv,
ics_ac,
ics_lock,
ics_saved 
};

const unsigned char* iconListSet[]  = {
ics_bright,
ics_file,
ics_info,
ics_repeat 
};

const unsigned char* iconListGame[]  = {
ics_game,
ics_game,
ics_game
};





const char* subMenuWiFi[] = { 
"Scan WiFi", 
"List Scan", 
"Beacon Spam", 
"RickRoll SSID"
 };
 
const char* subMenuBLE[]  = {
"BLE Scanner",
"Spam Apple",
"Spam Android"
 };
 
const char* subMenuIR[]   = {
"Read Signal",
"TV B-Gone",
"AC Remote",
"Brute Force",
"Saved Remotes"
 };

const char* subMenuSet[]  = {
"Brightness",
"SD Manager",
"About RootX",
"Reboot" 
};

const char* subMenuGame[] = {
"Dinosaur Game",
"Snake Game",
"Tetris Game"
 };

// ==========================================
// LOGIKA TAMPILAN
// ==========================================


MenuItem menuList[5] = {
    {wifi48, wifi32, "WI-FI"},
    {ble48, ble32, "BLE"},
    {infrared48, infrared32, "IR"},
    {setting48, setting32, "SETS"},
    {game48, game32, "GAME"}
};

int carouselCurrentIdx = 0;
int carouselAnimFrame = 0;
bool carouselAnimating = false;
uint32_t carouselAnimStart = 0;


// Fungsi Scaling Khusus 1-Bit Vertikal (Image2cpp)
void drawIconScaled(int x, int y, int src_w, int src_h, int dst_w, int dst_h, const uint8_t *icon, uint16_t color) {
    if (icon == NULL) return;
    if (dst_w <= 0 || dst_h <= 0) return;
    
    for (int row = 0; row < dst_h; row++) {
        int screen_y = y + row;
        if (screen_y < 0) continue;        // Skip kalau di atas layar
        if (screen_y >= dev._height) break; // Stop kalau udah lewat bawah layar
        
        for (int col = 0; col < dst_w; col++) {
            int screen_x = x + col;
            if (screen_x < 0) continue;        // Skip kiri
            if (screen_x >= dev._width) break;  // Stop kanan
            
            // Cari koordinat titik sumber sebelum di-scale
            int src_col = col * src_w / dst_w;
            int src_row = row * src_h / dst_h;
            
            // --- LOGIKA BACA 1-BIT VERTIKAL ---
            int byteIdx = (src_row / 8) * src_w + src_col;
            int bitIdx = src_row % 8;
            
            // Kalau bit-nya bernilai 1, tembak warnanya ke Frame Buffer
            if ((icon[byteIdx] >> bitIdx) & 0x01) {
                dev._frame_buffer[screen_y * dev._width + screen_x] = color;
            }
            // Kalau 0, otomatis di-skip (Transparan Sempurna!)
        }
    }
}

int carouselDirection = 0;
void updateCarouselAnimation() {
    if (!carouselAnimating) return;
    uint32_t elapsed = millis() - carouselAnimStart;
    if (elapsed >= 250) {
        carouselAnimating = false;
        carouselDirection = 0;
    }
}

// direction: 1 = klik down (geser ke atas), -1 = klik up (geser ke bawah)


// FUNGSI POST-PROCESSING GLITCH (Bikin Layar Sobek & Distorsi Neon)
void apply_cyber_glitch() {
    // 1. TIMING ACAK: Cuma aktif 10% dari total frame biar natural kagetnya
    if (esp_random() % 100 > 10) return; 

    // 2. TEARING EFFECT (Layar Sobek & Geser Horizontal)
    int jumlah_sobekan = (esp_random() % 3) + 1; // 1 sampe 3 sobekan
    
    for (int s = 0; s < jumlah_sobekan; s++) {
        int y_start = 20 + (esp_random() % 90);  // Di area menu (hindari header atas/baterai)
        int height = 2 + (esp_random() % 6);     // Tebal sobekan (2-7 pixel)
        int shift_x = (esp_random() % 20) - 10;  // Geser kiri/kanan (-10 sampe 10 pixel)
        
        if (shift_x == 0) shift_x = 5;

        for (int y = y_start; y < y_start + height; y++) {
            if (y >= dev._height) break;
            
            uint16_t temp_row[240];
            // Kopi baris asli dengan pergeseran (Dislokasi)
            for (int x = 0; x < dev._width; x++) {
                int src_x = x - shift_x;
                if (src_x >= 0 && src_x < dev._width) {
                    temp_row[x] = dev._frame_buffer[y * dev._width + src_x];
                } else {
                    temp_row[x] = BLACK; 
                }
            }
            // Timpa balik ke Frame Buffer
            for (int x = 0; x < dev._width; x++) {
                dev._frame_buffer[y * dev._width + x] = temp_row[x];
            }
        }
    }

    // 3. CHROMATIC ABERRATION (Garis Neon Rusak)
    int jumlah_neon = (esp_random() % 3) + 1;
    for (int n = 0; n < jumlah_neon; n++) {
        int y_neon = 20 + (esp_random() % 90);
        int length = 10 + (esp_random() % 60);
        int x_start = esp_random() % 150;
        
        // Pake warna dari kamus lu biar masuk tema
        uint16_t warna_glitch = (esp_random() % 2 == 0) ? CYAN : RED; 
        
        for (int x = x_start; x < x_start + length; x++) {
            if (x < dev._width) {
                dev._frame_buffer[y_neon * dev._width + x] = warna_glitch;
            }
        }
    }
}


void drawCarouselAnimated(float progress) {
    int y_atas   = 5;
    int y_tengah = 43;
    int y_bawah  = 96;
    int y_masuk_bawah = 135;
    int y_keluar_atas = -32;
    int y_masuk_atas  = -32;
    int y_keluar_bawah = 135;

    #define LERP(a, b, t) ((int)((a) + ((b) - (a)) * (t)))

    if (carouselDirection == 1) {
        int prev = (carouselCurrentIdx - 1 + 5) % 5;
        int curr = carouselCurrentIdx;
        int next = (carouselCurrentIdx + 1) % 5;
        int gone = (carouselCurrentIdx - 2 + 5) % 5;

        int goneY = LERP(y_atas, y_keluar_atas, progress);
        drawIconScaled(10, goneY, 32, 32, 32, 32, menuList[gone].icon_small, GRAY);

        int prevY = LERP(y_tengah, y_atas, progress);
        int prevS = LERP(48, 32, progress);
        int prevX = LERP(20, 10, progress);
        drawIconScaled(prevX, prevY, 48, 48, prevS, prevS, menuList[prev].icon_large, WHITE);

        int currY = LERP(y_bawah, y_tengah, progress);
        int currS = LERP(32, 48, progress);
        int currX = LERP(10, 20, progress);
        drawIconScaled(currX, currY, 32, 32, currS, currS, menuList[curr].icon_small, GRAY);

        int nextY = LERP(y_masuk_bawah, y_bawah, progress);
        drawIconScaled(10, nextY, 32, 32, 32, 32, menuList[next].icon_small, GRAY);

    } else if (carouselDirection == -1) {
        int next = (carouselCurrentIdx + 1) % 5;
        int curr = carouselCurrentIdx;
        int prev = (carouselCurrentIdx - 1 + 5) % 5;
        int gone = (carouselCurrentIdx + 2) % 5;

        int goneY = LERP(y_bawah, y_keluar_bawah, progress);
        drawIconScaled(10, goneY, 32, 32, 32, 32, menuList[gone].icon_small, GRAY);

        int nextY = LERP(y_tengah, y_bawah, progress);
        int nextS = LERP(48, 32, progress);
        int nextX = LERP(20, 10, progress);
        drawIconScaled(nextX, nextY, 48, 48, nextS, nextS, menuList[next].icon_large, WHITE);

        int currY = LERP(y_atas, y_tengah, progress);
        int currS = LERP(32, 48, progress);
        int currX = LERP(10, 20, progress);
        drawIconScaled(currX, currY, 32, 32, currS, currS, menuList[curr].icon_small, GRAY);

        int prevY = LERP(y_masuk_atas, y_atas, progress);
        drawIconScaled(10, prevY, 32, 32, 32, 32, menuList[prev].icon_small, GRAY);

    } else {
        int above = (carouselCurrentIdx - 1 + 5) % 5;
        int below = (carouselCurrentIdx + 1) % 5;
        drawIconScaled(10, y_atas, 32, 32, 32, 32, menuList[above].icon_small, GRAY);
        drawIconScaled(20, y_tengah, 48, 48, 48, 48, menuList[carouselCurrentIdx].icon_large, WHITE);
        drawIconScaled(10, y_bawah, 32, 32, 32, 32, menuList[below].icon_small, GRAY);
    }
}


void tampilkanMenuLogo() {
    drawBackground();
    
    updateCarouselAnimation();

float progress = 1.0f;
if (carouselAnimating) {
    progress = (millis() - carouselAnimStart) / 250.0f;
    if (progress > 1.0f) progress = 1.0f;
}

drawCarouselAnimated(progress);
    
    read_battery_percentage();
  
    lcdDrawRect(&dev, 216, 4, 234, 12, WHITE);
    lcdDrawFillRect(&dev, 235, 7, 237, 9, WHITE);
    
    uint16_t warna_bar = WHITE;
    int jumlah_bar = 0;

    if (batteryPercent > 75) {
        warna_bar = GREEN;  
        jumlah_bar = 4;
    } else if (batteryPercent > 50) {
        warna_bar = YELLOW; 
        jumlah_bar = 3;
    } else if (batteryPercent > 25) {
        warna_bar = ORANGE; 
        jumlah_bar = 2;
    } else {
        warna_bar = RED;    
        jumlah_bar = 1;
    }


    //BAR

    for (int b = 0; b < jumlah_bar; b++) {
        int bar_x_start = 218 + (b * 4); 
        lcdDrawFillRect(&dev, bar_x_start, 6, bar_x_start + 2, 10, warna_bar);
    }
    
    
    
    rootx_print_text_c(95, 2, "<RootX>", RED, RED);
    rootx_print_text_c(85, 122, "Dev: Andyy", WHITE, WHITE);
    
    
    
    
    rootx_print_text_kecil(75, 75, "<", ICE_CYAN, ICE_CYAN);
    rootx_print_text_kecil(86, 75, menuList[carouselCurrentIdx].label, WHITE, WHITE);
    
    apply_cyber_glitch();
    lcdDrawFinish(&dev);
}

static void rm_cyan_border(int y_start, int y_end) {
    uint16_t *fb = dev._frame_buffer;
    int sw = dev._width, sh = dev._height;
    // Intensitas per pixel (gaussian approximation, 8px glow)
    static const uint8_t G[8] = {255, 255, 160, 92, 48, 22, 9, 3};
    for (int y = y_start; y < y_end && y < sh; y++)
        for (int p = 0; p < 8 && p < sw; p++)
            fb[y * sw + p] = rgb565(0, G[p], G[p]);
}



static void rm_draw_item(int yPos, bool isActive,
                         const unsigned char *icon, const char *label) {
    
    


    if (isActive) {
        

        // ── B. Cyan border kiri + glow (8px gradient, identik mockup) ──
        rm_cyan_border(yPos, yPos + RM_ITEM_BAR);

        // ── C. Data stream glitch (2 blok hitam ngalir kanan→kiri) ──

        // ── D. Icon glow + icon bounce (text-shadow: 0 0 8px #00ffff) ──
        if (icon) {
            int bounce = getBounce(350, 2);
            int ix = 9, iy = yPos - 1 + bounce;
            
            screen_draw_bitmap_vertikal(0, ix, iy, icon, 18, 18,             // Icon di atas
                               rgb565(0, 255, 255));
        }

        // ── E. Label putih ──
        rootx_print_text_cb(35, yPos + 5, label, WHITE, WHITE);

    } else {
        // Non-aktif: icon abu + teks abu (no glow)
        if (icon) screen_draw_bitmap_vertikal(0, 9, yPos - 1, icon, 18, 18, GRAY);
        rootx_print_text_c(35, yPos+3, label, GRAY, GRAY);
    }

    // Separator bawah item

}

void tampilkanMenuUtama(void) {

 drawBackground();

    // ── 6. Header: "> WIFI // NETWORK" ─────────────────────────
    
        
  
        const char *catLabel = "";
        int totalSub = 0;
        if      (currentMenu == 0) { catLabel="WI-FI";  totalSub=4; }
        else if (currentMenu == 1) { catLabel="BLE"; totalSub=3; }
        else if (currentMenu == 2) { catLabel="IR";  totalSub=5; }
        else if (currentMenu == 3) { catLabel="SETTINGS"; totalSub=4; }
        else                       { catLabel="GAME";  totalSub=3; }

        // ">" cyan + nama kategori pink
        rootx_print_text_c(4,  4, ">",      CYAN, CYAN);
        rootx_print_text_c(14, 4, catLabel, PINK, PINK);

        // Sub-label abu
        

        // Underline: pink fade kanan — sama kayak mockup


        // ── 7. List item menu ─────────────────────────────────
        for (int i = 0; i < RM_MAX_VIS; i++) {
            int idx = topMenu + i;
            if (idx >= totalSub) break;
            bool isAct = (idx == currentSub);
            int  yPos  = RM_ITEM_Y0 + i * RM_ITEM_H;

            const unsigned char *ico = NULL;
            if      (currentMenu == 0) ico = iconListWiFi[idx];
            else if (currentMenu == 1) ico = iconListBLE[idx];
            else if (currentMenu == 2) ico = iconListIR[idx];
            else if (currentMenu == 3) ico = iconListSet[idx];
            else                       ico = iconListGame[idx];

            const char *lbl = "";
            if      (currentMenu == 0) lbl = subMenuWiFi[idx];
            else if (currentMenu == 1) lbl = subMenuBLE[idx];
            else if (currentMenu == 2) lbl = subMenuIR[idx];
            else if (currentMenu == 3) lbl = subMenuSet[idx];
            else                       lbl = subMenuGame[idx];

            rm_draw_item(yPos, isAct, ico, lbl);
        }

        // ── 8. Scroll dots (kalau item > 5) ──────────────────
        if (totalSub > RM_MAX_VIS) {
            int avail_h = dev._height - RM_ITEM_Y0 - 8;
            for (int d = 0; d < totalSub; d++) {
                int dotY = RM_ITEM_Y0 + d * avail_h / totalSub;
                int dotX = RM_PANEL_W - 7;
                if (d == currentSub) {
                    // Dot aktif: cyan 4x6 + mini glow
                    lcdDrawFillRect(&dev, dotX-1, dotY-1, dotX+4, dotY+6,
                                    rgb565(0, 40, 40));            // Glow dim
                    lcdDrawFillRect(&dev, dotX, dotY, dotX+3, dotY+5,
                                    rgb565(0, 255, 255));          // Inti
                } else {
                    lcdDrawFillRect(&dev, dotX, dotY+1, dotX+2, dotY+4,
                                    rgb565(128, 10, 40));          // Dot kecil pink
                }
            }
        }
    

    // ── 9. Battery (identik sama tampilkanMenuLogo) ────────────
    read_battery_percentage();
    lcdDrawRect(&dev, 216, 4, 234, 12, WHITE);
    lcdDrawFillRect(&dev, 235, 7, 237, 9, WHITE);
    {
        uint16_t warna_bar;
        int jumlah_bar;
        if      (batteryPercent > 75) { warna_bar = GREEN;  jumlah_bar = 4; }
        else if (batteryPercent > 50) { warna_bar = YELLOW; jumlah_bar = 3; }
        else if (batteryPercent > 25) { warna_bar = ORANGE; jumlah_bar = 2; }
        else                          { warna_bar = RED;    jumlah_bar = 1; }
        for (int b = 0; b < jumlah_bar; b++) {
            int bx = 218 + b * 4;
            lcdDrawFillRect(&dev, bx, 6, bx+2, 10, warna_bar);
        }
    }

    // ── 10. Scanlines overlay (identik CSS mockup) ─────────────
    

    // ── 11. Cyber glitch + flush ────────────────────────────────
    apply_cyber_glitch();
    lcdDrawFinish(&dev);
}



// --- TARUH INI DI ATAS FUNGSI ---


void tampilkanTrackScreen() {
    lcdFillScreen(&dev, BLACK);
    

    char buf[32];
    
    // --- ANIMASI FLOATING ICON (Icon WiFi naik turun pelan) ---
    int floatY = 15 + (int)(sin(millis() / 300.0) * 3);
    screen_draw_bitmap_vertikal(0, 105, floatY, ics_wifi, 10, 10, WHITE);

    // Header
    lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
    rootx_print_text_c(30, 1, "TRACKING RSSI", BLACK, WHITE);
    
    // --- ANIMASI RADAR PULSING ---
    static int r = 0;
    r++; if(r > 20) r = 0;
    lcdDrawCircle(&dev, 108, 18, r, WHITE);
    if(r > 5) lcdDrawCircle(&dev, 105, 15, r - 5, WHITE);

    // Data RSSI
    snprintf(buf, sizeof(buf), "%d", targetTerkunci.rssi);
    rootx_print_text_c(50, 30, buf, WHITE, BLACK);

    // Footer
    lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
    rootx_print_text_c(5, 55, "< BACK", BLACK, WHITE);
    
    lcdDrawFinish(&dev);
}


void tampilkanWifiScanner() {
    lcdFillScreen(&dev, BLACK);
    
    char buf[64]; 

    if (scannerState == 0) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_c(2, 1, "WIFI SCANNER", BLACK, WHITE);

        rootx_print_text_c(40, 25, "Yakin??", WHITE, BLACK);

        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_c(2, 55, "< CANCEL", BLACK, WHITE);
        rootx_print_text_c(95, 55, "YES >", BLACK, WHITE);
    }
    else if (scannerState == 1) {
        rootx_print_text_c(20, 25, "Scanning Air...", WHITE, BLACK);
        if (scanDone) scannerState = 2; 
    }
    else if (scannerState == 2) {
        if (totalWiFi == 0) {
            lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
            rootx_print_text_c(2, 1, "SAVED NETWORKS", BLACK, WHITE);
            rootx_print_text_c(15, 25, "BELUM ADA DATA!", WHITE, BLACK);
            rootx_print_text_c(10, 35, "Scan WiFi dulu", WHITE, BLACK);
            lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
            rootx_print_text_c(2, 55, "< BACK", BLACK, WHITE);
        } else {
          

            lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
            snprintf(buf, sizeof(buf), "SCANNER - %d", totalWiFi);
            rootx_print_text_c(2, 1, buf, BLACK, WHITE);
            
            for (int i = 0; i < 3; i++) {
                int itemIdx = scrollPosScanner + i;
                if (itemIdx < totalWiFi) {
                    int yPos = 14 + (i * 13);
                    int textColor = WHITE;
                    int bgColor = BLACK;
                    
                    if (i == cursorInScanner) {
                        lcdDrawFillRect(&dev, 0, yPos - 1, 128, yPos + 11, WHITE);
                        textColor = BLACK;
                        bgColor = WHITE;
                    }

                    snprintf(buf, sizeof(buf), "%d.", listWiFi[itemIdx].id);
                    rootx_print_text_c(1, yPos + 1, buf, textColor, bgColor);
                    
                    int maxChar = 8;
                    int len = strlen(listWiFi[itemIdx].ssid);
                    char textShow[16] = {0};

                    if (i == cursorInScanner && len > maxChar) {
                        int kelebihan = len - maxChar;
                        int offset = (millis() / 300) % (kelebihan + 4); 
                        if (offset > kelebihan) offset = kelebihan; 
                        strncpy(textShow, listWiFi[itemIdx].ssid + offset, maxChar);
                    } else {
                        if (len > maxChar) strncpy(textShow, listWiFi[itemIdx].ssid, maxChar);
                        else               strcpy(textShow, listWiFi[itemIdx].ssid);
                    }
                    rootx_print_text_c(16, yPos + 1, textShow, textColor, bgColor);

                    snprintf(buf, sizeof(buf), "C:%d", listWiFi[itemIdx].channel);
                    rootx_print_text_c(66, yPos + 1, buf, textColor, bgColor);
                    snprintf(buf, sizeof(buf), "%ddB", listWiFi[itemIdx].rssi);
                    rootx_print_text_c(95, yPos + 1, buf, textColor, bgColor);
                }
            }
            lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
            rootx_print_text_c(2, 55, "< BACK", BLACK, WHITE);
            rootx_print_text_c(53, 55, "[OK]", BLACK, WHITE);
        }
    }
    else if (scannerState == 3) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_c(22, 1, "DETAIL TARGET", BLACK, WHITE);

        int xSide = 5; 
        
        rootx_print_text_c(xSide, 13, "SSID: ", WHITE, BLACK);
        int lenSSID = strlen(targetTerkunci.ssid);
        char tmpSSID[20] = {0};
        if (lenSSID > 14) {
            int kelebihan = lenSSID - 10; 
            int offset = (millis() / 250) % (kelebihan + 4);
            if (offset > kelebihan) offset = kelebihan;
            strncpy(tmpSSID, targetTerkunci.ssid + offset, 14);
        } else {
            strcpy(tmpSSID, targetTerkunci.ssid);
        }
        rootx_print_text_c(xSide + 35, 13, tmpSSID, WHITE, BLACK);

        rootx_print_text_c(xSide, 23, "MAC : ", WHITE, BLACK);
        rootx_print_text_c(xSide + 35, 23, targetTerkunci.mac, WHITE, BLACK);
        
        snprintf(buf, sizeof(buf), "CH  : %d", targetTerkunci.channel);
        rootx_print_text_c(xSide, 33, buf, WHITE, BLACK);

        snprintf(buf, sizeof(buf), "SIG : %d dBm", targetTerkunci.rssi);
        rootx_print_text_c(xSide, 43, buf, WHITE, BLACK);

        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_c(2, 55, "[<] BACK", BLACK, WHITE);
    } 
                else if (scannerState == 4) { // Atau scannerStateSta == 4, sesuaikan aja
        // --- 1. HEADER ---
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_c(42, 1, "ACTIONS", BLACK, WHITE);

        // --- 2. BLOK PUTIH STATIS DI TENGAH ---
        // Y mulai dari 24, tinggi 16 piksel (Bener-bener pas di center OLED 128x64)
        lcdDrawFillRect(&dev, 0, 24, 128, 40, WHITE);

        // --- 3. LOGIKA ROLLING MENU ---
        for(int i = 0; i < 5; i++) {
            const char* teks;
            const unsigned char* icon;
            
            // Set Teks dan Icon
            if(i == 0)      { teks = "DEAUTH "; icon = ics_skull; }
            else if(i == 1) { teks = "EVIL TWIN"; icon = ics_conn; }
            else if(i == 2) { teks = "CLIENTS"; icon = ics_sniff; }
            else if(i == 3) { teks = "TRACK  "; icon = ics_wifi;  } 
            else            { teks = "DETAILS"; icon = ics_info;  }

            // Hitung jarak index ini dari kursor yang lagi aktif
            int diff = i - contextCursor; 
            
            // Jarak antar baris 15 piksel. Posisi tengah (diff=0) ada di Y=27
            int yPos = 27 + (diff * 15); 

            // Cuma gambar menu yang posisinya ada di area pandang (antara header & footer)
            if (yPos > 10 && yPos < 45) {
                
                if (diff == 0) { 
                    // --- MENU TERPILIH (DI TENGAH BLOK PUTIH) ---
                    // Warna dibalik (Hitam di atas Putih)
                    screen_draw_bitmap_vertikal(0, 26, yPos - 1, icon, 10, 10, BLACK); 
                    rootx_print_text_c(42, yPos, (char*)teks, BLACK, WHITE);
                    
                    // Tambahan efek panah biar kelihatan lebih "Gede/Lebar"
                    rootx_print_text_c(10, yPos, ">", BLACK, WHITE);
                    rootx_print_text_c(110, yPos, "<", BLACK, WHITE);
                } 
                else { 
                    // --- MENU GAK TERPILIH (DI ATAS / DI BAWAH) ---
                    // Warna normal (Putih di atas Hitam)
                    // Posisinya digeser X-nya (+4) biar seakan-akan mundur/mengecil
                    screen_draw_bitmap_vertikal(0, 30, yPos, icon, 10, 10, WHITE);
                    rootx_print_text_c(46, yPos + 1, (char*)teks, WHITE, BLACK);
                }
            }
        }

        // --- 4. FOOTER ---
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_c(2, 55, "< BACK", BLACK, WHITE);
        rootx_print_text_c(85, 55, "[OK] GO", BLACK, WHITE);
    }



    lcdDrawFinish(&dev);
}




void tampilkanStationScanner() {
    lcdFillScreen(&dev, BLACK);
    
    char buf[64]; 

    if (scannerStateSta == 0) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_c(2, 1, "STATION SCANNER", BLACK, WHITE);
        rootx_print_text_c(30, 25, "Scan Clients?", WHITE, BLACK);
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_c(2, 55, "< BACK", BLACK, WHITE);
        rootx_print_text_c(95, 55, "YES >", BLACK, WHITE);
    }
    else if (scannerStateSta == 1) {
        rootx_print_text_c(10, 20, "SNIFFING TARGET:", WHITE, BLACK);
        rootx_print_text_c(10, 35, targetTerkunci.ssid, WHITE, BLACK); 
        if (scanStaDone) scannerStateSta = 2; 
    }
    else if (scannerStateSta == 2) {
        if (totalStation == 0) {
            lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
            rootx_print_text_c(2, 1, "CLIENT LIST", BLACK, WHITE);
            rootx_print_text_c(15, 25, "NO CLIENTS FOUND!", WHITE, BLACK);
            lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
            rootx_print_text_c(2, 55, "< RESCAN", BLACK, WHITE);
        } else {
            lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
            snprintf(buf, sizeof(buf), "CLIENTS: %d", totalStation);
            rootx_print_text_c(2, 1, buf, BLACK, WHITE);
            
            for (int i = 0; i < 3; i++) {
                int itemIdx = scrollPosScanner + i;
                if (itemIdx < totalStation) {
                    int yPos = 14 + (i * 13);
                    int txtCol = (i == cursorInScanSta) ? BLACK : WHITE;
                    int bgCol = (i == cursorInScanSta) ? WHITE : BLACK;
                    
                    if (i == cursorInScanSta) lcdDrawFillRect(&dev, 0, yPos - 1, 128, yPos + 11, WHITE);

                    snprintf(buf, sizeof(buf), "%d.", listStation[itemIdx].id);
                    rootx_print_text_c(1, yPos + 1, buf, txtCol, bgCol);

                    snprintf(buf, sizeof(buf), "%02X:%02X..%02X:%02X", 
                             listStation[itemIdx].mac[0], listStation[itemIdx].mac[1],
                             listStation[itemIdx].mac[4], listStation[itemIdx].mac[5]);
                   
                  
                    rootx_print_text_c(18, yPos + 1, buf, txtCol, bgCol);

                    snprintf(buf, sizeof(buf), "%ddBm", listStation[itemIdx].rssi);
                    rootx_print_text_c(90, yPos + 1, buf, txtCol, bgCol);
                }
            }
            lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
            rootx_print_text_c(2, 55, "< BACK", BLACK, WHITE);
            rootx_print_text_c(53, 55, "[OK] ACTION", BLACK, WHITE);
        }
    }
    else if (scannerStateSta == 3) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_c(22, 1, "TARGET DETAILS", BLACK, WHITE);

        snprintf(buf, sizeof(buf), "MC:%02X:%02X:%02X:%02X:%02X:%02X", 
                 targetSta.mac[0], targetSta.mac[1], targetSta.mac[2],
                 targetSta.mac[3], targetSta.mac[4], targetSta.mac[5]);
                 
        
        rootx_print_text_c(5, 17, buf, WHITE, BLACK);
        

        snprintf(buf, sizeof(buf), "RSSI: %d dBm", targetSta.rssi);
        rootx_print_text_c(5, 25, buf, WHITE, BLACK);

        snprintf(buf, sizeof(buf), "PACKETS: %d", targetSta.paket_count);
        rootx_print_text_c(5, 35, buf, WHITE, BLACK);

        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_c(2, 55, "< BACK", BLACK, WHITE);
    } 
    
        else if (scannerStateSta == 4) { // Atau scannerStateSta == 4, sesuaikan aja
        // --- 1. HEADER ---
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_c(42, 1, "ACTIONS", BLACK, WHITE);

        // --- 2. BLOK PUTIH STATIS DI TENGAH ---
        // Y mulai dari 24, tinggi 16 piksel (Bener-bener pas di center OLED 128x64)
        lcdDrawFillRect(&dev, 0, 24, 128, 40, WHITE);

        // --- 3. LOGIKA ROLLING MENU ---
        for(int i = 0; i < 2; i++) {
            const char* teks;
            const unsigned char* icon;
            
            // Set Teks dan Icon
              if(i == 0)      { teks = "KICK CLIENT";  icon = ics_skull; }
            else            { teks = "DETAILS"; icon = ics_info;  }

            // Hitung jarak index ini dari kursor yang lagi aktif
            int diff = i - contextCursor; 
            
            // Jarak antar baris 15 piksel. Posisi tengah (diff=0) ada di Y=27
            int yPos = 27 + (diff * 15); 

            // Cuma gambar menu yang posisinya ada di area pandang (antara header & footer)
            if (yPos > 10 && yPos < 45) {
                
                if (diff == 0) { 
                    // --- MENU TERPILIH (DI TENGAH BLOK PUTIH) ---
                    // Warna dibalik (Hitam di atas Putih)
                    screen_draw_bitmap_vertikal(0, 26, yPos - 1, icon, 10, 10, BLACK); 
                    rootx_print_text_c(42, yPos, (char*)teks, BLACK, WHITE);
                    
                    // Tambahan efek panah biar kelihatan lebih "Gede/Lebar"
                    rootx_print_text_c(10, yPos, ">", BLACK, WHITE);
                    rootx_print_text_c(110, yPos, "<", BLACK, WHITE);
                } 
                else { 
                    // --- MENU GAK TERPILIH (DI ATAS / DI BAWAH) ---
                    // Warna normal (Putih di atas Hitam)
                    // Posisinya digeser X-nya (+4) biar seakan-akan mundur/mengecil
                    screen_draw_bitmap_vertikal(0, 30, yPos, icon, 10, 10, WHITE);
                    rootx_print_text_c(46, yPos + 1, (char*)teks, WHITE, BLACK);
                }
            }
        }

        // --- 4. FOOTER ---
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_c(2, 55, "< BACK", BLACK, WHITE);
        rootx_print_text_c(85, 55, "[OK] GO", BLACK, WHITE);
    }
    
   
        // --- 4. FOOTER (Tetap) ---

    lcdDrawFinish(&dev);
}





void tampilkandeauthsta() {
    lcdFillScreen(&dev, BLACK);
    
    char buf[64];
    
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_c(2, 1, "ATTACKING STATION...", BLACK, WHITE);
        
        snprintf(buf, sizeof(buf), "Target:%02X:%02X:%02X:%02X:%02X:%02X", 
                 targetSta.mac[0], targetSta.mac[1], targetSta.mac[2],
                 targetSta.mac[3], targetSta.mac[4], targetSta.mac[5]);

        rootx_print_text_c(0, 20, buf, WHITE, BLACK);
        snprintf(buf, sizeof(buf), "Ch: %d", targetTerkunci.channel);
        rootx_print_text_c(0, 30, buf, WHITE, BLACK);
        
        int animasiProgress = (millis() / 30) % 100; 

        drawLoadingBar(14, 42, 100, 8, animasiProgress);
        
        
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_c(2, 55, "< STOP ATTACK", BLACK, WHITE);
    
    lcdDrawFinish(&dev);
}

void tampilkanDeauthScreen() {
    lcdFillScreen(&dev, BLACK);
    
    char buf[64];
    
    if (deauthState == 0) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_c(26, 1, "DEAUTH ATTACK", BLACK, WHITE);
        
        rootx_print_text_c(10, 25, "Attack Target?", WHITE, BLACK);
        
        char shortSsid[16];
        strncpy(shortSsid, targetTerkunci.ssid, 15);
        shortSsid[15] = '\0';
        rootx_print_text_c(10, 35, shortSsid, WHITE, BLACK);

        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_c(2, 55, "< NO", BLACK, WHITE);
        rootx_print_text_c(95, 55, "YES >", BLACK, WHITE);
    } 
    else if (deauthState == 1) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_c(2, 1, "ATTACKING...", BLACK, WHITE);

        snprintf(buf, sizeof(buf), "Target: %s", targetTerkunci.ssid);
        rootx_print_text_c(0, 20, buf, WHITE, BLACK);
        snprintf(buf, sizeof(buf), "Ch: %d", targetTerkunci.channel);
        rootx_print_text_c(0, 30, buf, WHITE, BLACK);
        
        
        int animasiProgress = (millis() / 30) % 100; 

        drawLoadingBar(14, 42, 100, 8, animasiProgress);
        
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_c(2, 55, "< STOP ATTACK", BLACK, WHITE);
    }
    lcdDrawFinish(&dev);
}

void tampilkanBrightness() {
    lcdFillScreen(&dev, BLACK);
    
    char buf[16];

    lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
    rootx_print_text_c(35, 1, "BRIGHTNESS", BLACK, WHITE);

    lcdDrawRect(&dev, 14, 28, 114, 40, WHITE); 
    
    int barWidth = map(brightnessValue, 0, 255, 0, 96);
    lcdDrawFillRect(&dev, 16, 30, 16 + barWidth, 38, WHITE);

    int persen = map(brightnessValue, 0, 255, 0, 100);
    snprintf(buf, sizeof(buf), "%d%%", persen);
    rootx_print_text_c(55, 45, buf, WHITE, BLACK);

    lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
    rootx_print_text_c(5, 55, "[<] BACK", BLACK, WHITE);
    rootx_print_text_c(75, 55, "[UP/DN] SET", BLACK, WHITE);

    lcdDrawFinish(&dev);
}

void setOledBrightness(uint8_t level) {
    // Kodingan i2c lama udah RIP, kita ganti pake LEDC PWM
    
    // Set level kecerahan baru (0 sampai 255)
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, level);
    
    // Eksekusi perubahannya sekarang juga!
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}




void tampilkanSpamScreen(const char* judul, const char* subTeks) {
    lcdFillScreen(&dev, BLACK);
    
    char buf[64];
    
    if (spamState == 0) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_c(2, 1, (char*)judul, BLACK, WHITE);
        
        rootx_print_text_c(10, 25, (char*)subTeks, WHITE, BLACK);

        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_c(2, 55, "< NO", BLACK, WHITE);
        rootx_print_text_c(95, 55, "YES >", BLACK, WHITE);
    } 
    else if (spamState == 1) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_c(2, 1, "RUNNING...", BLACK, WHITE);

        snprintf(buf, sizeof(buf), "Mode: %s", subTeks);
        rootx_print_text_c(0, 25, buf, WHITE, BLACK);
        
        
        int animasiProgress = (millis() / 30) % 100; 
        drawLoadingBar(14, 42, 100, 8, animasiProgress);
        
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_c(2, 55, "< STOP", BLACK, WHITE);
    }
    lcdDrawFinish(&dev);
}








void tampilkanEvilTwinScreen() {
    lcdFillScreen(&dev, BLACK);

    
    
    if (evilTwinState == 0) {
    lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_c(2, 1, "EVIL TWIN", BLACK, WHITE);
        
        rootx_print_text_c(10, 25, "Start Evil Twin?", WHITE, BLACK);

     
        
        rootx_print_text_c(10, 35, targetTerkunci.ssid, WHITE, BLACK);
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_c(2, 55, "< NO", BLACK, WHITE);
        rootx_print_text_c(95, 55, "YES >", BLACK, WHITE);
    } 
    else if (evilTwinState == 1) {
        rootx_print_text_c(15, 20, "WAITING FOR DATA...", WHITE, BLACK);
        int bounce = (millis() / 200) % 5;
        rootx_print_text_c(50, 40 + bounce, "...", WHITE, BLACK);
        rootx_print_text_c(2, 55, "< STOP", WHITE, BLACK);
    }
    else if (evilTwinState == 2) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_c(20, 1, "PW EXPLOITED!", BLACK, WHITE);
        rootx_print_text_c(5, 25, "Target:", WHITE, BLACK);
        rootx_print_text_c(50, 25, targetTerkunci.ssid, WHITE, BLACK);
        rootx_print_text_c(5, 40, "Pass  :", WHITE, BLACK);
        rootx_print_text_c(50, 40, stolenPassword, WHITE, BLACK);
    }
    lcdDrawFinish(&dev);
}


ir_saved_state_t currentIRSavedState = IR_SAVED_STATE_LIST;
SavedRemote_t listSavedRemotes[20];
int totalSavedRemotes = 0;
int savedRemoteIndex = 0;
int actionMenuIndex = 0; // 0 = Transmit, 1 = Hapus

// Panggil fungsi ini pas PERTAMA KALI masuk menu Saved Remote
// Fungsi Parse Data Mentah dari SD Card
void loadSavedRemotes() {
    totalSavedRemotes = 0;
    FILE* f = fopen("/sdcard/ir_log.txt", "r");
    if (!f) return; 

    char line[1500]; // Buffer gede buat baca array
    while (fgets(line, sizeof(line), f) && totalSavedRemotes < 20) {
        char* token = strtok(line, "|");
        if (!token) continue;
        strcpy(listSavedRemotes[totalSavedRemotes].nama, token);
        
        token = strtok(NULL, "|");
        if (!token) continue;
        listSavedRemotes[totalSavedRemotes].num_pulses = atoi(token);
        
        token = strtok(NULL, "|");
        char* p_token = strtok(token, ",");
        int idx = 0;
        while (p_token != NULL && idx < 200) {
            listSavedRemotes[totalSavedRemotes].pulses[idx] = atoi(p_token);
            p_token = strtok(NULL, ",");
            idx++;
        }
        totalSavedRemotes++;
    }
    fclose(f);
}

// --- Potongan buat nampilin layar Hasil ---


void tampilkanMenuSavedIR() {
    lcdFillScreen(&dev, BLACK);
 // Bersihin layar (ID 0)

    if (currentIRSavedState == IR_SAVED_STATE_LIST) {
        // --- HEADER (BLOK PUTIH) ---
        // Format library lu: (ID, X, Y, Teks, Warna Teks, Warna Background)
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_c(2, 1, "SAVED REMOTE", BLACK, WHITE);

        if (totalSavedRemotes == 0) {
            rootx_print_text_c(10, 25, "Data Kosong!", WHITE, BLACK);
        } else {
            // Tampilkan max 3 item biar rapi (Paging logic)
            int startIdx = (savedRemoteIndex / 3) * 3;
            for (int i = 0; i < 3; i++) {
                int curr = startIdx + i;
                if (curr >= totalSavedRemotes) break;

                char buf[32];
                if (curr == savedRemoteIndex) {
                    snprintf(buf, sizeof(buf), "> %s", listSavedRemotes[curr].nama);
                } else {
                    snprintf(buf, sizeof(buf), "  %s", listSavedRemotes[curr].nama);
                }
                rootx_print_text_c(0, 16 + (i * 12), buf, WHITE, BLACK);
            }
        }

        // --- FOOTER (BLOK PUTIH) ---
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_c(2, 55, "< NO", BLACK, WHITE);
        rootx_print_text_c(95, 55, "OK >", BLACK, WHITE);
    } 
    else if (currentIRSavedState == IR_SAVED_STATE_ACTION) {
        char buf[32];
        
        snprintf(buf, sizeof(buf), " ACTION: %s ", listSavedRemotes[savedRemoteIndex].nama);
        // Header
        
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_c(2, 1, buf, BLACK, WHITE);

        // Menu Transmit / Hapus
        if (actionMenuIndex == 0) {
            rootx_print_text_c(15, 20, "> 1. TRANSMIT", WHITE, BLACK);
            rootx_print_text_c(15, 35, "  2. HAPUS", WHITE, BLACK);
        } else {
            rootx_print_text_c(15, 20, "  1. TRANSMIT", WHITE, BLACK);
            rootx_print_text_c(15, 35, "> 2. HAPUS", WHITE, BLACK);
        }
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_c(2, 55, "< NO", BLACK, WHITE);
    } 
    else if (currentIRSavedState == IR_SAVED_STATE_SENDING) {
        // Layar Polos, Tulisan di Tengah!
        rootx_print_text_c(25, 25, "IR SEND!", WHITE, BLACK);
    }

    // Refresh layar ID 0, dan force update (true)
    lcdDrawFinish(&dev); 
}

void tampilkanMenuIR() {
    lcdFillScreen(&dev, BLACK);

    char buf[32];

    if (currentIRState == IR_STATE_CONFIRM) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_c(2, 1, "SNIFF IR SIGNAL", BLACK, WHITE);
        rootx_print_text_c(10, 25, "Start Sniff??", WHITE, BLACK);
        
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_c(2, 55, "< NO", BLACK, WHITE);
        rootx_print_text_c(95, 55, "OK >", BLACK, WHITE);
    
    } 
    else if (currentIRState == IR_STATE_WAITING) {
        rootx_print_text_c(5, 20, "Menunggu", WHITE, BLACK);
        rootx_print_text_c(5, 40, "sinyal masuk...", WHITE, BLACK);
    } 
    else if (currentIRState == IR_STATE_RESULT) {
        rootx_print_text_c(0, 0, "== IR RESULT ==", WHITE, BLACK);
        rootx_print_text_c(0, 16, "Type: RAW CLONER", WHITE, BLACK);
        
        snprintf(buf, sizeof(buf), "Pulses: %d", last_ir_data.num_pulses);
        rootx_print_text_c(0, 30, buf, WHITE, BLACK);
        
        rootx_print_text_c(0, 56, "> SD Card Saved <", WHITE, BLACK);
    }
    lcdDrawFinish(&dev);
}



// ==========================================
// MESIN TETRIS VERTIKAL (Miring 90 Derajat)
// ==========================================




void renderAboutScreen() {
    lcdFillScreen(&dev, BLACK);


    // Bikin border kotak di pinggir layar biar UI-nya rapi
    lcdDrawRect(&dev, 0, 0, 128, 64, WHITE);
    lcdDrawRect(&dev, 2, 2, 126, 62, WHITE); // Border dalem (double line)

    // Judul
    rootx_print_text_c(32, 8, "ROOTX OS", WHITE, BLACK);
    lcdDrawLine(&dev, 25, 18, 103, 18, WHITE); // Garis bawah judul

    // Info Alat (Lu bisa ganti teksnya sesuka lu Cok!)
    rootx_print_text_c(10, 25, "Ver : 1.0.0", WHITE, BLACK);
    rootx_print_text_c(10, 35, "Core: ESP32-S3", WHITE, BLACK);
    rootx_print_text_c(10, 45, "By  : Andyy", WHITE, BLACK); // Ganti pake nama lu!

    // Tombol Keluar
    rootx_print_text_c(90, 45, "[<]", WHITE, BLACK); // Logo Kiri buat exit

    lcdDrawFinish(&dev);
}

void renderRebootScreen() {
    lcdFillScreen(&dev, BLACK);


    // Border Frame biar keren
    lcdDrawRect(&dev, 5, 5, 123, 59, WHITE);

    // Teks Pertanyaan
    rootx_print_text_c(20, 20, "Reboot sekarang?", WHITE, BLACK);

    // Petunjuk Tombol
    
    rootx_print_text_c(2, 55, "< NO", WHITE, BLACK);
    rootx_print_text_c(95, 55, "OK >", WHITE, BLACK);

    lcdDrawFinish(&dev);
}

// Variabel State buat SD Manager
// Variabel State buat SD Manager
int sdActionIdx = 0; // 0: EXIT, 1: FILES, 2: FORMAT
int sdState = 0;     // 0: Main Dashboard, 1: Confirm Format, 2: Formatting

void renderSdManager() {
    lcdFillScreen(&dev, BLACK);

    
    // --- 1. HEADER ---
    for(int y = 0; y < 11; y++) lcdDrawLine(&dev, 0, y, 128, y, WHITE);
    rootx_print_text_c(34, 2, "SD MANAGER", BLACK, WHITE);

    if (sdState == 0) { // DASHBOARD UTAMA
        struct statvfs st;
        float total_mb = 0, free_mb = 0, used_mb = 0;
        int percent = 0;
        bool is_mounted = false;

        // --- TRIK FIX 0MB: Coba 3 variasi path ---
        if (statvfs("/sdcard", &st) == 0) {
            is_mounted = true;
        } else if (statvfs("/sdcard/", &st) == 0) {
            is_mounted = true;
        } else if (statvfs("fatfs", &st) == 0) { // Kadang internal ESP32 kenalnya ini
            is_mounted = true;
        }

        if (is_mounted) {
            // Gunakan uint64_t buat kalkulasi biar gak overflow
            uint64_t total_bytes = (uint64_t)st.f_blocks * (uint64_t)st.f_frsize;
            uint64_t free_bytes = (uint64_t)st.f_bfree * (uint64_t)st.f_frsize;
    
            total_mb = (float)total_bytes / (1024.0 * 1024.0);
            free_mb = (float)free_bytes / (1024.0 * 1024.0);
            used_mb = total_mb - free_mb;
            
            if (total_mb > 0) percent = (int)((used_mb / total_mb) * 100);
        }

        // --- 3. DISPLAY INFO ---
        char buf[32];
        if (!is_mounted) {
            rootx_print_text_c(5, 20, "VFS Error!", WHITE, BLACK);
            rootx_print_text_c(5, 30, "Gagal Baca Size", WHITE, BLACK);
        } else {
            // Tampilan Size
            snprintf(buf, sizeof(buf), "Size: %.0f MB", total_mb);
            rootx_print_text_c(5, 15, buf, WHITE, BLACK);
            // Tampilan Free
            snprintf(buf, sizeof(buf), "Free: %.0f MB", free_mb);
            rootx_print_text_c(5, 25, buf, WHITE, BLACK);
        }
        
        // --- 4. PROGRESS BAR ---
        snprintf(buf, sizeof(buf), "%d%%", percent);
        rootx_print_text_c(100, 25, buf, WHITE, BLACK);
        lcdDrawRect(&dev, 5, 38, 123, 46, WHITE);
        
        int fillWidth = (percent * 114) / 100;
        if (fillWidth > 114) fillWidth = 114;
        if (fillWidth > 0) {
            for (int i = 0; i < 4; i++) {
                lcdDrawLine(&dev, 7, 40 + i, 7 + fillWidth, 40 + i, WHITE);
            }
        }

        // --- 5. MENU ---
        const char* menuNames[] = {"[ EXIT ]", "[ FILES ]", "[ FORMAT ]"};
        int textLen = strlen(menuNames[sdActionIdx]) * 6;
        int startX = (128 - textLen) / 2;

        rootx_print_text_c(5, 54, "<", WHITE, BLACK);
        rootx_print_text_c(startX, 54, menuNames[sdActionIdx], WHITE, BLACK);
        rootx_print_text_c(117, 54, ">", WHITE, BLACK);
    }
   
    else if (sdState == 1) { // KONFIRMASI FORMAT
        rootx_print_text_c(15, 20, "FORMAT SD CARD?", WHITE, BLACK);
        rootx_print_text_c(25, 32, "ALL DATA LOST!", WHITE, BLACK);
        rootx_print_text_c(5, 50, "[<-] NO   [OK] YES", WHITE, BLACK);
    } 
    else if (sdState == 2) { // FORMATTING
        rootx_print_text_c(25, 30, "FORMATTING...", WHITE, BLACK);
    }

    lcdDrawFinish(&dev);
}


// Definisi variabel Global buat File Explorer
char sdFileNames[MAX_FILES][32];
int sdTotalFiles = 0;
int sdFileCursor = 0;
int sdFileScroll = 0;
int sdFileState = 0; 
bool isFileExpInit = false;
char currentPath[256] = "/sdcard";

void renderFileExplorer() {
    // --- 1. INIT: BACA SD CARD ---
        // --- 1. INIT: BACA SD CARD ---
    if (!isFileExpInit) {
        sdTotalFiles = 0;
        sdFileCursor = 0;
        sdFileScroll = 0;
        sdFileState = 0;

        // FIX: Pake variabel currentPath, jangan "/sdcard" !!!
        DIR *d = opendir(currentPath);
        if (d) {
            struct dirent *dir;
            while ((dir = readdir(d)) != NULL && sdTotalFiles < MAX_FILES) {
                // Lewati folder tersembunyi
                if (dir->d_name[0] == '.') continue; 
                
                // Simpan nama file/folder ke array
                strncpy(sdFileNames[sdTotalFiles], dir->d_name, 31);
                sdFileNames[sdTotalFiles][31] = '\0';
                sdTotalFiles++;
            }
            closedir(d);
        }
        isFileExpInit = true;
    }

    lcdFillScreen(&dev, BLACK);

    // --- 2. HEADER UI ---
    for(int y = 0; y < 11; y++) lcdDrawLine(&dev, 0, y, 128, y, WHITE);
    rootx_print_text_c(38, 2, "SD FILES", BLACK, WHITE);

    if (sdTotalFiles == 0) {
        rootx_print_text_c(15, 30, "NO FILES FOUND!", WHITE, BLACK);
        rootx_print_text_c(30, 50, "[<-] BACK", WHITE, BLACK);
    } 
    else {
        if (sdFileState == 0) { // MODE LISTING
            // --- 3. RENDER DAFTAR FILE (Max 5 baris) ---
            int maxList = 5; 
            for (int i = 0; i < maxList; i++) {
                int fileIdx = sdFileScroll + i;
                if (fileIdx >= sdTotalFiles) break;

                int yPos = 14 + (i * 10); // Jarak antar baris 10 pixel

                if (fileIdx == sdFileCursor) {
                    // Kursor Aktif: Kasih panah dan hurufnya kita Invert biar keren
                    rootx_print_text_c(0, yPos, ">", WHITE, BLACK);
                    // Kotak Invert Background kursor
                    lcdDrawRect(&dev, 8, yPos - 1, 128, yPos + 8, WHITE);
                    rootx_print_text_c(10, yPos, sdFileNames[fileIdx], BLACK, WHITE);
                } else {
                    // File biasa
                    rootx_print_text_c(10, yPos, sdFileNames[fileIdx], WHITE, BLACK);
                }
            }
            

            // Ganti dari "[OK] DEL" jadi "[OK] SEL/DEL" (Select / Delete)
char foot[32]; 
snprintf(foot, sizeof(foot), "%d/%d [OK] SEL/DEL", sdFileCursor + 1, sdTotalFiles);
rootx_print_text_c(5, 56, foot, WHITE, BLACK);

            rootx_print_text_c(5, 56, foot, WHITE, BLACK);
        } 
        else if (sdFileState == 1) { // MODE CONFIRM DELETE
            rootx_print_text_c(20, 20, "DELETE FILE?", WHITE, BLACK);
            // Tulis nama file yg mau dihapus (Max 18 karakter biar muat di tengah)
            char truncName[20];
            snprintf(truncName, sizeof(truncName), "%.18s", sdFileNames[sdFileCursor]);
            rootx_print_text_c(10, 32, truncName, WHITE, BLACK);
            
            rootx_print_text_c(5, 50, "[<-] NO   [OK] YES", WHITE, BLACK);
        }
    }

    lcdDrawFinish(&dev);
}

// Definisi state TV-B-Gone
int tvbgoneState = 0;
int tvbgoneMenuIdx = 0;
int tvbgoneProgress = 0;
int tvbgoneTotal = 0;

void renderTvBGone() {
    lcdFillScreen(&dev, BLACK);

    
    // --- 1. HEADER PRESISI ---
    // Kotak putih Y: 0-12
    lcdDrawRect(&dev, 0, 0, 128, 12, WHITE);
    // Teks 9 huruf x 6px = 54. X = (128-54)/2 = 37
    rootx_print_text_c(37, 2, "TV-B-GONE", BLACK, WHITE);
    
    if (tvbgoneState == 0) { // MODE MENU PILIH REGION
        const char* menus[] = {"[ NA / ASIA ]", "[  EUROPE   ]", "[ ALL WORLD ]"};
        
        for(int i = 0; i < 3; i++) {
            int yPos = 20 + (i * 12);
            
            if (i == tvbgoneMenuIdx) {
                // Kursor Aktif
                rootx_print_text_c(18, yPos, ">", WHITE, BLACK);
                lcdDrawRect(&dev, 26, yPos - 1, 104, yPos + 8, WHITE); // Highlight
                rootx_print_text_c(28, yPos, menus[i], BLACK, WHITE);
            } else {
                rootx_print_text_c(28, yPos, menus[i], WHITE, BLACK);
            }
        }
        
        // Footer Petunjuk Tombol
        rootx_print_text_c(5, 55, "[<-] EXIT    [OK] START", WHITE, BLACK);

    } 
    else if (tvbgoneState == 1) { // MODE FIRING (LAGI NEMBAK)
        
        // Animasi Teks Kedip
        if ((xTaskGetTickCount() * portTICK_PERIOD_MS) / 500 % 2 == 0) {
            rootx_print_text_c(18, 20, "TRANSMITTING...", WHITE, BLACK);
        }
        
        // --- PROGRESS BAR MATEMATIS ---
        // Border Bar (X: 10, Lebar: 108)
        lcdDrawRect(&dev, 10, 35, 118, 45, WHITE);
        
        if (tvbgoneTotal > 0) {
            // Hitung lebar isi bar (Maks 104 pixel)
            int fillWidth = (tvbgoneProgress * 104) / tvbgoneTotal;
            if (fillWidth > 0) {
                lcdDrawRect(&dev, 12, 37, 12 + fillWidth, 43, WHITE); // Fill Bar
            }
        }
        
        // Teks Counter
        char counter[32];
        snprintf(counter, sizeof(counter), "CODE: %d / %d", tvbgoneProgress, tvbgoneTotal);
        // Tengahin teks counter (Asumsi maks 15 char = 90px. X = (128-90)/2 = 19)
        rootx_print_text_c(19, 50, counter, WHITE, BLACK);
    }
    
    lcdDrawFinish(&dev);
}
