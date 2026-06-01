#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "globals.h"
#include "esp_wifi.h"
#include <string.h>
#include "tvbgone_engine.h"
#include <unistd.h>
#include <sys/stat.h>

// Ambil array misterius dari WORLDcodes.c
extern const struct IrCode* const NApowerCodes[];
extern const struct IrCode* const EUpowerCodes[];
extern const uint8_t num_NAcodes;
extern const uint8_t num_EUcodes;
extern TaskHandle_t tvbgoneTaskHandle;
extern void tvbgone_fire_task(void *pvParameters);

// Pengganti String biar enteng dan cepat!


// Tarik fungsi dari display buat set brightness
extern void setOledBrightness(uint8_t level);
extern void tampilkanMenuSavedIR(void);
extern void tampilkanBrightness(void);
extern void transmit_ir_raw(uint16_t* pulses, int num_pulses);

extern void loadSavedRemotes(void);
extern void hapus_remote_di_sd(int index_target);


// Pengganti millis()
uint32_t input_millis() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}







// Deklarasi fungsi di bawah
void handleNavigasiScanner(int btn);
void handleNavigasiDeauth(int btn);
void handleNavigasiSpam(int btn);
void handleNavigasiScanSta(int btn);
void handleEvilTwinInput(int btn);
void handleDinoInput(int btn);







