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

// Forward declaration – defined later in this file
void drawIconScaled(int x, int y, int src_w, int src_h, int dst_w, int dst_h,
                    const uint8_t *icon, uint16_t color);

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

// ── Mockup-style UI Palette (ST7789 240×135) ──────────────────
#define UI_BG      rgb565(18,  18,  18)   // #121212
#define UI_HDR     rgb565(30,  30,  30)   // #1e1e1e
#define UI_SEP     rgb565(42,  42,  42)   // #2a2a2a
#define UI_ACT     rgb565(42,  58,  90)   // #2a3a5a  active item
#define UI_WRN     rgb565(58,  42,  42)   // #3a2a2a  warning pill
#define UI_BLU     rgb565(74,  124, 247)  // #4a7cf7  accent blue
#define UI_TXT     rgb565(238, 238, 238)  // #eee
#define UI_GRY     rgb565(170, 170, 170)  // #aaa
#define UI_MID     rgb565(136, 136, 136)  // #888
#define UI_DIM     rgb565(102, 102, 102)  // #666
#define UI_RED     rgb565(255, 107, 107)  // #ff6b6b

// Layout constants
#define SCR_W   240
#define SCR_H   135
#define HDR_H   14       // header row height
#define FTR_Y   121      // footer top y
#define CNT_Y   15       // content start y
#define CNT_CY  68       // content centre y  (15 + 106/2)
#define FW      7        // font7x13 pixel width per char

// Centre x for a string (font7x13)
static inline int _cx(const char *s) { return (SCR_W - (int)strlen(s)*FW)/2; }

// ── Header: title (bold) + right badge (regular) ──────────────
static void ui_hdr(const char *title, const char *badge) {
    lcdDrawFillRect(&dev, 0, 0, SCR_W, HDR_H, UI_HDR);
    lcdDrawLine(&dev, 0, HDR_H, SCR_W, HDR_H, UI_SEP);
    rootx_print_text_cb(6, 1, title, UI_TXT, UI_HDR);
    if (badge && badge[0]) {
        int bw = (int)strlen(badge)*FW + 6;
        int bx = SCR_W - bw - 6;
        lcdDrawFillRect(&dev, bx-2, 3, SCR_W-4, 11, UI_SEP);
        rootx_print_text_c(bx, 1, badge, UI_GRY, UI_SEP);
    }
}

// ── Footer: left (regular) + right (regular) ──────────────────
static void ui_ftr(const char *left, const char *right) {
    lcdDrawFillRect(&dev, 0, FTR_Y, SCR_W, SCR_H, UI_HDR);
    lcdDrawLine(&dev, 0, FTR_Y, SCR_W, FTR_Y, UI_SEP);
    if (left  && left[0])
        rootx_print_text_c(6, FTR_Y+1, left, UI_GRY, UI_HDR);
    if (right && right[0]) {
        int rx = SCR_W - (int)strlen(right)*FW - 6;
        rootx_print_text_c(rx, FTR_Y+1, right, UI_GRY, UI_HDR);
    }
}

// ── Progress bar (blue fill, dark border) ─────────────────────
static void ui_pbar(int x, int y, int w, int pct) {
    lcdDrawRect(&dev, x, y, x+w, y+6, UI_SEP);
    if (pct > 100) pct = 100;
    int fill = (w-2)*pct/100;
    if (fill > 0) lcdDrawFillRect(&dev, x+1, y+1, x+1+fill, y+5, UI_BLU);
}

// ── List row: icon (optional) + bold label + dim meta ─────────
static void ui_row(int y, const unsigned char *icon, const char *lbl,
                   const char *meta, bool act) {
    uint16_t bg = act ? UI_ACT : UI_BG;
    if (act) lcdDrawFillRect(&dev, 4, y, SCR_W-4, y+15, UI_ACT);
    int tx = 8;
    if (icon) {
        drawIconScaled(8, y+2, 18, 18, 10, 10, icon, act ? UI_TXT : UI_MID);
        tx = 22;
    }
    if (act) rootx_print_text_cb(tx, y+2, lbl, UI_TXT, UI_ACT);
    else     rootx_print_text_c (tx, y+2, lbl, UI_GRY, UI_BG);
    if (meta && meta[0]) {
        int mx = SCR_W - (int)strlen(meta)*FW - 8;
        rootx_print_text_c(mx, y+2, meta, UI_MID, bg);
    }
}

