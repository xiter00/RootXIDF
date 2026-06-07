#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdbool.h>
#include <stdint.h>
#include "st7789.h"
#include "esp_spiffs.h"
#include "driver/gpio.h"
#include "font7x13.h"
#include "background.h"
#include "iconmenu.h"
TFT_t dev; // Biar layar bisa diakses dari file mana aja





//COLOR
#define RED    rgb565(255,   0,   0) // 0xf800
#define GREEN  rgb565(  0, 255,   0) // 0x07e0
#define BLUE   rgb565(  0,   0, 255) // 0x001f
#define WHITE  rgb565(255, 255, 255) // 0xffff
#define BLACK  rgb565(  0,   0,   0) // 0x0000
#define GRAY   rgb565(128, 128, 128) // 0x8410
#define YELLOW rgb565(255, 255,   0) // 0xFFE0
#define CYAN   rgb565(  0, 156, 209) // 0x04FA 
#define PURPLE rgb565(128,   0, 128) // 0x8010
#define ORANGE rgb565(255,   128, 0) 
#define DARK_GRAY rgb565(72, 74, 74)
#define BLOOD_RED rgb565(184, 0, 0)
#define ICE_CYAN rgb565(0, 252, 255)
#define LGRID_COLOR rgb565(17, 6, 9)   
#define PINK       rgb565(255, 30, 90) 
#define LCYAN       rgb565(0, 255, 255) 
#define LDARK_BG    rgb565(10, 10, 10)  


// --- SETTING PIN JOYSTICK ---
#define PIN_UP    42
#define PIN_DOWN  41
#define PIN_LEFT  40
#define PIN_RIGHT 39
#define PIN_OK    38

// PIN SDCARD
// PIN SDCARD (Dipindah biar gak tabrakan sama Layar)
#define PIN_CS   15 
#define PIN_MOSI 16  
#define PIN_MISO 17 
#define PIN_CLK  18


#define MOUNT_POINT "/sdcard"


// PIN LAYAR
#define LCD_SDA 12
#define LCD_SCL 11
#define LCD_RES 8
#define LCD_DC 9
#define LCD_CS 10
#define LCD_BLK -1

// IR
#define IR_RX_PIN GPIO_NUM_4  
#define IR_TX_PIN GPIO_NUM_5


//BATTERY 
#define R1 440.0
#define R2 440.0
#define VOLTAGE_DIVIDER_RATIO ((R1 + R2) / R2) 
#define CALIBRATION_FACTOR 1.00 


//BTN
#define BTN_NONE  0
#define BTN_UP    1
#define BTN_DOWN  2
#define BTN_LEFT  3
#define BTN_RIGHT 4
#define BTN_OK    5

#define MODE_IR_SNIFFER 9
#define MODE_SAVED_REMOTE 10


// STRUKTUR MENU
typedef struct {
    const uint8_t *icon_large;   // 48x48
    const uint8_t *icon_small;   // 32x32
    const char *label;
} MenuItem;

extern MenuItem menuList[5];
extern int carouselCurrentIdx;
extern int carouselAnimFrame;
extern bool carouselAnimating;
extern uint32_t carouselAnimStart;


// --- STRUKTUR WIFI (String diganti char array) ---
typedef struct {
  int id;
  char ssid[33]; // Max SSID length itu 32 + 1 null terminator
  int rssi;
  int channel;
  char encrypt[20];
  bool is_open;
  char mac[18];
} WiFiData;

typedef struct {
    int id;          // <--- Tambahin ID
    uint8_t mac[6];
    int rssi;
    int paket_count;
} StationInfo;

// --- IR SYSTEM GLOBALS ---
// --- VARIABEL TV-B-GONE ---
extern int tvbgoneState;    // 0: Menu, 1: Firing
extern int tvbgoneMenuIdx;  // 0: NA, 1: EU, 2: ALL
extern int tvbgoneProgress; // Kode ke-berapa yang lagi ditembak
extern int tvbgoneTotal;    // Total kode yang harus ditembak

// --- SAVED REMOTE GLOBALS ---
// --- IR SYSTEM GLOBALS ---
typedef enum {
    IR_STATE_CONFIRM,
    IR_STATE_WAITING,
    IR_STATE_RESULT
} ir_read_state_t;

typedef struct {
    uint16_t pulses[200]; // Max 200 kedipan (cukup buat remote TV & AC)
    int num_pulses;
} ir_data_t;

extern ir_read_state_t currentIRState;
extern ir_data_t last_ir_data;
extern bool triggerReadIR;

// --- SAVED REMOTE GLOBALS ---
typedef enum {
    IR_SAVED_STATE_LIST,
    IR_SAVED_STATE_ACTION,
    IR_SAVED_STATE_SENDING
} ir_saved_state_t;

