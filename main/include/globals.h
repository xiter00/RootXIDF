#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdbool.h>
#include <stdint.h>
#include "st7789.h"
#include "esp_spiffs.h"

extern TFT_t dev; // Biar layar bisa diakses dari file mana aja

// --- SETTING PIN JOYSTICK ---
#define PIN_UP    42
#define PIN_DOWN  41
#define PIN_LEFT  40
#define PIN_RIGHT 39
#define PIN_OK    38
#define WHITE 1
#define BLACK 0

#define BTN_NONE  0
#define BTN_UP    1
#define BTN_DOWN  2
#define BTN_LEFT  3
#define BTN_RIGHT 4
#define BTN_OK    5

#define MODE_IR_SNIFFER 9
#define MODE_SAVED_REMOTE 10

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

void loadSavedRemotes(void);


// --- VARIABEL ENGINE GAME ---
extern int baca_highscore_dino();
void simpan_highscore_dino(int hs);
extern int baca_highscore_snake();
void simpan_highscore_snake(int hs);
// --- EKSPOR FUNGSI TEKS ST7789 BIAR BISA DIPAKAI DI SEMUA FILE ---
extern void rootx_print_text(int x, int y, const char* str, uint16_t fg, uint16_t bg);
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
