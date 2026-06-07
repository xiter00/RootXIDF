#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "globals.h"
#include "photo_data.h"
#include <math.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include <dirent.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "driver/ledc.h"
#include <math.h>
#include <sys/statvfs.h> // Wajib buat baca kapasitas memori
#include "esp_random.h"



#define MAX_STARS 15
#define PANEL_W 150  
#define PANEL_DARK 4  
#define ITEM_H   22    // Tinggi per item (px)
#define ICON_X    5    // X icon
#define TEXT_X   20    // X teks label
#define BAR_H    (ITEM_H - 3)  // Tinggi blok highlight
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

void rootx_print_text_custom(int x, int y, const char* str, uint16_t fg, uint16_t bg) {
    lcdDrawCustomString(&dev, fontC7x13, x, y, (char*)str, fg, bg); 
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
void renderDinoGame(void);
void tampilkanMenuIR(void);
void tampilkanMenuSavedIR(void);
void renderSnakeGame(void); 
void renderTvBGone(void);
void renderTetrisGame(void);

bool introDone = false;


void init_joystick() {
    int pins[] = {PIN_UP, PIN_DOWN, PIN_LEFT, PIN_RIGHT, PIN_OK};
    for(int i = 0; i < 5; i++) {
        gpio_set_direction(pins[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(pins[i], GPIO_PULLUP_ONLY);
    }
}


// Asumsikan dev dan batteryPercent sudah global (dari globals.h)



// Definisikan font yang digunakan (contoh, sesuaikan dengan file font yang tersedia)
// #include "font8x16.h"
// #include "font5x7.h"

// Warna khusus
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

// Fungsi bantu: menggambar dashed circle langsung ke buffer (jika frame buffer aktif)
static void draw_dashed_circle_to_buffer(TFT_t *dev, int xc, int yc, int r, uint16_t color, int dash_deg, int gap_deg) {
    if (!dev->_use_frame_buffer || dev->_frame_buffer == NULL) return;
    uint16_t *buf = dev->_frame_buffer;
    int w = dev->_width;
    int h = dev->_height;

    for (int angle = 0; angle < 360; angle++) {
        int cycle = angle % (dash_deg + gap_deg);
        if (cycle >= dash_deg) continue;

        float rad = angle * M_PI / 180.0f;
        int x = xc + (int)(r * cosf(rad));
        int y = yc + (int)(r * sinf(rad));
        if (x >= 0 && x < w && y >= 0 && y < h) {
            buf[y * w + x] = color;
        }
    }
}

static void draw_hacker_panel(void) {
    uint16_t *buf = dev._frame_buffer;
    int w = dev._width;   // 240
    int h = dev._height;  // 135

    // --- 1. Darken panel kiri ---
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < PANEL_W; x++) {
            uint16_t px = buf[y * w + x];
            uint8_t r = (((px >> 11) & 0x1F) << 3) / PANEL_DARK;
            uint8_t g = (((px >> 5)  & 0x3F) << 2) / PANEL_DARK;
            uint8_t b = ((px & 0x1F) << 3)          / PANEL_DARK;
            buf[y * w + x] = rgb565(r, g, b);
        }
    }

    // --- 2. Grid halus di panel kiri (12px pitch) ---
    uint16_t GRID_C = rgb565(13, 2, 5);
    for (int y = 0; y < h; y += 12) {
        for (int x = 0; x < PANEL_W; x++) {
            buf[y * w + x] = GRID_C;
        }
    }
    for (int x = 0; x < PANEL_W; x += 12) {
        for (int y = 0; y < h; y++) {
            buf[y * w + x] = GRID_C;
        }
    }

    // --- 3. Garis pembatas kanan panel (pink tipis) ---
   
    for (int y = 0; y < h; y++) {
        buf[y * w + (PANEL_W - 1)] = PINK;
    }

    // --- 4. Top bar: garis merah di atas panel ---
    for (int x = 0; x < PANEL_W; x++) {
        buf[0 * w + x] = PINK;
        buf[1 * w + x] = PINK;
    }

    // --- 5. Bottom bar: garis cyan di bawah panel ---
    for (int x = 0; x < PANEL_W; x++) {
        buf[(h-1) * w + x] = CYAN;
    }
}

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
} else if (appMode == 11) {
renderDinoGame();
} else if (appMode == 12) {       // <--- TAMBAHIN INI
            renderSnakeGame();            // <--- TAMBAHIN INI
        } else if (appMode == MODE_IR_SNIFFER) {    // <--- TAMBAHIN INI
            tampilkanMenuIR();
        } else if (appMode == 13) {       // <--- TAMBAHIN INI (TETRIS MIRING)
            renderTetrisGame();           // <--- TAMBAHIN INI
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

// --- DATA UNTUK ANIMASI BINTANG ---



// Inisialisasi bintang pertama kali
extern void screen_draw_bitmap(uint8_t id, int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color);

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
iconSmall_scan,
iconSmall_sniff,
iconSmall_spam,
iconSmall_wifi
};

const unsigned char* iconListBLE[]  = {
iconSmall_scan,
iconSmall_apple,
iconSmall_android
};

const unsigned char* iconListIR[]   = {
iconSmall_ir,
iconSmall_tv,
iconSmall_ac,
iconSmall_lock,
iconSmall_saved 
};

const unsigned char* iconListSet[]  = {
iconSmall_bright,
iconSmall_saved,
iconSmall_info,
iconSmall_repeat 
};

const unsigned char* iconListGame[]  = {
iconSmall_bright,
iconSmall_bright,
iconSmall_bright
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
    
    
    
    rootx_print_text_custom(95, 2, "<RootX>", RED, RED);
    rootx_print_text_custom(85, 122, "Dev: Andyy", WHITE, WHITE);
    
    
    
    
    rootx_print_text_kecil(75, 75, "<", ICE_CYAN, ICE_CYAN);
    rootx_print_text_kecil(86, 75, menuList[carouselCurrentIdx].label, WHITE, WHITE);
    
    apply_cyber_glitch();
    lcdDrawFinish(&dev);
}

static void draw_menu_item(int yPos, bool isActive,
                           const unsigned char* icon, const char* label) {
    uint16_t *buf = dev._frame_buffer;
    int w = dev._width;



    if (isActive) {
        // --- Gradasi horizontal pink → transparan ---
        for (int x = 0; x < PANEL_W - 1; x++) {
            float ratio = (float)x / (float)(PANEL_W - 2);
            // Makin ke kanan makin gelap/transparan
            float alpha = 0.85f * (1.0f - ratio * ratio);
            // Target warna pink gelap: rgb(205, 25, 73)
            uint8_t r = (uint8_t)(205 * alpha);
            uint8_t g = (uint8_t)(25  * alpha);
            uint8_t b = (uint8_t)(73  * alpha);
            for (int y = yPos; y < yPos + BAR_H; y++) {
                if (y < dev._height) {
                    // Blend ke atas pixel background yg ada
                    uint16_t bg = buf[y * w + x];
                    uint8_t rb = (((bg >> 11) & 0x1F) << 3);
                    uint8_t gb = (((bg >> 5)  & 0x3F) << 2);
                    uint8_t bb = ((bg & 0x1F) << 3);
                    uint8_t rf = (uint8_t)(rb * (1.0f - alpha) + r);
                    uint8_t gf = (uint8_t)(gb * (1.0f - alpha) + g);
                    uint8_t bf = (uint8_t)(bb * (1.0f - alpha) + b);
                    buf[y * w + x] = rgb565(rf, gf, bf);
                }
            }
        }

        // --- Cyan border kiri 2px (glow effect) ---
        for (int y = yPos; y < yPos + BAR_H; y++) {
            if (y < dev._height) {
                buf[y * w + 0] = CYAN;
                buf[y * w + 1] = CYAN;
                // Pixel ke-3 setengah cyan (soft glow)
                buf[y * w + 2] = rgb565(0, 128, 128);
            }
        }

        // --- Data stream glitch: kotak hitam ngalir dari kanan ke kiri ---
        int slide   = (millis() / 40) % (PANEL_W / 2);
        int animX   = (PANEL_W - 15) - slide;
        if (animX > 3 && animX + 8 < PANEL_W) {
            lcdDrawFillRect(&dev, animX,     yPos, animX + 2, yPos + BAR_H - 1, BLACK);
            lcdDrawFillRect(&dev, animX + 5, yPos, animX + 8, yPos + BAR_H - 1, BLACK);
        }

        // --- Icon bounce (naik-turun 2px) ---
        int bounce = getBounce(200, 2);
        if (icon) screen_draw_bitmap(0, ICON_X, yPos + 5 + bounce, icon, 10, 10, CYAN);

        // --- Teks putih ---
        rootx_print_text_custom(TEXT_X, yPos + 6, label, WHITE, BLACK);

        // --- Titik cursor di ujung kanan ---
        int dotX = PANEL_W - 8;
        int dotY = yPos + BAR_H / 2 - 1;
        lcdDrawFillRect(&dev, dotX, dotY, dotX + 3, dotY + 3, CYAN);

    } else {
        // --- Item non-aktif: icon & teks abu-abu ---
        if (icon) screen_draw_bitmap(0, ICON_X, yPos + 5, icon, 10, 10, GRAY);
        rootx_print_text_custom(TEXT_X, yPos + 6, label, GRAY, BLACK);
    }

    // --- Garis pembatas bawah item (sangat tipis) ---
    uint16_t SEP = rgb565(20, 5, 10);
    for (int x = 4; x < PANEL_W - 4; x++) {
        int sepY = yPos + ITEM_H - 1;
        if (sepY < dev._height) buf[sepY * dev._width + x] = SEP;
    }
}