void handleJoystick() {
    static uint32_t lastPress = 0;
    uint32_t debounceLimit = (appMode == 8) ? 120 : 250; 
    if (input_millis() - lastPress < debounceLimit) return; 

    // --- 1. TENTUIN DULU BTN NYA ---
    int btn = BTN_NONE;
    if (gpio_get_level(PIN_UP) == 0)         btn = BTN_UP;
    else if (gpio_get_level(PIN_DOWN) == 0)  btn = BTN_DOWN;
    else if (gpio_get_level(PIN_LEFT) == 0)  btn = BTN_LEFT;
    else if (gpio_get_level(PIN_RIGHT) == 0) btn = BTN_RIGHT;
    else if (gpio_get_level(PIN_OK) == 0)    btn = BTN_OK;

    if (btn == BTN_NONE) return; 

    // ==========================================
    // 2. HANDLER SUB-APLIKASI (DIPISAH & DI-REM)
    // ==========================================
    if (appMode == 12) {
        static uint32_t pressTime = 0; // Buat ngitung lama ditahan

        // 1. LOGIKA BELOK ULAR
        if (btn == BTN_RIGHT && snakeDir != 2) {
            snakeDir = 0; // Kanan
        }
        else if (btn == BTN_DOWN && snakeDir != 3) {
            snakeDir = 1; // Bawah
        }
        else if (btn == BTN_LEFT && snakeDir != 0 && snakeState == 0) {
            snakeDir = 2; // Kiri
        }
        else if (btn == BTN_UP && snakeDir != 1) {
            snakeDir = 3; // Atas
        }

        // 2. LOGIKA KELUAR PAS GAME OVER (Mati)
        if (snakeState == 1) {
            if (btn == BTN_OK) { // Restart Game
                snakeState = 0;     
                isSnakeInitialized = false; 
            } else if (btn == BTN_LEFT) { // Keluar Ke Menu Utama
                appMode = 0; 
                isSnakeInitialized = false;
            }
        }

        // 3. LOGIKA PANIC BUTTON (Tahan OK buat keluar pas lagi main)
        if (btn == BTN_OK && snakeState == 0) {
            if (pressTime == 0) pressTime = input_millis(); // Mulai itung
            
            // Kalau ditahan lebih dari 1500ms (1.5 detik)
            if (input_millis() - pressTime > 1500) {
                appMode = 0; // Langsung mental ke Menu Utama
                isSnakeInitialized = false;
                pressTime = 0;
            }
        } else {
            pressTime = 0; // Kalau tombol dilepas, reset itungan
        }

        lastPress = input_millis(); // Update waktu terakhir dipencet
        return; 
    

    }

    if (appMode == MODE_IR_SNIFFER) { 
        if (btn == BTN_OK || btn == BTN_RIGHT) {
            if (currentIRState == IR_STATE_CONFIRM) {
                currentIRState = IR_STATE_WAITING;
                triggerReadIR = true; 
            }
            else if (currentIRState == IR_STATE_RESULT) {
                currentIRState = IR_STATE_CONFIRM; // Scan ulang
            }
        }
        else if (btn == BTN_LEFT) { 
            if (currentIRState == IR_STATE_WAITING) {
                triggerReadIR = false;
                currentIRState = IR_STATE_CONFIRM; // Batalin nunggu
            } 
            else if (currentIRState == IR_STATE_RESULT) {
                appMode = 0;// Kembali dari hasil scan
            }
            else {
                appMode = 0; // Balik ke Submenu dengan benar
            }
        }
        lastPress = input_millis();
        return; // <--- KUNCI SAKTI BIAR GAK BOCOR KE MENU UTAMA
    }
        if (appMode == 14) { // Menu About RootX
        if (btn == BTN_LEFT || btn == BTN_OK) {
            appMode = 0; // Balik ke Menu Utama
        }
        lastPress = input_millis();
        return; 
    }
    
        if (appMode == 15) {
        if (btn == BTN_OK || btn == BTN_RIGHT) {
            // EKSEKUSI REBOOT!
            lcdFillScreen(&dev, BLACK);
            rootx_print_text(25, 30, "REBOOTING...", WHITE, BLACK);
            lcdDrawFinish(&dev);
            
            vTaskDelay(pdMS_TO_TICKS(500)); // Kasih jeda dikit biar OLED sempet nulis
            esp_restart(); // Perintah sakti buat restart ESP32
        } 
        else if (btn == BTN_LEFT) {
            appMode = 0; // Balik ke Menu Utama
        }
        
        lastPress = input_millis();
        return;
    }
        if (appMode == 16) { // SD CARD MANAGER
        extern int sdActionIdx;
        extern int sdState;

        if (sdState == 0) { // Di Dashboard
            if (btn == BTN_LEFT) {
                sdActionIdx--;
                if (sdActionIdx < 0) sdActionIdx = 2; // Looping ke Format
            } 
            else if (btn == BTN_RIGHT) {
                sdActionIdx++;
                if (sdActionIdx > 2) sdActionIdx = 0; // Looping ke Exit
            } 
            else if (btn == BTN_OK) {
                if (sdActionIdx == 0) {
                    appMode = 0; // KELUAR
                } 
                else if (sdActionIdx == 1) {
                    // MASUK KE FILE EXPLORER
                    extern bool isFileExpInit;
                    isFileExpInit = false; // Paksa scan ulang
                    appMode = 17; 
                } 

                else if (sdActionIdx == 2) {
                    sdState = 1; // MASUK KE KONFIRMASI FORMAT
                }
            }
        } 
        else if (sdState == 1) { // Layar Konfirmasi Format
            if (btn == BTN_LEFT) {
                sdState = 0; // Batal, balik ke Dashboard
            } 
            else if (btn == BTN_OK) {
                // EKSEKUSI FORMAT
                sdState = 2; // Tampilin layar "Formatting..."
                // (Masa depan: Panggil fungsi format ESP-IDF F_MKFS disini)
                
                // Kasih delay buatan sementara biar keliatan kerja (hapus ini ntar kalo fungsi format aslinya udah dipasang)
                vTaskDelay(pdMS_TO_TICKS(1500)); 
                
                sdState = 0; // Balik ke dashboard setelah selesai
                sdActionIdx = 0; // Kursor balik ke Exit biar aman
            }
        }

        lastPress = input_millis();
        return;
    }
    
        if (appMode == 17) { // FILE EXPLORER
        extern int sdFileCursor, sdFileScroll, sdTotalFiles, sdFileState;
        extern bool isFileExpInit;
        extern char sdFileNames[MAX_FILES][32];
        struct stat st;
        char full_path[512];
        extern char currentPath[256];

        if (sdTotalFiles == 0) {
            if (btn == BTN_LEFT || btn == BTN_OK) {
                appMode = 16; // Balik ke SD Manager
            }
        }
        else {
            if (sdFileState == 0) { // MODE BROWSE
                if (btn == BTN_DOWN) {
                    if (sdFileCursor < sdTotalFiles - 1) {
                        sdFileCursor++;
                        // Logika Scrolling kebawah
                        if (sdFileCursor >= sdFileScroll + 5) sdFileScroll++;
                    }
                } 
                else if (btn == BTN_UP) {
                    if (sdFileCursor > 0) {
                        sdFileCursor--;
                        // Logika Scrolling keatas
                        if (sdFileCursor < sdFileScroll) sdFileScroll--;
                    }
                } 
                                else if (btn == BTN_OK) {
                    // --- LOGIKA CEK FOLDER / FILE ---
                    
                    
                    
                    
                    // Gabungin path sekarang dengan nama file/folder yang dipilih
                    snprintf(full_path, sizeof(full_path), "%s/%s", currentPath, sdFileNames[sdFileCursor]);
                    
                    if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                        // JIKA INI FOLDER: Masuk ke folder itu
                        strcpy(currentPath, full_path);
                        sdFileCursor = 0; // Reset kursor ke atas
                        sdFileScroll = 0;
                        isFileExpInit = false; // Paksa scan isi folder baru
                    } else {
                        // JIKA INI FILE BIASA: Baru munculin minta konfirmasi delete/action
                        sdFileState = 1; 
                    }
                }
                else if (btn == BTN_LEFT) {
                    
                    // --- LOGIKA KEMBALI / MUNDUR FOLDER ---
                    if (strcmp(currentPath, "/sdcard") == 0) {
                        appMode = 16; // Kalau udah di paling luar, balik ke SD Manager Menu
                    } else {
                        // Kalau lagi di dalem folder, mundur 1 langkah
                        char *lastSlash = strrchr(currentPath, '/');
                        if (lastSlash != NULL) {
                            *lastSlash = '\0'; // Potong path-nya
                            if (strlen(currentPath) == 0) strcpy(currentPath, "/sdcard"); // Jaga-jaga
                        }
                        sdFileCursor = 0;
                        sdFileScroll = 0;
                        isFileExpInit = false; // Paksa scan ulang setelah mundur
                    }
                }

            }
            else if (sdFileState == 1) { // MODE KONFIRMASI DELETE
                if (btn == BTN_LEFT) {
                    sdFileState = 0; // Batal delete
                } 
                               else if (btn == BTN_OK) {
                    // EKSEKUSI HAPUS FILE!
                    
                    
                    snprintf(full_path, sizeof(full_path), "%s/%s", currentPath, sdFileNames[sdFileCursor]); // <-- Ganti bagian ini biar ngikutin folder
                    unlink(full_path); 
                    
                    isFileExpInit = false; 
                    sdFileState = 0; // Balik ke mode browse setelah hapus
                }

            }
        }
        
        lastPress = input_millis();
        return;
    }
        if (appMode == 18) { // TV-B-GONE MENU
        extern int tvbgoneState, tvbgoneMenuIdx;

        if (tvbgoneState == 0) { // Lagi Pilih Menu
            if (btn == BTN_UP) {
                if (tvbgoneMenuIdx > 0) tvbgoneMenuIdx--;
            } 
            else if (btn == BTN_DOWN) {
                if (tvbgoneMenuIdx < 2) tvbgoneMenuIdx++;
            } 
            else if (btn == BTN_LEFT) {
                appMode = 0; // Keluar ke Menu Utama
            } 
            else if (btn == BTN_OK) {
                // START PENEMBAKAN!
                tvbgoneState = 1; 
                
                // Bikin pekerja bayangan (FreeRTOS Task) buat nembak
                if (tvbgoneTaskHandle == NULL) {
                    xTaskCreate(tvbgone_fire_task, "tvbgone_task", 8192, NULL, 5, &tvbgoneTaskHandle);
                }
            }
        } 
        else if (tvbgoneState == 1) { // Lagi Nembak
            if (btn == BTN_LEFT || btn == BTN_OK) {
                // STOP PENEMBAKAN!
                tvbgoneState = 0; // Ini bakal ngasih tau Task buat bunuh diri (goto end_task)
            }
        }

        lastPress = input_millis();
        return;
    }
    

    if (appMode == MODE_SAVED_REMOTE) {
        if (currentIRSavedState == IR_SAVED_STATE_LIST) {
            if (btn == BTN_DOWN) {
                if (savedRemoteIndex < totalSavedRemotes - 1) savedRemoteIndex++;
            } 
            else if (btn == BTN_UP) {
                if (savedRemoteIndex > 0) savedRemoteIndex--;
            } 
            else if (btn == BTN_OK || btn == BTN_RIGHT) {
                if (totalSavedRemotes > 0) {
                    currentIRSavedState = IR_SAVED_STATE_ACTION;
                    actionMenuIndex = 0; 
                }
            } 
            else if (btn == BTN_LEFT) { 
                appMode = 0; // Balik ke Submenu
            }
        }
        else if (currentIRSavedState == IR_SAVED_STATE_ACTION) {
            if (btn == BTN_DOWN || btn == BTN_UP) {
                actionMenuIndex = !actionMenuIndex; 
            } 
            else if (btn == BTN_LEFT) { 
                currentIRSavedState = IR_SAVED_STATE_LIST; 
            } 
            else if (btn == BTN_OK) {
                if (actionMenuIndex == 0) {
                    currentIRSavedState = IR_SAVED_STATE_SENDING;
                    tampilkanMenuSavedIR(); 
                    transmit_ir_raw(listSavedRemotes[savedRemoteIndex].pulses, listSavedRemotes[savedRemoteIndex].num_pulses);
                    vTaskDelay(pdMS_TO_TICKS(500)); 
                    currentIRSavedState = IR_SAVED_STATE_ACTION; 
                } 
                else if (actionMenuIndex == 1) {
                    hapus_remote_di_sd(savedRemoteIndex); 
                    loadSavedRemotes(); 
                    if (savedRemoteIndex >= totalSavedRemotes) savedRemoteIndex = totalSavedRemotes - 1;
                    if (savedRemoteIndex < 0) savedRemoteIndex = 0;
                    currentIRSavedState = IR_SAVED_STATE_LIST; 
                }
            }
        }
        lastPress = input_millis();
        return; // <--- KUNCI SAKTI 
    }

    // --- HELPER LAINNYA (UDAH AMAN KARENA ADA RETURN) ---
    if (appMode == 1) { handleNavigasiScanner(btn); lastPress = input_millis(); return; }
    if (appMode == 2) { handleNavigasiDeauth(btn);  lastPress = input_millis(); return; }
    if (appMode == 4) { handleNavigasiSpam(btn);    lastPress = input_millis(); return; } 
    if (appMode == 5) { handleNavigasiScanSta(btn); lastPress = input_millis(); return; }
    if (appMode == 8) { handleEvilTwinInput(btn);   lastPress = input_millis(); return; }
    if (appMode == 11){ handleDinoInput(btn);       lastPress = input_millis(); return; }
    if (appMode == 13) { handleTetrisInput(btn);  lastPress = input_millis();  return;  }
    
    
    if (appMode == 6) {
        if (btn == BTN_LEFT) { scannerState = 4;  appMode = 1; triggerTrack = false; }
        lastPress = input_millis();
        return;
    }
    
    if (appMode == 7) {
        if (btn == BTN_LEFT) { 
            esp_wifi_stop(); 
            isDeauthSta = false;
            appMode = 5; 
        }
        lastPress = input_millis();
        return;
    }

    if (appMode == 3) {
        if (btn == BTN_UP) {
            // Mentokin sampai batas maksimal PWM 8-bit (255)
            if (brightnessValue <= 245) brightnessValue += 10;
            else brightnessValue = 255; 
            
            setOledBrightness(brightnessValue);
            tampilkanBrightness(); // Langsung render ulang layarnya biar bar-nya ikut naik
        }
        else if (btn == BTN_DOWN) {
            // Batas bawah biar gak mati total lampunya (minimal 5 atau 10)
            if (brightnessValue >= 15) brightnessValue -= 10;
            else brightnessValue = 5; 
            
            setOledBrightness(brightnessValue);
            tampilkanBrightness(); // Langsung render ulang layarnya biar bar-nya ikut turun
        }
        else if (btn == BTN_LEFT) {
            appMode = 0; // Balik ke menu utama
        }
        
        lastPress = input_millis();
        return;
    }


    // ==========================================
    // 3. LOGIKA MENU UTAMA (APPMODE == 0)
    // ==========================================
    // DIBUNGKUS IF(0) BIAR MENU LAIN GAK NGERUSAK KURSOR DI SINI!

    if (appMode == 0) {
        if (!inSubMenu) {
            if (btn == BTN_RIGHT) {
                currentMenu = (currentMenu + 1) % 5; 
            }
            else if (btn == BTN_LEFT) {
                currentMenu = (currentMenu - 1 + 5) % 5; 
            }
            else if (btn == BTN_OK) {
                inSubMenu = true; 
                currentSub = 0;   
                topMenu = 0;
            }
        } 
        else { // DI DALAM LIST SUBMENU
            if (btn == BTN_DOWN) {
                int limitMenu = 0; 
                if(currentMenu == 0)      limitMenu = 4; 
                else if(currentMenu == 1) limitMenu = 3;
                else if(currentMenu == 2) limitMenu = 5;
                else if(currentMenu == 3) limitMenu = 4;
                else limitMenu = 3;

                if (currentSub < (limitMenu - 1)) { 
                    currentSub++;
                    if (currentSub >= topMenu + 5) topMenu++;
                }
            }
            else if (btn == BTN_UP) {
                if (currentSub > 0) {
                    currentSub--;
                    if (currentSub < topMenu) topMenu--;
                }
            }
            else if (btn == BTN_LEFT) {
                inSubMenu = false; // BALIK KE LOGO RootX
            }
            else if (btn == BTN_OK) {
                if (currentMenu == 0 && currentSub == 0) {
                    appMode = 1;      
                    scannerState = 0; 
                } else if (currentMenu == 0 && currentSub == 1) {
                    appMode = 1;
                    scannerState = 2;     
                    cursorInScanner = 0;  
                    scrollPosScanner = 0; 
                } else if (currentMenu == 3 && currentSub == 0) { 
                    appMode = 3; 
                } else if (currentMenu == 2 && currentSub == 0) { 
                    appMode = MODE_IR_SNIFFER; 
                    currentIRState = IR_STATE_CONFIRM; // Reset tiap masuk menu
                } else if (currentMenu == 2 && currentSub == 4) { 
                    loadSavedRemotes(); // Baca memori SD
                    currentIRSavedState = IR_SAVED_STATE_LIST; 
                    savedRemoteIndex = 0; 
                    appMode = MODE_SAVED_REMOTE; 
                } else if (currentMenu == 0 && currentSub == 2) {
                    aktifModeSpam = 1; 
                    appMode = 4;       
                    spamState = 0;
                } else if (currentMenu == 0 && currentSub == 3) {
                    aktifModeSpam = 2; 
                    appMode = 4;
                    spamState = 0;
                } else if (currentMenu == 4 && currentSub == 0) {
                    appMode = 11;
                } else if (currentMenu == 4 && currentSub == 1) {
                    appMode = 12;
                    } else if (currentMenu == 4 && currentSub == 2) {
                    appMode = 13;
                    } else if (currentMenu == 3 && currentSub == 2) {
                    appMode = 14;
                } else if (currentMenu == 3 && currentSub == 3) {
                    appMode = 15;
                } else if (currentMenu == 3 && currentSub == 1) {
                    appMode = 16;
                } else if (currentMenu == 2 && currentSub == 1) {
                    appMode = 18;
                }
            }
        }
        lastPress = input_millis();
        return;
    }
}