// ── Yes / No pill buttons (warn=true → red pill for danger) ───
static void ui_yes_no(int cy, bool warn) {
    rootx_print_text_c(68, cy, "Tidak", UI_MID, UI_BG);
    uint16_t pbg = warn ? UI_WRN : UI_ACT;
    uint16_t pfg = warn ? UI_RED : UI_TXT;
    lcdDrawFillRect(&dev, 150, cy-4, 196, cy+13, pbg);
    rootx_print_text_cb(160, cy, "Ya", pfg, pbg);
}

// ── Key-value row for detail screens ──────────────────────────
static void ui_kv(int y, const char *key, const char *val, uint16_t vcol) {
    rootx_print_text_cb(8,  y, key, UI_GRY, UI_BG);
    rootx_print_text_c(90, y, val, vcol,   UI_BG);
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
    lcdFillScreen(&dev, UI_BG);

    ui_hdr("Track RSSI", targetTerkunci.ssid);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", targetTerkunci.rssi);

    // Big RSSI number – medium font, centred
    int gw = (int)strlen(buf) * 12;
    int gx = (SCR_W - gw) / 2;
    rootx_print_text_sedang(gx, CNT_CY - 28, buf, UI_TXT, UI_BG);

    // "dBm" label
    rootx_print_text_c(_cx("dBm"), CNT_CY - 4, "dBm", UI_MID, UI_BG);

    // Floating WiFi icon
    int floatY = CNT_CY + 10 + (int)(sin(millis() / 300.0) * 3);
    drawIconScaled(SCR_W/2 - 12, floatY, 18, 18, 24, 24, ics_wifi, UI_BLU);

    // "Last updated" hint
    rootx_print_text_c(_cx("Terakhir diperbarui: 2 dtk lalu"),
                       CNT_CY + 42, "Terakhir diperbarui: 2 dtk lalu", UI_DIM, UI_BG);

    ui_ftr("< Kembali", NULL);
    lcdDrawFinish(&dev);
}


void tampilkanWifiScanner() {
    lcdFillScreen(&dev, UI_BG);
    char buf[64];

    if (scannerState == 0) {
        // ── State 0: Konfirmasi ──────────────────────────────────
        ui_hdr("WiFi Scanner", "v1");
        rootx_print_text_cb(_cx("Mulai scan?"), 50, "Mulai scan?", UI_TXT, UI_BG);
        ui_yes_no(82, false);
        ui_ftr("< Batal", "OK >");
    }
    else if (scannerState == 1) {
        // ── State 1: Scanning ────────────────────────────────────
        ui_hdr("WiFi Scanner", "memindai");
        rootx_print_text_c(_cx("Sedang memindai..."), 55,
                           "Sedang memindai...", UI_MID, UI_BG);
        int animPct = (int)((millis() / 30) % 100);
        ui_pbar(50, 72, 140, animPct);
        snprintf(buf, sizeof(buf), "Jaringan ditemukan: %d", totalWiFi);
        rootx_print_text_c(_cx(buf), 84, buf, UI_DIM, UI_BG);
        if (scanDone) scannerState = 2;
        ui_ftr("< Batal", NULL);
    }
    else if (scannerState == 2) {
        // ── State 2: Daftar WiFi ─────────────────────────────────
        if (totalWiFi == 0) {
            ui_hdr("WiFi Scanner", "kosong");
            rootx_print_text_c(_cx("Belum ada data!"), 62,
                               "Belum ada data!", UI_MID, UI_BG);
            ui_ftr("< Kembali", NULL);
        } else {
            snprintf(buf, sizeof(buf), "%d jaringan", totalWiFi);
            ui_hdr("WiFi Scanner", buf);

            int maxVis = 6;
            for (int i = 0; i < maxVis; i++) {
                int itemIdx = scrollPosScanner + i;
                if (itemIdx >= totalWiFi) break;
                int yPos = CNT_Y + 1 + i * 17;
                bool act = (i == cursorInScanner);

                // Scroll long SSID on active row
                int len = strlen(listWiFi[itemIdx].ssid);
                char textShow[20] = {0};
                int maxChar = 12;
                if (act && len > maxChar) {
                    int extra = len - maxChar;
                    int off = (millis() / 300) % (extra + 4);
                    if (off > extra) off = extra;
                    strncpy(textShow, listWiFi[itemIdx].ssid + off, maxChar);
                } else {
                    strncpy(textShow, listWiFi[itemIdx].ssid, maxChar);
                }

                snprintf(buf, sizeof(buf), "CH%d %ddB",
                         listWiFi[itemIdx].channel, listWiFi[itemIdx].rssi);
                ui_row(yPos, ics_wifi, textShow, buf, act);
            }
            ui_ftr("< Kembali", "Pilih [OK]");
        }
    }
    else if (scannerState == 3) {
        // ── State 3: Detail ──────────────────────────────────────
        snprintf(buf, sizeof(buf), "%.9s", targetTerkunci.ssid);
        ui_hdr("Detail Jaringan", buf);

        // Scroll long SSID
        int lenSSID = strlen(targetTerkunci.ssid);
        char tmpSSID[22] = {0};
        if (lenSSID > 16) {
            int extra = lenSSID - 16;
            int off = (millis() / 250) % (extra + 4);
            if (off > extra) off = extra;
            strncpy(tmpSSID, targetTerkunci.ssid + off, 16);
        } else { strcpy(tmpSSID, targetTerkunci.ssid); }

        ui_kv(22, "SSID",    tmpSSID,              UI_BLU);
        ui_kv(44, "MAC",     targetTerkunci.mac,    UI_TXT);
        snprintf(buf, sizeof(buf), "%d", targetTerkunci.channel);
        ui_kv(66, "Channel", buf,                   UI_TXT);
        snprintf(buf, sizeof(buf), "%d dBm", targetTerkunci.rssi);
        ui_kv(88, "RSSI",    buf,                   UI_TXT);

        ui_ftr("< Kembali", NULL);
    }
    else if (scannerState == 4) {
        // ── State 4: Action menu ─────────────────────────────────
        snprintf(buf, sizeof(buf), "%.9s", targetTerkunci.ssid);
        ui_hdr("Actions", buf);

        const char         *labels[] = {"Deauth", "Evil Twin", "Clients",
                                         "Track RSSI", "Detail"};
        const unsigned char *icons[]  = {ics_skull, ics_conn, ics_sniff,
                                          ics_wifi,  ics_info};
        for (int i = 0; i < 5; i++) {
            int yPos = CNT_Y + 1 + i * 17;
            ui_row(yPos, icons[i], labels[i], ">", (i == contextCursor));
        }
        ui_ftr("< Kembali", "Pilih [OK]");
    }

    lcdDrawFinish(&dev);
}