// ----------------------------------------------------------------
// FUNGSI UTAMA — Ganti tampilkanMenuUtama() yang lama dengan ini
// ----------------------------------------------------------------
void tampilkanMenuUtama(void) {

    // === 1. ANIME BACKGROUND TETAP ===
    drawBackground();   // <-- sama kayak menu logo

    // === 2. PANEL KIRI DARK OVERLAY ===
    draw_hacker_panel();

    // === 3. BATTERY INDICATOR (kanan atas, sama kayak menu logo) ===
    read_battery_percentage();

    lcdDrawRect(&dev, 216, 4, 234, 12, rgb565(255,255,255));
    lcdDrawFillRect(&dev, 235, 7, 237, 9, rgb565(255,255,255));

    uint16_t warna_bar;
    int jumlah_bar;
    if      (batteryPercent > 75) { warna_bar = GREEN;  jumlah_bar = 4; }
    else if (batteryPercent > 50) { warna_bar = YELLOW; jumlah_bar = 3; }
    else if (batteryPercent > 25) { warna_bar = ORANGE; jumlah_bar = 2; }
    else                          { warna_bar = RED;    jumlah_bar = 1; }

    for (int b = 0; b < jumlah_bar; b++) {
        int bx = 218 + (b * 4);
        lcdDrawFillRect(&dev, bx, 6, bx + 2, 10, warna_bar);
    }

    // === 4. HEADER KATEGORI ===
    // Contoh:  "> WI-FI // NETWORK TOOLS"
    // Format:  CYAN ">" RED "NAMA" GRAY "// SUB"
    uint16_t CYAN  = rgb565(0, 255, 255);
    uint16_t PINK  = rgb565(255, 30, 90);
    uint16_t GRAY  = rgb565(80, 80, 80);
    uint16_t WHITE = rgb565(255, 255, 255);
    uint16_t BLACK = rgb565(0, 0, 0);

    // Teks ">" cyan
    rootx_print_text_custom(3, 4, ">", CYAN, BLACK);

    // Nama kategori merah/pink
    const char* catLabel = "";
    const char* catSub   = "";
    int totalSub = 0;

    if      (currentMenu == 0) { catLabel = "WI-FI";    catSub = "NETWORK"; totalSub = 4; }
    else if (currentMenu == 1) { catLabel = "BLE";      catSub = "BLUETOOTH"; totalSub = 3; }
    else if (currentMenu == 2) { catLabel = "IR";       catSub = "INFRARED"; totalSub = 5; }
    else if (currentMenu == 3) { catLabel = "SETTINGS"; catSub = "SYSTEM"; totalSub = 4; }
    else                       { catLabel = "GAME";     catSub = "ARCADE"; totalSub = 3; }

    rootx_print_text_custom(12, 4, catLabel, PINK, BLACK);

    // Sub-label abu
    int subX = 12 + strlen(catLabel) * 6 + 4;
    rootx_print_text_custom(subX, 4, "//", GRAY, BLACK);
    rootx_print_text_custom(subX + 14, 4, catSub, GRAY, BLACK);

    // Garis pembatas header (pink fade)
    for (int x = 0; x < PANEL_W - 1; x++) {
        float t = (float)x / (PANEL_W - 2);
        uint8_t r = (uint8_t)(255 * (1.0f - t * 0.8f));
        uint8_t g = (uint8_t)(30  * (1.0f - t));
        uint8_t b = (uint8_t)(90  * (1.0f - t));
        int lineY = 14;
        dev._frame_buffer[lineY * dev._width + x] = rgb565(r, g, b);
        // Garis kedua lebih tipis
        if (x < PANEL_W / 2)
            dev._frame_buffer[(lineY+1) * dev._width + x] = rgb565(r/3, g/3, b/3);
    }

    // === 5. LIST ITEM MENU ===
    int maxVisible = 5;
    int start_y    = 18;   // Y mulai item pertama (setelah header)

    for (int i = 0; i < maxVisible; i++) {
        int itemIndex = topMenu + i;
        if (itemIndex >= totalSub) break;

        bool isActive = (itemIndex == currentSub);
        int  yPos     = start_y + (i * ITEM_H);

        // Ambil icon sesuai kategori
        const unsigned char* iconSmall = NULL;
        if      (currentMenu == 0) iconSmall = iconListWiFi[itemIndex];
        else if (currentMenu == 1) iconSmall = iconListBLE[itemIndex];
        else if (currentMenu == 2) iconSmall = iconListIR[itemIndex];
        else if (currentMenu == 3) iconSmall = iconListSet[itemIndex];
        else                       iconSmall = iconListGame[itemIndex];

        // Ambil label teks
        const char* label = "";
        if      (currentMenu == 0) label = subMenuWiFi[itemIndex];
        else if (currentMenu == 1) label = subMenuBLE[itemIndex];
        else if (currentMenu == 2) label = subMenuIR[itemIndex];
        else if (currentMenu == 3) label = subMenuSet[itemIndex];
        else                       label = subMenuGame[itemIndex];

        draw_menu_item(yPos, isActive, iconSmall, label);
    }

    // === 6. SCROLL INDICATOR (titik-titik di tepi kanan panel) ===
    // Kalau item > maxVisible, kasih scroll dots biar tau posisi
    if (totalSub > maxVisible) {
        int dot_x = PANEL_W - 5;
        for (int d = 0; d < totalSub; d++) {
            int dot_y = start_y + (d * (dev._height - start_y - 4) / totalSub);
            if (d == currentSub) {
                // Dot aktif: cyan
                lcdDrawFillRect(&dev, dot_x, dot_y, dot_x + 3, dot_y + 3, CYAN);
            } else {
                // Dot biasa: dark pink
                lcdDrawFillRect(&dev, dot_x, dot_y, dot_x + 2, dot_y + 2, PINK);
            }
        }
    }

    // === 7. GLITCH EFFECT TERAKHIR ===
    apply_cyber_glitch();

    // === 8. FLUSH KE LAYAR ===
    lcdDrawFinish(&dev);
}


// --- TARUH INI DI ATAS FUNGSI ---