TaskHandle_t tvbgoneTaskHandle = NULL;

void tvbgone_fire_task(void *pvParameters) {
    extern int tvbgoneMenuIdx, tvbgoneProgress, tvbgoneTotal, tvbgoneState;
    
    tvbgoneProgress = 0;
    // Set Target Total
    if (tvbgoneMenuIdx == 0) tvbgoneTotal = num_NAcodes;
    else if (tvbgoneMenuIdx == 1) tvbgoneTotal = num_EUcodes;
    else tvbgoneTotal = num_NAcodes + num_EUcodes;

    // 1. HAJAR REGION NA / ASIA (Jika Dipilih atau Mode ALL)
    if (tvbgoneMenuIdx == 0 || tvbgoneMenuIdx == 2) {
        for (int i = 0; i < num_NAcodes; i++) {
            send_tvbgone_code(NApowerCodes[i]); // Tembak!
            tvbgoneProgress++;
            vTaskDelay(pdMS_TO_TICKS(250));     // Jeda 250ms biar sensor TV gak nge-lag
            
            // Kalo state berubah (berarti user pencet tombol Exit/Stop)
            if (tvbgoneState == 0) goto end_task; 
        }
    }

    // 2. HAJAR REGION EUROPE (Jika Dipilih atau Mode ALL)
    if (tvbgoneMenuIdx == 1 || tvbgoneMenuIdx == 2) {
        for (int i = 0; i < num_EUcodes; i++) {
            send_tvbgone_code(EUpowerCodes[i]); // Tembak!
            tvbgoneProgress++;
            vTaskDelay(pdMS_TO_TICKS(250));
            
            if (tvbgoneState == 0) goto end_task; 
        }
    }

end_task:
    tvbgoneState = 0; // Balik ke Menu kalau udah selesai
    tvbgoneTaskHandle = NULL;
    vTaskDelete(NULL); // Bunuh diri task-nya
}