typedef struct {
    char nama[16];   
    int num_pulses;
    uint16_t pulses[200]; 
} SavedRemote_t;

extern ir_saved_state_t currentIRSavedState;
extern SavedRemote_t listSavedRemotes[20];
extern int totalSavedRemotes;
extern int savedRemoteIndex;
extern int actionMenuIndex;
// --- VARIABEL FILE EXPLORER ---
#define MAX_FILES 20  // Maksimal 20 file biar RAM ESP32 lu gak jebol
extern char sdFileNames[MAX_FILES][32];
extern int sdTotalFiles;
extern int sdFileCursor;
extern int sdFileScroll;
extern int sdFileState; // 0: List, 1: Confirm Delete
extern bool isFileExpInit;
extern int carouselDirection;

void loadSavedRemotes(void);

extern void drawBackground(void);
// --- VARIABEL ENGINE GAME ---
extern int baca_highscore_dino();
void simpan_highscore_dino(int hs);
extern int baca_highscore_snake();
void simpan_highscore_snake(int hs);
// --- EKSPOR FUNGSI TEKS ST7789 BIAR BISA DIPAKAI DI SEMUA FILE ---
extern void rootx_print_text_custom(int x, int y, const char* str, uint16_t fg, uint16_t bg);
extern void rootx_print_text_kecil(int x, int y, const char* str, uint16_t fg, uint16_t bg);
extern void rootx_print_text_sedang(int x, int y, const char* str, uint16_t fg, uint16_t bg);
extern void rootx_print_text_gede(int x, int y, const char* str, uint16_t fg, uint16_t bg);

// --- EXTERN VARIABEL GLOBAL ---
// --- EXTERN VARIABEL GLOBAL ---

// --- BATTERY GLOBALS ---
extern int batteryPercent;
void init_battery(void);
int read_battery_percentage(void);

// --- VARIABEL SNAKE GAME ---
extern int snakeDir;      // Arah Ular: 0=Kanan, 1=Bawah, 2=Kiri, 3=Atas
extern int snakeState;    // 0=Main, 1=Game Over
extern int snakeScore;
extern int snakeHighScore;
extern bool isSnakeInitialized;

// --- VARIABEL TETRIS GAME ---
extern int tetrisState;
extern int tetrisScore;
extern int tetrisHighScore;
extern bool isTetrisInitialized;
extern void handleTetrisInput(int btn); // Biar input_system bisa manggil


extern int dinoLimit;   

// Variabel buat Evil Twin
extern bool isEvilTwin;
extern int evilTwinState; 
extern char stolenPassword[64];
extern bool triggerEvilTwin;



extern float rawScore;
extern int dinoScore, dinoHighScore;
extern int dinoY;        // Posisi tanah baru buat Dino 24px
extern float dinoVy;      
extern bool isJumping;
extern int obstacleX, obstacleY, obstacleType; // 0=Kaktus1, 1=Kaktus2, 2=Burung
extern float gameSpeed; 
extern int dinoState, endTimer;      
extern int skyX; // Posisi matahari/bulan

// Posisi Bintang (Latar Belakang)
extern int starX[5];
extern int starY[5];

extern bool isWiFiConnected;
extern char connSSID[33];
extern int connRSSI;
extern int connCH;
extern bool triggerDisconnect;
// Update submenu WiFi
extern const char* subMenuWiFi[]; 
extern int statusKoneksi; // 0: Connecting, 1: Berhasil, 2: Gagal

extern char inputPassword[64];
extern int cursorPass; // Posisi karakter yang lagi diedit
extern bool triggerConnect; 
extern bool triggerTrack; // Tambahin ini di deretan extern bool
extern int deauthProgress;
extern bool inSubMenu;
extern int currentMenu;
extern int currentSub;
extern int topMenu;
extern WiFiData listWiFi[30];
extern StationInfo listStation[30];
extern StationInfo targetSta;
extern WiFiData targetTerkunci; 
extern int brightnessValue;
extern int spamState; 
extern bool isSpamming;
extern int aktifModeSpam;
extern bool spamUdahSetup;
extern bool deauthUdahSetup;
extern int scannerState; 
extern int scannerStateSta;  // Udah bener gak pake 'b'
extern uint32_t popUpTimer; 
extern bool triggerScan; 
extern bool triggerScanSta;  // Tambahan
extern bool scanDone;    
extern bool scanStaDone;     // Tambahan
extern int totalWiFi;
extern int totalStation;
extern int cursorInScanner; 
extern int cursorInScanSta;
extern int scrollPosScanner;
extern int targetLockedIdx;
extern int contextCursor;
extern bool adaTarget;  
extern bool adaTargetSta;    // Tambahan
extern int deauthState;
extern bool isDeauthing;
extern bool isDeauthSta;     // Tambahan
extern bool sedang_scan;
extern int appMode;

#endif