void tampilkanTrackScreen() {
    lcdFillScreen(&dev, BLACK);
    

    char buf[32];
    
    // --- ANIMASI FLOATING ICON (Icon WiFi naik turun pelan) ---
    int floatY = 15 + (int)(sin(millis() / 300.0) * 3);
    screen_draw_bitmap(0, 105, floatY, iconSmall_wifi, 10, 10, WHITE);

    // Header
    lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
    rootx_print_text_custom(30, 1, "TRACKING RSSI", BLACK, WHITE);
    
    // --- ANIMASI RADAR PULSING ---
    static int r = 0;
    r++; if(r > 20) r = 0;
    lcdDrawCircle(&dev, 108, 18, r, WHITE);
    if(r > 5) lcdDrawCircle(&dev, 105, 15, r - 5, WHITE);

    // Data RSSI
    snprintf(buf, sizeof(buf), "%d", targetTerkunci.rssi);
    rootx_print_text_custom(50, 30, buf, WHITE, BLACK);

    // Footer
    lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
    rootx_print_text_custom(5, 55, "< BACK", BLACK, WHITE);
    
    lcdDrawFinish(&dev);
}


void tampilkanWifiScanner() {
    lcdFillScreen(&dev, BLACK);
    
    char buf[64]; 

    if (scannerState == 0) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_custom(2, 1, "WIFI SCANNER", BLACK, WHITE);

        rootx_print_text_custom(40, 25, "Yakin??", WHITE, BLACK);

        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_custom(2, 55, "< CANCEL", BLACK, WHITE);
        rootx_print_text_custom(95, 55, "YES >", BLACK, WHITE);
    }
    else if (scannerState == 1) {
        rootx_print_text_custom(20, 25, "Scanning Air...", WHITE, BLACK);
        if (scanDone) scannerState = 2; 
    }
    else if (scannerState == 2) {
        if (totalWiFi == 0) {
            lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
            rootx_print_text_custom(2, 1, "SAVED NETWORKS", BLACK, WHITE);
            rootx_print_text_custom(15, 25, "BELUM ADA DATA!", WHITE, BLACK);
            rootx_print_text_custom(10, 35, "Scan WiFi dulu", WHITE, BLACK);
            lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
            rootx_print_text_custom(2, 55, "< BACK", BLACK, WHITE);
        } else {
          

            lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
            snprintf(buf, sizeof(buf), "SCANNER - %d", totalWiFi);
            rootx_print_text_custom(2, 1, buf, BLACK, WHITE);
            
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
                    rootx_print_text_custom(1, yPos + 1, buf, textColor, bgColor);
                    
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
                    rootx_print_text_custom(16, yPos + 1, textShow, textColor, bgColor);

                    snprintf(buf, sizeof(buf), "C:%d", listWiFi[itemIdx].channel);
                    rootx_print_text_custom(66, yPos + 1, buf, textColor, bgColor);
                    snprintf(buf, sizeof(buf), "%ddB", listWiFi[itemIdx].rssi);
                    rootx_print_text_custom(95, yPos + 1, buf, textColor, bgColor);
                }
            }
            lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
            rootx_print_text_custom(2, 55, "< BACK", BLACK, WHITE);
            rootx_print_text_custom(53, 55, "[OK]", BLACK, WHITE);
        }
    }
    else if (scannerState == 3) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_custom(22, 1, "DETAIL TARGET", BLACK, WHITE);

        int xSide = 5; 
        
        rootx_print_text_custom(xSide, 13, "SSID: ", WHITE, BLACK);
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
        rootx_print_text_custom(xSide + 35, 13, tmpSSID, WHITE, BLACK);

        rootx_print_text_custom(xSide, 23, "MAC : ", WHITE, BLACK);
        rootx_print_text_custom(xSide + 35, 23, targetTerkunci.mac, WHITE, BLACK);
        
        snprintf(buf, sizeof(buf), "CH  : %d", targetTerkunci.channel);
        rootx_print_text_custom(xSide, 33, buf, WHITE, BLACK);

        snprintf(buf, sizeof(buf), "SIG : %d dBm", targetTerkunci.rssi);
        rootx_print_text_custom(xSide, 43, buf, WHITE, BLACK);

        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_custom(2, 55, "[<] BACK", BLACK, WHITE);
    } 
                else if (scannerState == 4) { // Atau scannerStateSta == 4, sesuaikan aja
        // --- 1. HEADER ---
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_custom(42, 1, "ACTIONS", BLACK, WHITE);

        // --- 2. BLOK PUTIH STATIS DI TENGAH ---
        // Y mulai dari 24, tinggi 16 piksel (Bener-bener pas di center OLED 128x64)
        lcdDrawFillRect(&dev, 0, 24, 128, 40, WHITE);

        // --- 3. LOGIKA ROLLING MENU ---
        for(int i = 0; i < 5; i++) {
            const char* teks;
            const unsigned char* icon;
            
            // Set Teks dan Icon
            if(i == 0)      { teks = "DEAUTH "; icon = iconSmall_skull; }
            else if(i == 1) { teks = "EVIL TWIN"; icon = iconSmall_conn; }
            else if(i == 2) { teks = "CLIENTS"; icon = iconSmall_sniff; }
            else if(i == 3) { teks = "TRACK  "; icon = iconSmall_wifi;  } 
            else            { teks = "DETAILS"; icon = iconSmall_info;  }

            // Hitung jarak index ini dari kursor yang lagi aktif
            int diff = i - contextCursor; 
            
            // Jarak antar baris 15 piksel. Posisi tengah (diff=0) ada di Y=27
            int yPos = 27 + (diff * 15); 

            // Cuma gambar menu yang posisinya ada di area pandang (antara header & footer)
            if (yPos > 10 && yPos < 45) {
                
                if (diff == 0) { 
                    // --- MENU TERPILIH (DI TENGAH BLOK PUTIH) ---
                    // Warna dibalik (Hitam di atas Putih)
                    screen_draw_bitmap(0, 26, yPos - 1, icon, 10, 10, BLACK); 
                    rootx_print_text_custom(42, yPos, (char*)teks, BLACK, WHITE);
                    
                    // Tambahan efek panah biar kelihatan lebih "Gede/Lebar"
                    rootx_print_text_custom(10, yPos, ">", BLACK, WHITE);
                    rootx_print_text_custom(110, yPos, "<", BLACK, WHITE);
                } 
                else { 
                    // --- MENU GAK TERPILIH (DI ATAS / DI BAWAH) ---
                    // Warna normal (Putih di atas Hitam)
                    // Posisinya digeser X-nya (+4) biar seakan-akan mundur/mengecil
                    screen_draw_bitmap(0, 30, yPos, icon, 10, 10, WHITE);
                    rootx_print_text_custom(46, yPos + 1, (char*)teks, WHITE, BLACK);
                }
            }
        }

        // --- 4. FOOTER ---
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_custom(2, 55, "< BACK", BLACK, WHITE);
        rootx_print_text_custom(85, 55, "[OK] GO", BLACK, WHITE);
    }



    lcdDrawFinish(&dev);
}