void tampilkanStationScanner() {
    lcdFillScreen(&dev, UI_BG);
    char buf[64];

    if (scannerStateSta == 0) {
        // ── State 0: Konfirmasi ──────────────────────────────────
        ui_hdr("Station Scanner", "client");
        rootx_print_text_cb(_cx("Scan client terhubung?"), 50,
                            "Scan client terhubung?", UI_TXT, UI_BG);
        ui_yes_no(82, false);
        ui_ftr("< Batal", "OK >");
    }
    else if (scannerStateSta == 1) {
        // ── State 1: Sniffing ────────────────────────────────────
        ui_hdr("Station Scanner", "sniffing");
        rootx_print_text_c(_cx("Sniffing target..."), 55,
                           "Sniffing target...", UI_MID, UI_BG);
        rootx_print_text_c(_cx(targetTerkunci.ssid), 72,
                           targetTerkunci.ssid, UI_BLU, UI_BG);
        int animPct = (int)((millis() / 30) % 100);
        ui_pbar(50, 88, 140, animPct);
        if (scanStaDone) scannerStateSta = 2;
        ui_ftr("< Batal", NULL);
    }
    else if (scannerStateSta == 2) {
        // ── State 2: Daftar Client ───────────────────────────────
        if (totalStation == 0) {
            ui_hdr("Station Scanner", "kosong");
            rootx_print_text_c(_cx("No clients found!"), 62,
                               "No clients found!", UI_MID, UI_BG);
            ui_ftr("< Rescan", NULL);
        } else {
            snprintf(buf, sizeof(buf), "%d client", totalStation);
            ui_hdr("Station Scanner", buf);

            int maxVis = 5;
            for (int i = 0; i < maxVis; i++) {
                int itemIdx = scrollPosScanner + i;
                if (itemIdx >= totalStation) break;
                int yPos = CNT_Y + 1 + i * 19;
                bool act = (i == cursorInScanSta);
                snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                         listStation[itemIdx].mac[0], listStation[itemIdx].mac[1],
                         listStation[itemIdx].mac[2], listStation[itemIdx].mac[3],
                         listStation[itemIdx].mac[4], listStation[itemIdx].mac[5]);
                char meta[12];
                snprintf(meta, sizeof(meta), "%ddBm", listStation[itemIdx].rssi);
                ui_row(yPos, ics_sniff, buf, meta, act);
            }
            ui_ftr("< Kembali", "Pilih [OK]");
        }
    }
    else if (scannerStateSta == 3) {
        // ── State 3: Detail Client ───────────────────────────────
        ui_hdr("Detail Client", "info");

        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 targetSta.mac[0], targetSta.mac[1], targetSta.mac[2],
                 targetSta.mac[3], targetSta.mac[4], targetSta.mac[5]);
        ui_kv(22, "MAC",     buf,  UI_TXT);
        snprintf(buf, sizeof(buf), "%d dBm", targetSta.rssi);
        ui_kv(44, "RSSI",    buf,  UI_TXT);
        snprintf(buf, sizeof(buf), "%d", targetSta.paket_count);
        ui_kv(66, "Paket",   buf,  UI_TXT);

        ui_ftr("< Kembali", NULL);
    }
    else if (scannerStateSta == 4) {
        // ── State 4: Action menu ─────────────────────────────────
        ui_hdr("Actions", "client");

        const char         *labels[] = {"Kick Client", "Detail"};
        const unsigned char *icons[]  = {ics_skull,    ics_info};
        for (int i = 0; i < 2; i++) {
            int yPos = CNT_Y + 1 + i * 17;
            ui_row(yPos, icons[i], labels[i], ">", (i == contextCursor));
        }
        ui_ftr("< Kembali", "Pilih [OK]");
    }

    lcdDrawFinish(&dev);
}