// ==========================================
// LOGIKA NAVIGASI KHUSUS 
// ==========================================


void handleNavigasiScanner(int btn) {
    if (scannerState == 0) {
        if (btn == BTN_LEFT) appMode = 0; 
        else if (btn == BTN_RIGHT || btn == BTN_OK) { 
            scannerState = 1;     
            triggerScan = true;   
            scanDone = false;     
            cursorInScanner = 0;  
            scrollPosScanner = 0; 
        }
    }
    else if (scannerState == 1) {
        if (btn == BTN_LEFT) scannerState = 0; 
    }
    else if (scannerState == 2) {
        if (btn == BTN_UP) {
            if (cursorInScanner > 0) cursorInScanner--;
            else if (scrollPosScanner > 0) scrollPosScanner--;
        } 
        else if (btn == BTN_DOWN) {
            if (cursorInScanner < 2 && (scrollPosScanner + cursorInScanner) < (totalWiFi - 1)) cursorInScanner++;
            else if ((scrollPosScanner + 3) < totalWiFi) scrollPosScanner++;
        }
        else if (btn == BTN_OK) {
            if (totalWiFi > 0) {
                targetLockedIdx = scrollPosScanner + cursorInScanner; 
                targetTerkunci = listWiFi[targetLockedIdx];
                adaTarget = true; 
               // triggerTrackWifi = true;
                scannerState = 4;   
                contextCursor = 0;  
            }
        }
        else if (btn == BTN_LEFT) {
            scannerState = 0; 
            appMode = 0;      
        }
    }
    else if (scannerState == 3) {
        if (btn == BTN_LEFT) scannerState = 4; 
    }
      else if (scannerState == 4) {
        // Navigasi Naik/Turun
        if (btn == BTN_UP) {
            contextCursor = (contextCursor > 0) ? contextCursor - 1 : 4; // Batas atas ke 2
        } 
        else if (btn == BTN_DOWN) {
            contextCursor = (contextCursor < 4) ? contextCursor + 1 : 0; // Batas bawah ke 2
        }
        
        else if (btn == BTN_OK) {
        if (contextCursor == 0) { appMode = 2; deauthState = 0; } 
        else if (contextCursor == 1) {
           appMode = 8;
        }
        else if (contextCursor == 2) { appMode = 5; scannerStateSta = 0; contextCursor = 0; }
        else if (contextCursor == 3) { // TRACK
            appMode = 6; 
            triggerTrack = true; // Nyalain update RSSI
        }
        else if (contextCursor == 4) { scannerState = 3; }
    }
        else if (btn == BTN_LEFT) scannerState = 2; 
    }

}