void tampilkanStationScanner() {
    lcdFillScreen(&dev, BLACK);
    
    char buf[64]; 

    if (scannerStateSta == 0) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_custom(2, 1, "STATION SCANNER", BLACK, WHITE);
        rootx_print_text_custom(30, 25, "Scan Clients?", WHITE, BLACK);
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_custom(2, 55, "< BACK", BLACK, WHITE);
        rootx_print_text_custom(95, 55, "YES >", BLACK, WHITE);
    }
    else if (scannerStateSta == 1) {
        rootx_print_text_custom(10, 20, "SNIFFING TARGET:", WHITE, BLACK);
        rootx_print_text_custom(10, 35, targetTerkunci.ssid, WHITE, BLACK); 
        if (scanStaDone) scannerStateSta = 2; 
    }
    else if (scannerStateSta == 2) {
        if (totalStation == 0) {
            lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
            rootx_print_text_custom(2, 1, "CLIENT LIST", BLACK, WHITE);
            rootx_print_text_custom(15, 25, "NO CLIENTS FOUND!", WHITE, BLACK);
            lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
            rootx_print_text_custom(2, 55, "< RESCAN", BLACK, WHITE);
        } else {
            lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
            snprintf(buf, sizeof(buf), "CLIENTS: %d", totalStation);
            rootx_print_text_custom(2, 1, buf, BLACK, WHITE);
            
            for (int i = 0; i < 3; i++) {
                int itemIdx = scrollPosScanner + i;
                if (itemIdx < totalStation) {
                    int yPos = 14 + (i * 13);
                    int txtCol = (i == cursorInScanSta) ? BLACK : WHITE;
                    int bgCol = (i == cursorInScanSta) ? WHITE : BLACK;
                    
                    if (i == cursorInScanSta) lcdDrawFillRect(&dev, 0, yPos - 1, 128, yPos + 11, WHITE);

                    snprintf(buf, sizeof(buf), "%d.", listStation[itemIdx].id);
                    rootx_print_text_custom(1, yPos + 1, buf, txtCol, bgCol);

                    snprintf(buf, sizeof(buf), "%02X:%02X..%02X:%02X", 
                             listStation[itemIdx].mac[0], listStation[itemIdx].mac[1],
                             listStation[itemIdx].mac[4], listStation[itemIdx].mac[5]);
                   
                  
                    rootx_print_text_custom(18, yPos + 1, buf, txtCol, bgCol);

                    snprintf(buf, sizeof(buf), "%ddBm", listStation[itemIdx].rssi);
                    rootx_print_text_custom(90, yPos + 1, buf, txtCol, bgCol);
                }
            }
            lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
            rootx_print_text_custom(2, 55, "< BACK", BLACK, WHITE);
            rootx_print_text_custom(53, 55, "[OK] ACTION", BLACK, WHITE);
        }
    }
    else if (scannerStateSta == 3) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_custom(22, 1, "TARGET DETAILS", BLACK, WHITE);

        snprintf(buf, sizeof(buf), "MC:%02X:%02X:%02X:%02X:%02X:%02X", 
                 targetSta.mac[0], targetSta.mac[1], targetSta.mac[2],
                 targetSta.mac[3], targetSta.mac[4], targetSta.mac[5]);
                 
        
        rootx_print_text_custom(5, 17, buf, WHITE, BLACK);
        

        snprintf(buf, sizeof(buf), "RSSI: %d dBm", targetSta.rssi);
        rootx_print_text_custom(5, 25, buf, WHITE, BLACK);

        snprintf(buf, sizeof(buf), "PACKETS: %d", targetSta.paket_count);
        rootx_print_text_custom(5, 35, buf, WHITE, BLACK);

        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_custom(2, 55, "< BACK", BLACK, WHITE);
    } 
    
        else if (scannerStateSta == 4) { // Atau scannerStateSta == 4, sesuaikan aja
        // --- 1. HEADER ---
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_custom(42, 1, "ACTIONS", BLACK, WHITE);

        // --- 2. BLOK PUTIH STATIS DI TENGAH ---
        // Y mulai dari 24, tinggi 16 piksel (Bener-bener pas di center OLED 128x64)
        lcdDrawFillRect(&dev, 0, 24, 128, 40, WHITE);

        // --- 3. LOGIKA ROLLING MENU ---
        for(int i = 0; i < 2; i++) {
            const char* teks;
            const unsigned char* icon;
            
            // Set Teks dan Icon
              if(i == 0)      { teks = "KICK CLIENT";  icon = iconSmall_skull; }
            else            { teks = "DETAILS"; icon = iconSmall_info;  }

            // Hitung jarak index ini dari kursor yang lagi aktif
            int diff = i - contextCursor; 
            
            // Jarak antar baris 15 piksel. Posisi tengah (diff=0) ada di Y=27
            int yPos = 27 + (diff * 15); 

            // Cuma gambar menu yang posisinya ada di area pandang (antara header & footer)
            if (yPos > 10 && yPos < 45) {
                
                if (diff == 0) { 
                    // --- MENU TERPILIH (DI TENGAH BLOK PUTIH) ---
                    // Warna dibalik (Hitam di atas Putih)
                    screen_draw_bitmap(0, 26, yPos - 1, icon, 10, 10, BLACK); 
                    rootx_print_text_custom(42, yPos, (char*)teks, BLACK, WHITE);
                    
                    // Tambahan efek panah biar kelihatan lebih "Gede/Lebar"
                    rootx_print_text_custom(10, yPos, ">", BLACK, WHITE);
                    rootx_print_text_custom(110, yPos, "<", BLACK, WHITE);
                } 
                else { 
                    // --- MENU GAK TERPILIH (DI ATAS / DI BAWAH) ---
                    // Warna normal (Putih di atas Hitam)
                    // Posisinya digeser X-nya (+4) biar seakan-akan mundur/mengecil
                    screen_draw_bitmap(0, 30, yPos, icon, 10, 10, WHITE);
                    rootx_print_text_custom(46, yPos + 1, (char*)teks, WHITE, BLACK);
                }
            }
        }

        // --- 4. FOOTER ---
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_custom(2, 55, "< BACK", BLACK, WHITE);
        rootx_print_text_custom(85, 55, "[OK] GO", BLACK, WHITE);
    }
    
   
        // --- 4. FOOTER (Tetap) ---

    lcdDrawFinish(&dev);
}





void tampilkandeauthsta() {
    lcdFillScreen(&dev, BLACK);
    
    char buf[64];
    
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_custom(2, 1, "ATTACKING STATION...", BLACK, WHITE);
        
        snprintf(buf, sizeof(buf), "Target:%02X:%02X:%02X:%02X:%02X:%02X", 
                 targetSta.mac[0], targetSta.mac[1], targetSta.mac[2],
                 targetSta.mac[3], targetSta.mac[4], targetSta.mac[5]);

        rootx_print_text_custom(0, 20, buf, WHITE, BLACK);
        snprintf(buf, sizeof(buf), "Ch: %d", targetTerkunci.channel);
        rootx_print_text_custom(0, 30, buf, WHITE, BLACK);
        
        int animasiProgress = (millis() / 30) % 100; 

        drawLoadingBar(14, 42, 100, 8, animasiProgress);
        
        
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_custom(2, 55, "< STOP ATTACK", BLACK, WHITE);
    
    lcdDrawFinish(&dev);
}

void tampilkanDeauthScreen() {
    lcdFillScreen(&dev, BLACK);
    
    char buf[64];
    
    if (deauthState == 0) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_custom(26, 1, "DEAUTH ATTACK", BLACK, WHITE);
        
        rootx_print_text_custom(10, 25, "Attack Target?", WHITE, BLACK);
        
        char shortSsid[16];
        strncpy(shortSsid, targetTerkunci.ssid, 15);
        shortSsid[15] = '\0';
        rootx_print_text_custom(10, 35, shortSsid, WHITE, BLACK);

        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_custom(2, 55, "< NO", BLACK, WHITE);
        rootx_print_text_custom(95, 55, "YES >", BLACK, WHITE);
    } 
    else if (deauthState == 1) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_custom(2, 1, "ATTACKING...", BLACK, WHITE);

        snprintf(buf, sizeof(buf), "Target: %s", targetTerkunci.ssid);
        rootx_print_text_custom(0, 20, buf, WHITE, BLACK);
        snprintf(buf, sizeof(buf), "Ch: %d", targetTerkunci.channel);
        rootx_print_text_custom(0, 30, buf, WHITE, BLACK);
        
        
        int animasiProgress = (millis() / 30) % 100; 

        drawLoadingBar(14, 42, 100, 8, animasiProgress);
        
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_custom(2, 55, "< STOP ATTACK", BLACK, WHITE);
    }
    lcdDrawFinish(&dev);
}