void tampilkandeauthsta() {
    lcdFillScreen(&dev, UI_BG);
    char buf[64];

    ui_hdr("Deauth Station", "berjalan");

    // Target MAC
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             targetSta.mac[0], targetSta.mac[1], targetSta.mac[2],
             targetSta.mac[3], targetSta.mac[4], targetSta.mac[5]);
    ui_kv(25, "Target", buf, UI_TXT);

    // Channel
    snprintf(buf, sizeof(buf), "%d", targetTerkunci.channel);
    ui_kv(47, "Channel", buf, UI_TXT);

    // Progress bar
    int animPct = (int)((millis() / 30) % 100);
    ui_pbar(50, 70, 140, animPct);
    snprintf(buf, sizeof(buf), "%d%%", animPct);
    rootx_print_text_c(SCR_W - (int)strlen(buf)*FW - 8, 80, buf, UI_MID, UI_BG);

    ui_ftr("< Hentikan", NULL);
    lcdDrawFinish(&dev);
}

void tampilkanDeauthScreen() {
    lcdFillScreen(&dev, UI_BG);
    char buf[64];

    if (deauthState == 0) {
        // ── State 0: Konfirmasi ──────────────────────────────────
        ui_hdr("Deauth Attack", "waspada");
        rootx_print_text_cb(_cx("Serang target?"), 48, "Serang target?", UI_TXT, UI_BG);
        // Show target SSID in blue
        char shortSsid[16]; strncpy(shortSsid, targetTerkunci.ssid, 15); shortSsid[15]='\0';
        rootx_print_text_c(_cx(shortSsid), 64, shortSsid, UI_BLU, UI_BG);
        ui_yes_no(84, true);
        ui_ftr("< Tidak", "OK >");
    }
    else if (deauthState == 1) {
        // ── State 1: Berjalan ────────────────────────────────────
        ui_hdr("Deauth Attack", "berjalan");
        ui_kv(25, "Target",  targetTerkunci.ssid, UI_BLU);
        snprintf(buf, sizeof(buf), "%d", targetTerkunci.channel);
        ui_kv(47, "Channel", buf, UI_TXT);
        int animPct = (int)((millis() / 30) % 100);
        ui_pbar(50, 70, 140, animPct);
        snprintf(buf, sizeof(buf), "%d%%", animPct);
        rootx_print_text_c(SCR_W-(int)strlen(buf)*FW-8, 80, buf, UI_MID, UI_BG);
        ui_ftr("< Hentikan", NULL);
    }
    lcdDrawFinish(&dev);
}