void handleNavigasiScanSta(int btn) {
    if (scannerStateSta == 0) {
        if (btn == BTN_LEFT) appMode = 1; // Balik ke WiFi Scanner
        else if (btn == BTN_RIGHT || btn == BTN_OK) {
            totalStation = 0;
            scannerStateSta = 1;
            triggerScanSta = true;
            scanStaDone = false;
        }
    } 
    else if (scannerStateSta == 2) {
        if (btn == BTN_LEFT) { appMode = 1;  triggerScanSta = false; }// Mau scan lagi
        else if (btn == BTN_UP) {
            if (cursorInScanSta > 0) cursorInScanSta--; 
            else if (scrollPosScanner > 0) scrollPosScanner--;
        } 
        else if (btn == BTN_DOWN) {
            if (cursorInScanSta < 2 && (scrollPosScanner + cursorInScanSta) < (totalStation - 1)) cursorInScanSta++; 
            else if ((scrollPosScanner + 3) < totalStation) scrollPosScanner++;
        }
        else if (btn == BTN_OK) {
            if (totalStation > 0) {
                targetLockedIdx = scrollPosScanner + cursorInScanSta; 
                targetSta = listStation[targetLockedIdx];
                adaTargetSta = true; 
                scannerStateSta = 4;   
                contextCursor = 0;  
            }
        }
    } 
    else if (scannerStateSta == 3) {
        if (btn == BTN_LEFT || btn == BTN_OK) scannerStateSta = 4; 
    } 
    else if (scannerStateSta == 4) {
        if (btn == BTN_UP) contextCursor = (contextCursor > 0) ? contextCursor - 1 : 1; 
        else if (btn == BTN_DOWN) contextCursor = (contextCursor < 1) ? contextCursor + 1 : 0;
        else if (btn == BTN_OK) {
            if (contextCursor == 0) {
                // MULAI ATTACK KE HP
                isDeauthSta = true; 
                scannerStateSta = 0;
                appMode = 7; // Pindah layar ke animasi Deauth
            } 
            else if (contextCursor == 1) {
                scannerStateSta = 3; // Lihat Detail
            }
        }
        else if (btn == BTN_LEFT) { scannerStateSta = 2;}
    }
}