void tampilkanBrightness() {
    lcdFillScreen(&dev, BLACK);
    
    char buf[16];

    lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
    rootx_print_text_custom(35, 1, "BRIGHTNESS", BLACK, WHITE);

    lcdDrawRect(&dev, 14, 28, 114, 40, WHITE); 
    
    int barWidth = map(brightnessValue, 0, 255, 0, 96);
    lcdDrawFillRect(&dev, 16, 30, 16 + barWidth, 38, WHITE);

    int persen = map(brightnessValue, 0, 255, 0, 100);
    snprintf(buf, sizeof(buf), "%d%%", persen);
    rootx_print_text_custom(55, 45, buf, WHITE, BLACK);

    lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
    rootx_print_text_custom(5, 55, "[<] BACK", BLACK, WHITE);
    rootx_print_text_custom(75, 55, "[UP/DN] SET", BLACK, WHITE);

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
        rootx_print_text_custom(2, 1, (char*)judul, BLACK, WHITE);
        
        rootx_print_text_custom(10, 25, (char*)subTeks, WHITE, BLACK);

        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_custom(2, 55, "< NO", BLACK, WHITE);
        rootx_print_text_custom(95, 55, "YES >", BLACK, WHITE);
    } 
    else if (spamState == 1) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_custom(2, 1, "RUNNING...", BLACK, WHITE);

        snprintf(buf, sizeof(buf), "Mode: %s", subTeks);
        rootx_print_text_custom(0, 25, buf, WHITE, BLACK);
        
        
        int animasiProgress = (millis() / 30) % 100; 
        drawLoadingBar(14, 42, 100, 8, animasiProgress);
        
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_custom(2, 55, "< STOP", BLACK, WHITE);
    }
    lcdDrawFinish(&dev);
}





void renderDinoGame() {
    if (dinoHighScore == -1) dinoHighScore = baca_highscore_dino();
    lcdFillScreen(&dev, BLACK);
    

    // --- LOGIC SIANG MALAM ---
    int cycle = dinoScore % 1000;
    bool isNight = (cycle >= 0 && cycle < 300 && dinoScore >= 1000); 
    if(isNight) lcdInversionOn(&dev); else lcdInversionOff(&dev);

    // --- UI: SKOR ---
    char scoreBuf[32];
    snprintf(scoreBuf, sizeof(scoreBuf), "HI %05d  %05d", dinoHighScore, dinoScore);
    rootx_print_text_custom(35, 0, scoreBuf, WHITE, BLACK);

    // Variabel Musuh pake static biar enteng
    static int obs1X = 130; 
    static int obs2X = 250; // Musuh kedua nunggu jauh biar gak barengan pas awal
    static int obs1Type = 0, obs2Type = 0;
    static int obs1Y = 44, obs2Y = 44;

    if (dinoState == 0) { // SEDANG MAIN
        
        // ==========================================
        // OBAT BUG 1 & 2: AUTO RESET PAS RESTART
        // ==========================================
        if (dinoScore == 0 && gameSpeed <= 4.1) {
            obs1X = 130; 
            obs2X = 250; // Jarak awal antara musuh 1 dan 2
        }

        rawScore += (gameSpeed * 0.15);
        dinoScore = (int)rawScore;
        gameSpeed = 4.0 + (dinoScore / 500.0);
        if (gameSpeed > 8.5) gameSpeed = 8.5;

        // Physics Loncat
        dinoY += dinoVy;
        if (isJumping) dinoVy += 1.6; 
        if (dinoY >= 36) { dinoY = 36; isJumping = false; dinoVy = 0; }

        // --- BACKGROUND ---
        skyX -= 0.5;
        if (skyX < -20) skyX = 128;
        if (isNight) screen_draw_bitmap(0, (int)skyX, 8, bulan_16, 16, 16, WHITE);
        else screen_draw_bitmap(0, (int)skyX, 8, matahari_16, 16, 16, WHITE);

        // ==========================================
        // OBAT BUG 3: ANTI TELEPORT (SPAWN SELALU DARI KANAN)
        // ==========================================
        
        // Gerakin Musuh
        obs1X -= (int)gameSpeed;
        obs2X -= (int)gameSpeed;

               // Reset Musuh 1
        if (obs1X < -24) {
            obs1X = obs2X + 80 + (rand() % 60);
            // KUNCI ANTI TELEPORT: Kalau hasil rumusnya malah di dalem layar, PAKSA ke ujung!
            if (obs1X < 130) obs1X = 130 + (rand() % 40); 
            
            obs1Type = rand() % 3; 
            if (obs1Type == 2) { int h[] = {20, 32}; obs1Y = h[rand()%2]; } 
            else { obs1Y = (obs1Type == 0) ? 44 : 38; }
        }

        // Reset Musuh 2
        if (obs2X < -24) {
            obs2X = obs1X + 80 + (rand() % 60);
            if (obs2X < 130) obs2X = 130 + (rand() % 40);
            
            obs2Type = rand() % 3; 
            if (obs2Type == 2) { int h[] = {20, 32}; obs2Y = h[rand()%2]; } 
            else { obs2Y = (obs2Type == 0) ? 44 : 38; }
        }


        // --- DRAW GROUND ---
        lcdDrawLine(&dev, 0, 60, 128, 60, WHITE);
        for (int i = 0; i < 128; i += 16) {
            int scrollX = (i - ((int)rawScore % 128));
            if (scrollX < 0) scrollX += 128;
            lcdDrawPixel(&dev, scrollX, 62, WHITE);
            lcdDrawPixel(&dev, (scrollX + 7) % 128, 61, WHITE);
            if (i % 32 == 0) lcdDrawLine(&dev, scrollX, 61, scrollX + 4, 61, WHITE);
        }

        // --- DRAW DINO ---
        const unsigned char* dinoFrame = (dinoY < 36) ? dino_lari1 : (((millis()/100)%2==0) ? dino_lari1 : dino_lari2);
        screen_draw_bitmap(0, 15, (int)dinoY, dinoFrame, 24, 24, WHITE);

        // --- DRAW SEMUA MUSUH ---
        const unsigned char* pteroFrame = ((millis() / 200) % 2 == 0) ? ptero_up : ptero_down;
        
        // Musuh 1
        if (obs1Type == 0) screen_draw_bitmap(0, obs1X, 44, kaktus_16, 16, 16, WHITE);
        else if (obs1Type == 1) screen_draw_bitmap(0, obs1X, 38, kaktus_besar, 24, 24, WHITE);
        else if (obs1Type == 2) screen_draw_bitmap(0, obs1X, obs1Y, pteroFrame, 16, 16, WHITE);

        // Musuh 2
        if (obs2Type == 0) screen_draw_bitmap(0, obs2X, 44, kaktus_16, 16, 16, WHITE);
        else if (obs2Type == 1) screen_draw_bitmap(0, obs2X, 38, kaktus_besar, 24, 24, WHITE);
        else if (obs2Type == 2) screen_draw_bitmap(0, obs2X, obs2Y, pteroFrame, 16, 16, WHITE);


        // ==========================================
        // COLLISION DETECTION (CEK DUA MUSUH)
        // ==========================================
        
        // Cek Musuh 1
        if (obs1X > 10 && obs1X < 30) { 
            int d_top = dinoY + 4; int d_bottom = dinoY + 20;
            int o_top = (obs1Type == 2) ? obs1Y + 4 : ((obs1Type == 0) ? 44 : 38);
            int o_bottom = (obs1Type == 2) ? obs1Y + 12 : 60;
            if (!(d_bottom < o_top || d_top > o_bottom)) { 
                dinoState = 1; lcdInversionOff(&dev); 
                if (dinoScore > dinoHighScore) { dinoHighScore = dinoScore; simpan_highscore_dino(dinoHighScore); }
            }
        }
        // Cek Musuh 2
        if (obs2X > 10 && obs2X < 30) { 
            int d_top = dinoY + 4; int d_bottom = dinoY + 20;
            int o_top = (obs2Type == 2) ? obs2Y + 4 : ((obs2Type == 0) ? 44 : 38);
            int o_bottom = (obs2Type == 2) ? obs2Y + 12 : 60;
            if (!(d_bottom < o_top || d_top > o_bottom)) { 
                dinoState = 1; lcdInversionOff(&dev); 
                if (dinoScore > dinoHighScore) { dinoHighScore = dinoScore; simpan_highscore_dino(dinoHighScore); }
            }
        }

    } 
    else { // GAME OVER
        rootx_print_text_custom(20, 25, "G A M E  O V E R", WHITE, BLACK);
        rootx_print_text_custom(28, 50, "[OK] RESTART", WHITE, BLACK);
    }
    lcdDrawFinish(&dev);
}