void tampilkanBrightness() {
    lcdFillScreen(&dev, UI_BG);
    char buf[16];

    ui_hdr("Brightness", "display");

    int persen = (int)map(brightnessValue, 0, 255, 0, 100);
    snprintf(buf, sizeof(buf), "%d%%", persen);

    // Big percentage – medium font centred
    int gw = (int)strlen(buf) * 12;
    rootx_print_text_sedang((SCR_W - gw)/2, CNT_CY - 28, buf, UI_TXT, UI_BG);

    // Progress bar centred
    ui_pbar(50, CNT_CY - 2, 140, persen);

    // Hint text
    rootx_print_text_c(_cx("Atur dengan ^ v"), CNT_CY + 16,
                       "Atur dengan ^ v", UI_MID, UI_BG);

    ui_ftr("< Kembali", "^ v atur");
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
    lcdFillScreen(&dev, UI_BG);
    char buf[64];

    if (spamState == 0) {
        // ── State 0: Konfirmasi ──────────────────────────────────
        ui_hdr(judul, "waspada");
        rootx_print_text_cb(_cx("Mulai spam?"), 50, "Mulai spam?", UI_TXT, UI_BG);
        ui_yes_no(82, true);
        ui_ftr("< Tidak", "OK >");
    }
    else if (spamState == 1) {
        // ── State 1: Berjalan ────────────────────────────────────
        ui_hdr(judul, "berjalan");
        snprintf(buf, sizeof(buf), "Mode: %s", subTeks);
        rootx_print_text_c(_cx(buf), 55, buf, UI_MID, UI_BG);
        int animPct = (int)((millis() / 30) % 100);
        ui_pbar(50, 72, 140, animPct);
        snprintf(buf, sizeof(buf), "%d%%", animPct);
        rootx_print_text_c(SCR_W-(int)strlen(buf)*FW-8, 84, buf, UI_DIM, UI_BG);
        ui_ftr("< Hentikan", NULL);
    }
    lcdDrawFinish(&dev);
}