void handleNavigasiDeauth(int btn) {
    if (deauthState == 0) { 
        if (btn == BTN_LEFT) appMode = 0; 
        else if (btn == BTN_RIGHT || btn == BTN_OK) { 
            deauthState = 1;
            isDeauthing = true;
        }
    } 
    else if (deauthState == 1) { 
        if (btn == BTN_LEFT) { 
            esp_wifi_stop(); 
            isDeauthing = false;
            deauthState = 0;
            appMode = 1; 
        }
    }
}

// --- TARUH INI DI LUAR FUNGSI (Di atas handleInputPassword) ---


void handleNavigasiSpam(int btn) {
    if (spamState == 0) { 
        if (btn == BTN_RIGHT || btn == BTN_OK) { 
            spamState = 1;
            isSpamming = true; 
        } 
        else if (btn == BTN_LEFT) { 
            esp_wifi_stop(); 
            appMode = 0; 
            isSpamming = false;
            aktifModeSpam = 0; 
        }
    } 
    else if (spamState == 1) { 
        if (btn == BTN_LEFT) { 
            isSpamming = false;
            spamState = 0;
            appMode = 0; 
            aktifModeSpam = 0;
        }
    }
}


void handleEvilTwinInput(int btn) {
    if (evilTwinState == 0) {
        if (btn == BTN_RIGHT || btn == BTN_OK) {
            triggerEvilTwin = true;
            
        } else if (btn == BTN_LEFT) {
            appMode = 1; // Balik ke menu scanner
        }
    } else if (evilTwinState == 2) {
        if (btn == BTN_LEFT || btn == BTN_OK) {
            isEvilTwin = false;
            esp_wifi_stop();
            appMode = 1;
        } 
    } else if (evilTwinState == 1) { if (btn == BTN_LEFT) {
            appMode = 1; // Balik ke menu scanner
            isEvilTwin = false;
            esp_wifi_stop();
        }}
}


void handleDinoInput(int btn) {
    if (dinoState == 0) {
        if ((btn == BTN_OK || btn == BTN_UP) && !isJumping) {
            dinoVy = -8.5; // Lompatan disesuaikan buat berat Dino 24px
            isJumping = true;
        }
    } else {
        if (btn == BTN_OK) { 
            // Reset Game
            dinoScore = 0;
            rawScore = 0;
            gameSpeed = 3.0;
            obstacleX = 128;
            dinoY = 36;
            dinoVy = 0;
            isJumping = false;
            dinoState = 0;
            endTimer = 0;
        }
    }

    // Tombol Keluar (Kiri)
    if (btn == BTN_LEFT) {
        lcdInversionOff(&dev); // Pastiin layar gak nyangkut item
        appMode = 0; 
        dinoScore = 0; rawScore = 0;
        obstacleX = 128;
        dinoState = 0;
    }
}