void tampilkanEvilTwinScreen() {
    lcdFillScreen(&dev, BLACK);

    
    
    if (evilTwinState == 0) {
    lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_custom(2, 1, "EVIL TWIN", BLACK, WHITE);
        
        rootx_print_text_custom(10, 25, "Start Evil Twin?", WHITE, BLACK);

     
        
        rootx_print_text_custom(10, 35, targetTerkunci.ssid, WHITE, BLACK);
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_custom(2, 55, "< NO", BLACK, WHITE);
        rootx_print_text_custom(95, 55, "YES >", BLACK, WHITE);
    } 
    else if (evilTwinState == 1) {
        rootx_print_text_custom(15, 20, "WAITING FOR DATA...", WHITE, BLACK);
        int bounce = (millis() / 200) % 5;
        rootx_print_text_custom(50, 40 + bounce, "...", WHITE, BLACK);
        rootx_print_text_custom(2, 55, "< STOP", WHITE, BLACK);
    }
    else if (evilTwinState == 2) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_custom(20, 1, "PW EXPLOITED!", BLACK, WHITE);
        rootx_print_text_custom(5, 25, "Target:", WHITE, BLACK);
        rootx_print_text_custom(50, 25, targetTerkunci.ssid, WHITE, BLACK);
        rootx_print_text_custom(5, 40, "Pass  :", WHITE, BLACK);
        rootx_print_text_custom(50, 40, stolenPassword, WHITE, BLACK);
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
        rootx_print_text_custom(2, 1, "SAVED REMOTE", BLACK, WHITE);

        if (totalSavedRemotes == 0) {
            rootx_print_text_custom(10, 25, "Data Kosong!", WHITE, BLACK);
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
                rootx_print_text_custom(0, 16 + (i * 12), buf, WHITE, BLACK);
            }
        }

        // --- FOOTER (BLOK PUTIH) ---
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_custom(2, 55, "< NO", BLACK, WHITE);
        rootx_print_text_custom(95, 55, "OK >", BLACK, WHITE);
    } 
    else if (currentIRSavedState == IR_SAVED_STATE_ACTION) {
        char buf[32];
        
        snprintf(buf, sizeof(buf), " ACTION: %s ", listSavedRemotes[savedRemoteIndex].nama);
        // Header
        
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_custom(2, 1, buf, BLACK, WHITE);

        // Menu Transmit / Hapus
        if (actionMenuIndex == 0) {
            rootx_print_text_custom(15, 20, "> 1. TRANSMIT", WHITE, BLACK);
            rootx_print_text_custom(15, 35, "  2. HAPUS", WHITE, BLACK);
        } else {
            rootx_print_text_custom(15, 20, "  1. TRANSMIT", WHITE, BLACK);
            rootx_print_text_custom(15, 35, "> 2. HAPUS", WHITE, BLACK);
        }
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_custom(2, 55, "< NO", BLACK, WHITE);
    } 
    else if (currentIRSavedState == IR_SAVED_STATE_SENDING) {
        // Layar Polos, Tulisan di Tengah!
        rootx_print_text_custom(25, 25, "IR SEND!", WHITE, BLACK);
    }

    // Refresh layar ID 0, dan force update (true)
    lcdDrawFinish(&dev); 
}

void tampilkanMenuIR() {
    lcdFillScreen(&dev, BLACK);

    char buf[32];

    if (currentIRState == IR_STATE_CONFIRM) {
        lcdDrawFillRect(&dev, 0, 0, 128, 10, WHITE);
        rootx_print_text_custom(2, 1, "SNIFF IR SIGNAL", BLACK, WHITE);
        rootx_print_text_custom(10, 25, "Start Sniff??", WHITE, BLACK);
        
        lcdDrawFillRect(&dev, 0, 54, 128, 64, WHITE);
        rootx_print_text_custom(2, 55, "< NO", BLACK, WHITE);
        rootx_print_text_custom(95, 55, "OK >", BLACK, WHITE);
    
    } 
    else if (currentIRState == IR_STATE_WAITING) {
        rootx_print_text_custom(5, 20, "Menunggu", WHITE, BLACK);
        rootx_print_text_custom(5, 40, "sinyal masuk...", WHITE, BLACK);
    } 
    else if (currentIRState == IR_STATE_RESULT) {
        rootx_print_text_custom(0, 0, "== IR RESULT ==", WHITE, BLACK);
        rootx_print_text_custom(0, 16, "Type: RAW CLONER", WHITE, BLACK);
        
        snprintf(buf, sizeof(buf), "Pulses: %d", last_ir_data.num_pulses);
        rootx_print_text_custom(0, 30, buf, WHITE, BLACK);
        
        rootx_print_text_custom(0, 56, "> SD Card Saved <", WHITE, BLACK);
    }
    lcdDrawFinish(&dev);
}

void renderSnakeGame() {
    // Kalau lu punya fungsi simpan/baca sd card khusus snake, taruh sini.
    // Sementara kita set manual kalo belum ada.
    
   if (snakeHighScore == -1) snakeHighScore = baca_highscore_snake();
    lcdFillScreen(&dev, BLACK);

    // Array buat nyimpen koordinat badan Ular (Maksimal panjang 100)
    static int snakeX[100];
    static int snakeY[100];
    static int snakeLen = 3; // Panjang awal
    
    static int appleX = 20;
    static int appleY = 8;
    
    static uint32_t lastMoveTime = 0;
    

    // Kecepatan Ular (Makin kecil makin ngebut, misal 100ms per gerak)
    int moveInterval = 120; 

    // --- 1. RESET / INIT PAS BARU MULAI ---
        // --- 1. RESET / INIT PAS BARU MULAI ---

    if (!isSnakeInitialized) {
        snakeX[0] = 10; snakeY[0] = 8; // Kepala
        snakeX[1] = 9;  snakeY[1] = 8; // Badan 1
        snakeX[2] = 8;  snakeY[2] = 8; // Ekor
        snakeLen = 3;
        snakeDir = 0; // Awal mulai gerak ke Kanan
        snakeScore = 0;
        snakeState = 0;
        
        // Spawn apel pertama (Area aman: X: 0-31, Y: 3-15)
        appleX = rand() % 32;
        appleY = 3 + (rand() % 13);
        
        isSnakeInitialized = true;
    }

    lcdFillScreen(&dev, BLACK);

    if (snakeState == 0) { // SEDANG MAIN
        
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Logika pergerakan (Jalan tiap beberapa milidetik aja, gak setiap frame)
        if (now - lastMoveTime > moveInterval) {
            lastMoveTime = now;

            // A. Geser koordinat badan ngikutin yg depannya (dari ekor ke leher)
            for (int i = snakeLen - 1; i > 0; i--) {
                snakeX[i] = snakeX[i - 1];
                snakeY[i] = snakeY[i - 1];
            }

            // B. Geser Kepala sesuai Arah (snakeDir)
            if (snakeDir == 0) snakeX[0]++;      // Kanan
            else if (snakeDir == 1) snakeY[0]++; // Bawah
            else if (snakeDir == 2) snakeX[0]--; // Kiri
            else if (snakeDir == 3) snakeY[0]--; // Atas

            // C. CEK TABRAKAN TEMBOK (Kiri, Kanan, Atas, Bawah)
            // Area Y < 3 itu buat Skor, jadi anggep tembok atas
            if (snakeX[0] < 0 || snakeX[0] >= 32 || snakeY[0] < 3 || snakeY[0] >= 16) {
                snakeState = 1; // Mati nabrak tembok!
            }

            // D. CEK TABRAKAN BADAN SENDIRI
            for (int i = 1; i < snakeLen; i++) {
                if (snakeX[0] == snakeX[i] && snakeY[0] == snakeY[i]) {
                    snakeState = 1; // Mati gigit ekor!
                }
            }

            // E. MAKAN APEL
            if (snakeX[0] == appleX && snakeY[0] == appleY) {
                if (snakeLen < 100) snakeLen++; // Tambah panjang
                snakeScore += 10;               // Tambah skor
                
                // Spawn apel baru (Random posisi)
                appleX = rand() % 32;
                appleY = 3 + (rand() % 13);
                
                // Opsional: Makin panjang makin ngebut?
                moveInterval -= 2; 
            }
        }

        // --- MENGGAMBAR GAME (RENDER) ---
        
        // 1. Gambar Batas Atas (Garis bawah Skor)
        lcdDrawLine(&dev, 0, 10, 128, 10, WHITE);
        
        // 2. Teks Skor
        char scoreBuf[32];
        snprintf(scoreBuf, sizeof(scoreBuf), "HI:%04d  SCR:%04d", snakeHighScore, snakeScore);
        rootx_print_text_custom(0, 0, scoreBuf, WHITE, BLACK);

        // 3. Gambar Apel (Bikin titik aja/kotak kecil 4x4)
        lcdDrawRect(&dev, appleX * 4, appleY * 4, (appleX * 4) + 4, (appleY * 4) + 4, WHITE);
        
        // Kasih efek "bintik" di tengah apel biar beda sama badan ular
        lcdDrawPixel(&dev, (appleX * 4) + 1, (appleY * 4) + 1, BLACK); 

        // 4. Gambar Badan Ular
        for (int i = 0; i < snakeLen; i++) {
            if (i == 0) {
                // Kepala: Kotak tebel (Full)
                // x1, y1, x2, y2 (x2 = x1+4, y2 = y1+4)
                lcdDrawFillRect(&dev, snakeX[i] * 4, snakeY[i] * 4, (snakeX[i] * 4) + 4, (snakeY[i] * 4) + 4, WHITE);
            }
 else {
                // Badan: Kotak bolong / bergaris biar keliatan ruasnya
                lcdDrawRect(&dev, snakeX[i] * 4, snakeY[i] * 4, (snakeX[i] * 4) + 3, (snakeY[i] * 4) + 3, WHITE);
            }
        }

    }
           
    else { // GAME OVER
        rootx_print_text_custom(20, 25, "G A M E  O V E R", WHITE, BLACK);
        
        // Cukup tampilin instruksi aja, logikanya udah diatur di input_system.c
        rootx_print_text_custom(15, 45, "[OK] RESTART", WHITE, BLACK);
        rootx_print_text_custom(15, 55, "[<] BACK", WHITE, BLACK);

        if (snakeScore > snakeHighScore) {
            snakeHighScore = snakeScore;
            simpan_highscore_snake(snakeHighScore); 
        }
    

    


        // Kalau lu pencet tombol OK di menu lu (misal ngerubah snakeState jadi 0 lagi),
        // Jangan lupa bikin `isInitialized = false;` di dalem logika tombol OK lu.
    }
    
    lcdDrawFinish(&dev);
}