void tampilkanEvilTwinScreen() {
    lcdFillScreen(&dev, UI_BG);

    if (evilTwinState == 0) {
        // ── State 0: Konfirmasi ──────────────────────────────────
        ui_hdr("Evil Twin", "waspada");
        rootx_print_text_cb(_cx("Mulai Evil Twin?"), 48,
                            "Mulai Evil Twin?", UI_TXT, UI_BG);
        rootx_print_text_c(_cx(targetTerkunci.ssid), 64,
                           targetTerkunci.ssid, UI_BLU, UI_BG);
        ui_yes_no(84, true);
        ui_ftr("< Tidak", "OK >");
    }
    else if (evilTwinState == 1) {
        // ── State 1: Menunggu ────────────────────────────────────
        ui_hdr("Evil Twin", "menunggu");
        rootx_print_text_c(_cx("Menunggu data..."), 55,
                           "Menunggu data...", UI_MID, UI_BG);
        // Animated dots
        int dotPhase = (millis() / 300) % 3;
        const char *dots[] = {"  .  ", "  .. ", "  ..."};
        rootx_print_text_cb(_cx(dots[dotPhase]), 73, dots[dotPhase], UI_BLU, UI_BG);
        ui_ftr("< Hentikan", NULL);
    }
    else if (evilTwinState == 2) {
        // ── State 2: Password ditangkap ─────────────────────────
        ui_hdr("Evil Twin", "berhasil!");
        ui_kv(22, "Target",   targetTerkunci.ssid, UI_BLU);
        ui_kv(44, "Password", stolenPassword,       UI_BLU);
        ui_ftr("< Kembali", NULL);
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
    lcdFillScreen(&dev, UI_BG);

    if (currentIRSavedState == IR_SAVED_STATE_LIST) {
        // ── State LIST ───────────────────────────────────────────
        char badge[12];
        snprintf(badge, sizeof(badge), "%d tersimpan", totalSavedRemotes);
        ui_hdr("Saved Remote", badge);

        if (totalSavedRemotes == 0) {
            rootx_print_text_c(_cx("Data kosong!"), CNT_CY,
                               "Data kosong!", UI_MID, UI_BG);
        } else {
            const unsigned char *icons[] = {ics_tv, ics_ac, ics_saved};
            int startIdx = (savedRemoteIndex / 3) * 3;
            for (int i = 0; i < 3; i++) {
                int curr = startIdx + i;
                if (curr >= totalSavedRemotes) break;
                int yPos = CNT_Y + 1 + i * 17;
                bool act = (curr == savedRemoteIndex);
                const unsigned char *ico = icons[i % 3];
                ui_row(yPos, ico, listSavedRemotes[curr].nama, ">", act);
            }
        }
        ui_ftr("< Kembali", "Pilih [OK]");
    }
    else if (currentIRSavedState == IR_SAVED_STATE_ACTION) {
        // ── State ACTION ─────────────────────────────────────────
        ui_hdr("Action", listSavedRemotes[savedRemoteIndex].nama);

        const char         *labels[] = {"Transmit", "Hapus"};
        const unsigned char *icons[]  = {ics_ir,    ics_file};
        for (int i = 0; i < 2; i++) {
            int yPos = CNT_Y + 1 + i * 17;
            ui_row(yPos, icons[i], labels[i], ">", (i == actionMenuIndex));
        }
        ui_ftr("< Kembali", "Pilih [OK]");
    }
    else if (currentIRSavedState == IR_SAVED_STATE_SENDING) {
        // ── State SENDING ────────────────────────────────────────
        ui_hdr("IR Send", "mengirim");
        rootx_print_text_cb(_cx("Mengirim IR..."), CNT_CY - 10,
                            "Mengirim IR...", UI_TXT, UI_BG);
        ui_pbar(70, CNT_CY + 8, 100, 100);
        ui_ftr("< Batal", NULL);
    }

    lcdDrawFinish(&dev);
}

void tampilkanMenuIR() {
    lcdFillScreen(&dev, UI_BG);
    char buf[32];

    if (currentIRState == IR_STATE_CONFIRM) {
        // ── State CONFIRM ────────────────────────────────────────
        ui_hdr("IR Sniffer", "inframerah");
        rootx_print_text_cb(_cx("Mulai merekam IR?"), 50,
                            "Mulai merekam IR?", UI_TXT, UI_BG);
        ui_yes_no(82, false);
        ui_ftr("< Tidak", "OK >");
    }
    else if (currentIRState == IR_STATE_WAITING) {
        // ── State WAITING ────────────────────────────────────────
        ui_hdr("IR Sniffer", "menunggu");
        rootx_print_text_c(_cx("Menunggu sinyal IR..."), 55,
                           "Menunggu sinyal IR...", UI_MID, UI_BG);
        int dotPhase = (millis() / 300) % 3;
        const char *dots[] = {"  .  ", "  .. ", "  ..."};
        rootx_print_text_cb(_cx(dots[dotPhase]), 73, dots[dotPhase], UI_BLU, UI_BG);
        ui_ftr("< Batal", NULL);
    }
    else if (currentIRState == IR_STATE_RESULT) {
        // ── State RESULT ─────────────────────────────────────────
        ui_hdr("IR Result", "tersimpan");
        ui_kv(22, "Tipe",   "RAW CLONER", UI_TXT);
        snprintf(buf, sizeof(buf), "%d", last_ir_data.num_pulses);
        ui_kv(44, "Pulses", buf,           UI_TXT);
        // Saved confirmation
        rootx_print_text_c(_cx("v Tersimpan di SD"), 72,
                           "v Tersimpan di SD", UI_BLU, UI_BG);
        ui_ftr("< Kembali", NULL);
    }
    lcdDrawFinish(&dev);
}



// ==========================================
// MESIN TETRIS VERTIKAL (Miring 90 Derajat)
// ==========================================




void renderAboutScreen() {
    lcdFillScreen(&dev, UI_BG);

    ui_hdr("About", "ROOTX OS");

    // Bordered card
    lcdDrawRect(&dev, 8, CNT_Y + 4, SCR_W - 8, FTR_Y - 4, UI_SEP);

    // Title inside card
    rootx_print_text_cb(_cx("ROOTX OS"), CNT_Y + 10, "ROOTX OS", UI_TXT, UI_BG);
    lcdDrawLine(&dev, 10, CNT_Y + 24, SCR_W - 10, CNT_Y + 24, UI_SEP);

    // Info rows
    ui_kv(CNT_Y + 30, "Versi  ", "1.0.0",    UI_TXT);
    ui_kv(CNT_Y + 46, "Core   ", "ESP32-S3", UI_TXT);
    ui_kv(CNT_Y + 62, "Pembuat", "Andyy",    UI_BLU);

    ui_ftr("< Kembali", NULL);
    lcdDrawFinish(&dev);
}

void renderRebootScreen() {
    lcdFillScreen(&dev, UI_BG);

    ui_hdr("Reboot", "konfirmasi");

    // Bordered warning card
    lcdDrawRect(&dev, 12, CNT_Y + 8, SCR_W - 12, FTR_Y - 8, UI_WRN);

    rootx_print_text_cb(_cx("Reboot sekarang?"), CNT_CY - 12,
                        "Reboot sekarang?", UI_TXT, UI_BG);
    rootx_print_text_c(_cx("Data belum tersimpan"), CNT_CY + 6,
                       "Data belum tersimpan", UI_MID, UI_BG);

    ui_yes_no(CNT_CY + 22, true);
    ui_ftr("< Tidak", "OK >");
    lcdDrawFinish(&dev);
}

// Variabel State buat SD Manager
// Variabel State buat SD Manager
int sdActionIdx = 0; // 0: EXIT, 1: FILES, 2: FORMAT
int sdState = 0;     // 0: Main Dashboard, 1: Confirm Format, 2: Formatting

void renderSdManager() {
    lcdFillScreen(&dev, UI_BG);
    char buf[48];

    if (sdState == 0) {
        // ── State 0: Dashboard ───────────────────────────────────
        struct statvfs st;
        float total_mb = 0, free_mb = 0, used_mb = 0;
        int percent = 0;
        bool is_mounted = false;

        if      (statvfs("/sdcard",  &st) == 0) is_mounted = true;
        else if (statvfs("/sdcard/", &st) == 0) is_mounted = true;
        else if (statvfs("fatfs",    &st) == 0) is_mounted = true;

        if (is_mounted) {
            uint64_t total_bytes = (uint64_t)st.f_blocks * (uint64_t)st.f_frsize;
            uint64_t free_bytes  = (uint64_t)st.f_bfree  * (uint64_t)st.f_frsize;
            total_mb = (float)total_bytes / (1024.0f * 1024.0f);
            free_mb  = (float)free_bytes  / (1024.0f * 1024.0f);
            used_mb  = total_mb - free_mb;
            if (total_mb > 0) percent = (int)((used_mb / total_mb) * 100);
        }

        ui_hdr("SD Manager", is_mounted ? "terpasang" : "error");

        if (!is_mounted) {
            rootx_print_text_c(_cx("VFS Error!"), CNT_CY - 8,
                               "VFS Error!", UI_RED, UI_BG);
            rootx_print_text_c(_cx("Gagal baca SD"), CNT_CY + 8,
                               "Gagal baca SD", UI_MID, UI_BG);
        } else {
            // Storage info rows
            snprintf(buf, sizeof(buf), "%.0f MB", total_mb);
            ui_kv(CNT_Y + 8,  "Total", buf, UI_TXT);
            snprintf(buf, sizeof(buf), "%.0f MB", free_mb);
            ui_kv(CNT_Y + 26, "Bebas", buf, UI_BLU);

            // Progress bar
            snprintf(buf, sizeof(buf), "%d%% terpakai", percent);
            rootx_print_text_c(_cx(buf), CNT_Y + 46, buf, UI_DIM, UI_BG);
            ui_pbar(50, CNT_Y + 62, 140, percent);
        }

        // Scroll menu: EXIT / FILES / FORMAT
        const char *menuNames[] = {"EXIT", "FILES", "FORMAT"};
        rootx_print_text_cb(_cx(menuNames[sdActionIdx]),
                            FTR_Y + 1, menuNames[sdActionIdx], UI_TXT, UI_HDR);
        rootx_print_text_c(SCR_W - FW - 8, FTR_Y + 1, ">", UI_GRY, UI_HDR);

        // Draw footer bg first then overlay text
        lcdDrawFillRect(&dev, 0, FTR_Y, SCR_W, SCR_H, UI_HDR);
        lcdDrawLine(&dev, 0, FTR_Y, SCR_W, FTR_Y, UI_SEP);
        rootx_print_text_c(8, FTR_Y + 1, "<", UI_GRY, UI_HDR);
        rootx_print_text_cb(_cx(menuNames[sdActionIdx]),
                            FTR_Y + 1, menuNames[sdActionIdx], UI_TXT, UI_HDR);
        rootx_print_text_c(SCR_W - FW - 8, FTR_Y + 1, ">", UI_GRY, UI_HDR);
    }
    else if (sdState == 1) {
        // ── State 1: Konfirmasi Format ───────────────────────────
        ui_hdr("SD Manager", "peringatan");
        rootx_print_text_cb(_cx("Format SD Card?"), CNT_CY - 18,
                            "Format SD Card?", UI_TXT, UI_BG);
        rootx_print_text_c(_cx("SEMUA DATA TERHAPUS!"), CNT_CY - 2,
                           "SEMUA DATA TERHAPUS!", UI_RED, UI_BG);
        ui_yes_no(CNT_CY + 18, true);
        ui_ftr("< Tidak", "OK >");
    }
    else if (sdState == 2) {
        // ── State 2: Memformat ───────────────────────────────────
        ui_hdr("SD Manager", "memformat");
        rootx_print_text_cb(_cx("Memformat SD..."), CNT_CY - 10,
                            "Memformat SD...", UI_TXT, UI_BG);
        int animPct = (int)((millis() / 30) % 100);
        ui_pbar(50, CNT_CY + 8, 140, animPct);
        ui_ftr(NULL, NULL);
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
    // ── Init: baca SD Card ───────────────────────────────────────
    if (!isFileExpInit) {
        sdTotalFiles = 0;
        sdFileCursor = 0;
        sdFileScroll = 0;
        sdFileState  = 0;

        DIR *d = opendir(currentPath);
        if (d) {
            struct dirent *dir;
            while ((dir = readdir(d)) != NULL && sdTotalFiles < MAX_FILES) {
                if (dir->d_name[0] == '.') continue;
                strncpy(sdFileNames[sdTotalFiles], dir->d_name, 31);
                sdFileNames[sdTotalFiles][31] = '\0';
                sdTotalFiles++;
            }
            closedir(d);
        }
        isFileExpInit = true;
    }

    lcdFillScreen(&dev, UI_BG);

    // Badge: last part of path
    const char *pathDisp = strrchr(currentPath, '/');
    pathDisp = pathDisp ? pathDisp + 1 : currentPath;
    ui_hdr("File Explorer", pathDisp[0] ? pathDisp : "/");

    if (sdTotalFiles == 0) {
        rootx_print_text_c(_cx("Folder kosong!"), CNT_CY,
                           "Folder kosong!", UI_MID, UI_BG);
        ui_ftr("< Kembali", NULL);
    }
    else if (sdFileState == 0) {
        // ── State 0: Daftar file ──────────────────────────────────
        int maxList = 6;
        for (int i = 0; i < maxList; i++) {
            int fileIdx = sdFileScroll + i;
            if (fileIdx >= sdTotalFiles) break;
            int yPos = CNT_Y + 1 + i * 17;
            bool act  = (fileIdx == sdFileCursor);

            // Truncate name
            char nameDisp[18] = {0};
            strncpy(nameDisp, sdFileNames[fileIdx], 17);

            ui_row(yPos, ics_file, nameDisp, ">", act);
        }

        char foot[20];
        snprintf(foot, sizeof(foot), "%d/%d [OK]",
                 sdFileCursor + 1, sdTotalFiles);
        ui_ftr("< Kembali", foot);
    }
    else if (sdFileState == 1) {
        // ── State 1: Konfirmasi hapus ─────────────────────────────
        ui_hdr("File Explorer", "hapus file?");

        char truncName[20];
        snprintf(truncName, sizeof(truncName), "%.18s", sdFileNames[sdFileCursor]);

        rootx_print_text_cb(_cx("Hapus file ini?"), CNT_CY - 18,
                            "Hapus file ini?", UI_TXT, UI_BG);
        rootx_print_text_c(_cx(truncName), CNT_CY - 2, truncName, UI_BLU, UI_BG);
        ui_yes_no(CNT_CY + 18, true);
        ui_ftr("< Tidak", "OK >");
    }

    lcdDrawFinish(&dev);
}

// Definisi state TV-B-Gone
int tvbgoneState = 0;
int tvbgoneMenuIdx = 0;
int tvbgoneProgress = 0;
int tvbgoneTotal = 0;

void renderTvBGone() {
    lcdFillScreen(&dev, UI_BG);
    char buf[32];

    if (tvbgoneState == 0) {
        // ── State 0: Pilih region ────────────────────────────────
        ui_hdr("TV-B-GONE", "pilih region");

        const char *menus[]     = {"NA / ASIA", "EUROPE", "ALL WORLD"};
        const unsigned char *icons[] = {ics_wifi, ics_wifi, ics_wifi};

        for (int i = 0; i < 3; i++) {
            int yPos = CNT_Y + 4 + i * 20;
            ui_row(yPos, icons[i], menus[i], ">", (i == tvbgoneMenuIdx));
        }
        ui_ftr("< Keluar", "OK Start");
    }
    else if (tvbgoneState == 1) {
        // ── State 1: Mengirim ────────────────────────────────────
        ui_hdr("TV-B-GONE", "mengirim");

        // Animated blink text
        if ((millis() / 500) % 2 == 0) {
            rootx_print_text_cb(_cx("Mengirim kode IR..."), CNT_CY - 20,
                                "Mengirim kode IR...", UI_TXT, UI_BG);
        } else {
            rootx_print_text_cb(_cx("Mengirim kode IR..."), CNT_CY - 20,
                                "Mengirim kode IR...", UI_MID, UI_BG);
        }

        // Progress bar
        int pct = (tvbgoneTotal > 0)
                  ? (tvbgoneProgress * 100) / tvbgoneTotal : 0;
        ui_pbar(50, CNT_CY - 2, 140, pct);

        // Counter text
        snprintf(buf, sizeof(buf), "Code: %d / %d",
                 tvbgoneProgress, tvbgoneTotal);
        rootx_print_text_c(_cx(buf), CNT_CY + 12, buf, UI_MID, UI_BG);

        ui_ftr("< Hentikan", NULL);
    }

    lcdDrawFinish(&dev);
}