// ==========================================
// MESIN TETRIS VERTIKAL (Miring 90 Derajat)
// ==========================================


// Variabel statis untuk mesin gamenya
static uint8_t tetris_grid[25][10]; // 20 baris (X), 10 kolom (Y)
static int t_shape, t_rot, t_x, t_y; 
static uint32_t lastFallTime = 0;

// Data 7 Balok Tetris (Bitmask 16-bit biar enteng)
static const uint16_t tetris_shapes[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444}, // I (Lurus)
    {0x8E00, 0x6440, 0x0E20, 0x4C40}, // J
    {0x2E00, 0x4460, 0x0E80, 0xC440}, // L
    {0xCC00, 0xCC00, 0xCC00, 0xCC00}, // O (Kotak)
    {0x6C00, 0x4620, 0x06C0, 0x8C40}, // S
    {0x4E00, 0x4640, 0x0E40, 0x4C40}, // T
    {0xC600, 0x2640, 0x0C60, 0x4C80}  // Z
};

// Fungsi Cek Tabrakan
bool check_tetris_col(int shape, int rot, int px, int py) {
    uint16_t p = tetris_shapes[shape][rot];
    for (int i=0; i<16; i++) {
        if (p & (0x8000 >> i)) {
            int grid_x = px + (i % 4);
            int grid_y = py + (i / 4);
            if (grid_x < 0 || grid_x >= 10 || grid_y >= 20) return true; // Nabrak tembok
            if (grid_y >= 0 && tetris_grid[grid_y][grid_x]) return true; // Nabrak balok lain
        }
    }
    return false;
}

// Handler Input biar input_system lu bersih!
void handleTetrisInput(int btn) {
    if (tetrisState == 0) {
        if (btn == BTN_UP) { if (!check_tetris_col(t_shape, t_rot, t_x - 1, t_y)) t_x--; }
        else if (btn == BTN_DOWN) { if (!check_tetris_col(t_shape, t_rot, t_x + 1, t_y)) t_x++; }
        else if (btn == BTN_RIGHT) { if (!check_tetris_col(t_shape, t_rot, t_x, t_y + 1)) t_y++; }
        else if (btn == BTN_OK) { 
            int nr = (t_rot + 1) % 4;
            if (!check_tetris_col(t_shape, nr, t_x, t_y)) t_rot = nr;
        }
        else if (btn == BTN_LEFT) { appMode = 0; isTetrisInitialized = false; }
    } else {
        if (btn == BTN_OK) { isTetrisInitialized = false; tetrisState = 0; }
        else if (btn == BTN_LEFT) { appMode = 0; }
    }
}

void renderTetrisGame() {
    if (tetrisHighScore == -1) tetrisHighScore = baca_highscore_tetris();
    lcdFillScreen(&dev, BLACK);


    if (!isTetrisInitialized) {
        memset(tetris_grid, 0, sizeof(tetris_grid));
        t_shape = rand() % 7; t_rot = 0; t_x = 3; t_y = 0;
        tetrisScore = 0; tetrisState = 0;
        isTetrisInitialized = true;
    }

    lcdFillScreen(&dev, BLACK);
    int fallSpeed = 500 - (tetrisScore * 2); // Makin gede skor, makin ngebut
    if (fallSpeed < 100) fallSpeed = 100;

    if (tetrisState == 0) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Logika Gravitasi
        if (now - lastFallTime > fallSpeed) {
            lastFallTime = now;
            if (!check_tetris_col(t_shape, t_rot, t_x, t_y + 1)) {
                t_y++;
            } else {
                // Kunci balok yang nyentuh tanah
                uint16_t p = tetris_shapes[t_shape][t_rot];
                for (int i=0; i<16; i++) {
                    if (p & (0x8000 >> i)) {
                        int x = t_x + (i % 4); int y = t_y + (i / 4);
                        if (y >= 0 && y < 20 && x >= 0 && x < 10) tetris_grid[y][x] = 1;
                    }
                }
                
                // Cek Baris Penuh (Line Clear)
                int linesCleared = 0;
                for (int y = 19; y >= 0; y--) {
                    bool full = true;
                    for (int x = 0; x < 10; x++) if (!tetris_grid[y][x]) full = false;
                    
                    if (full) {
                        linesCleared++;
                        for (int yy = y; yy > 0; yy--) {
                            for (int x = 0; x < 10; x++) tetris_grid[yy][x] = tetris_grid[yy-1][x];
                        }
                        y++; // Cek ulang baris ini (karena atasnya turun)
                    }
                }
                tetrisScore += (linesCleared * linesCleared) * 10; // Bonus skor berlipat

                // Spawn Balok Baru
                t_shape = rand() % 7; t_rot = 0; t_x = 3; t_y = 0;
                if (check_tetris_col(t_shape, t_rot, t_x, t_y)) tetrisState = 1; // Game Over
            }
        }

             // --- RENDER VISUAL TETRIS VERTIKAL ---
        // Grid layar kita geser ke Y=14 biar ada ruang kosong buat teks di Y=0 sampai 12
        lcdDrawLine(&dev, 0, 13, 126, 13, WHITE);  // Border Kiri
        lcdDrawLine(&dev, 0, 63, 126, 63, WHITE);  // Border Kanan
        lcdDrawLine(&dev, 126, 13, 126, 63, WHITE); // Border Bawah (Tanah)

        for (int y = 0; y < 20; y++) {
            for (int x = 0; x < 10; x++) {
                if (tetris_grid[y][x]) {
                    lcdDrawRect(&dev, y * 5, 14 + (x * 5), (y * 5) + 5, 14 + (x * 5) + 5, WHITE); 
                }
            }
        }

        uint16_t p = tetris_shapes[t_shape][t_rot];
        for (int i=0; i<16; i++) {
            if (p & (0x8000 >> i)) {
                int x = t_x + (i % 4); int y = t_y + (i / 4);
                lcdDrawRect(&dev, y * 5, 14 + (x * 5), (y * 5) + 5, 14 + (x * 5) + 5, WHITE);
            }
        }

    } else { // GAME OVER
        rootx_print_text_custom(20, 25, "GAME OVER", WHITE, BLACK);
        
                if (tetrisScore > tetrisHighScore) {
            tetrisHighScore = tetrisScore;
            simpan_highscore_tetris(tetrisHighScore); 
        }
    }

    // --- TEKS SKOR (Horizontal Bawaan OLED) ---
    // Posisinya di Y=2 (Ruang kosong di sebelah kiri arena kalau dipegang vertikal)
    char sc[32]; 
    snprintf(sc, sizeof(sc), "HI:%d  SCR:%d", tetrisHighScore, tetrisScore);
    rootx_print_text_custom(0, 2, sc, WHITE, BLACK);

    

    
    lcdDrawFinish(&dev);
}

void renderAboutScreen() {
    lcdFillScreen(&dev, BLACK);


    // Bikin border kotak di pinggir layar biar UI-nya rapi
    lcdDrawRect(&dev, 0, 0, 128, 64, WHITE);
    lcdDrawRect(&dev, 2, 2, 126, 62, WHITE); // Border dalem (double line)

    // Judul
    rootx_print_text_custom(32, 8, "ROOTX OS", WHITE, BLACK);
    lcdDrawLine(&dev, 25, 18, 103, 18, WHITE); // Garis bawah judul

    // Info Alat (Lu bisa ganti teksnya sesuka lu Cok!)
    rootx_print_text_custom(10, 25, "Ver : 1.0.0", WHITE, BLACK);
    rootx_print_text_custom(10, 35, "Core: ESP32-S3", WHITE, BLACK);
    rootx_print_text_custom(10, 45, "By  : Andyy", WHITE, BLACK); // Ganti pake nama lu!

    // Tombol Keluar
    rootx_print_text_custom(90, 45, "[<]", WHITE, BLACK); // Logo Kiri buat exit

    lcdDrawFinish(&dev);
}

void renderRebootScreen() {
    lcdFillScreen(&dev, BLACK);


    // Border Frame biar keren
    lcdDrawRect(&dev, 5, 5, 123, 59, WHITE);

    // Teks Pertanyaan
    rootx_print_text_custom(20, 20, "Reboot sekarang?", WHITE, BLACK);

    // Petunjuk Tombol
    
    rootx_print_text_custom(2, 55, "< NO", WHITE, BLACK);
    rootx_print_text_custom(95, 55, "OK >", WHITE, BLACK);

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
    rootx_print_text_custom(34, 2, "SD MANAGER", BLACK, WHITE);

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
            rootx_print_text_custom(5, 20, "VFS Error!", WHITE, BLACK);
            rootx_print_text_custom(5, 30, "Gagal Baca Size", WHITE, BLACK);
        } else {
            // Tampilan Size
            snprintf(buf, sizeof(buf), "Size: %.0f MB", total_mb);
            rootx_print_text_custom(5, 15, buf, WHITE, BLACK);
            // Tampilan Free
            snprintf(buf, sizeof(buf), "Free: %.0f MB", free_mb);
            rootx_print_text_custom(5, 25, buf, WHITE, BLACK);
        }
        
        // --- 4. PROGRESS BAR ---
        snprintf(buf, sizeof(buf), "%d%%", percent);
        rootx_print_text_custom(100, 25, buf, WHITE, BLACK);
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

        rootx_print_text_custom(5, 54, "<", WHITE, BLACK);
        rootx_print_text_custom(startX, 54, menuNames[sdActionIdx], WHITE, BLACK);
        rootx_print_text_custom(117, 54, ">", WHITE, BLACK);
    }
   
    else if (sdState == 1) { // KONFIRMASI FORMAT
        rootx_print_text_custom(15, 20, "FORMAT SD CARD?", WHITE, BLACK);
        rootx_print_text_custom(25, 32, "ALL DATA LOST!", WHITE, BLACK);
        rootx_print_text_custom(5, 50, "[<-] NO   [OK] YES", WHITE, BLACK);
    } 
    else if (sdState == 2) { // FORMATTING
        rootx_print_text_custom(25, 30, "FORMATTING...", WHITE, BLACK);
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
    rootx_print_text_custom(38, 2, "SD FILES", BLACK, WHITE);

    if (sdTotalFiles == 0) {
        rootx_print_text_custom(15, 30, "NO FILES FOUND!", WHITE, BLACK);
        rootx_print_text_custom(30, 50, "[<-] BACK", WHITE, BLACK);
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
                    rootx_print_text_custom(0, yPos, ">", WHITE, BLACK);
                    // Kotak Invert Background kursor
                    lcdDrawRect(&dev, 8, yPos - 1, 128, yPos + 8, WHITE);
                    rootx_print_text_custom(10, yPos, sdFileNames[fileIdx], BLACK, WHITE);
                } else {
                    // File biasa
                    rootx_print_text_custom(10, yPos, sdFileNames[fileIdx], WHITE, BLACK);
                }
            }
            

            // Ganti dari "[OK] DEL" jadi "[OK] SEL/DEL" (Select / Delete)
char foot[32]; 
snprintf(foot, sizeof(foot), "%d/%d [OK] SEL/DEL", sdFileCursor + 1, sdTotalFiles);
rootx_print_text_custom(5, 56, foot, WHITE, BLACK);

            rootx_print_text_custom(5, 56, foot, WHITE, BLACK);
        } 
        else if (sdFileState == 1) { // MODE CONFIRM DELETE
            rootx_print_text_custom(20, 20, "DELETE FILE?", WHITE, BLACK);
            // Tulis nama file yg mau dihapus (Max 18 karakter biar muat di tengah)
            char truncName[20];
            snprintf(truncName, sizeof(truncName), "%.18s", sdFileNames[sdFileCursor]);
            rootx_print_text_custom(10, 32, truncName, WHITE, BLACK);
            
            rootx_print_text_custom(5, 50, "[<-] NO   [OK] YES", WHITE, BLACK);
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
    rootx_print_text_custom(37, 2, "TV-B-GONE", BLACK, WHITE);
    
    if (tvbgoneState == 0) { // MODE MENU PILIH REGION
        const char* menus[] = {"[ NA / ASIA ]", "[  EUROPE   ]", "[ ALL WORLD ]"};
        
        for(int i = 0; i < 3; i++) {
            int yPos = 20 + (i * 12);
            
            if (i == tvbgoneMenuIdx) {
                // Kursor Aktif
                rootx_print_text_custom(18, yPos, ">", WHITE, BLACK);
                lcdDrawRect(&dev, 26, yPos - 1, 104, yPos + 8, WHITE); // Highlight
                rootx_print_text_custom(28, yPos, menus[i], BLACK, WHITE);
            } else {
                rootx_print_text_custom(28, yPos, menus[i], WHITE, BLACK);
            }
        }
        
        // Footer Petunjuk Tombol
        rootx_print_text_custom(5, 55, "[<-] EXIT    [OK] START", WHITE, BLACK);

    } 
    else if (tvbgoneState == 1) { // MODE FIRING (LAGI NEMBAK)
        
        // Animasi Teks Kedip
        if ((xTaskGetTickCount() * portTICK_PERIOD_MS) / 500 % 2 == 0) {
            rootx_print_text_custom(18, 20, "TRANSMITTING...", WHITE, BLACK);
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
        rootx_print_text_custom(19, 50, counter, WHITE, BLACK);
    }
    
    lcdDrawFinish(&dev);
}
