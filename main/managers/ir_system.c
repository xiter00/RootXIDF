#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_encoder.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "globals.h"




ir_read_state_t currentIRState = IR_STATE_CONFIRM;
ir_data_t last_ir_data = {0};
bool triggerReadIR = false;

// --- FUNGSI SD CARD (RAW FORMAT) ---
void simpan_ir_ke_sd() {
    // --- 1. NGINTIP SD CARD DULU BIAR RAM GAK AMNESIA ---
    loadSavedRemotes(); 

    FILE* f = fopen("/sdcard/ir_log.txt", "a");
    if (f == NULL) {
        ESP_LOGE("IR_SYSTEM", "Gagal buka SD Card!");
        return;
    }
    
    // Format: Nama | Jumlah Pulsa | Angka,Angka,Angka...
    // Karena udah di-load, totalSavedRemotes otomatis nyesuain isi SD Card!
    fprintf(f, "Remote_%d|%d|", totalSavedRemotes + 1, last_ir_data.num_pulses);
    
    for(int i = 0; i < last_ir_data.num_pulses; i++) {
        fprintf(f, "%d,", last_ir_data.pulses[i]);
    }
    fprintf(f, "\n");
    fclose(f);
    
    // --- 2. LOAD LAGI BIAR MENU "SAVED REMOTES" LANGSUNG UPDATE ---
    loadSavedRemotes(); 
}


void hapus_remote_di_sd(int index_target) {
    FILE* f = fopen("/sdcard/ir_log.txt", "r");
    FILE* f_temp = fopen("/sdcard/temp.txt", "w");
    if (!f || !f_temp) { if(f) fclose(f); if(f_temp) fclose(f_temp); return; }
    char line[1500]; int current_idx = 0;
    while (fgets(line, sizeof(line), f)) {
        if (current_idx != index_target) fprintf(f_temp, "%s", line);
        current_idx++;
    }
    fclose(f); fclose(f_temp);
    remove("/sdcard/ir_log.txt"); rename("/sdcard/temp.txt", "/sdcard/ir_log.txt");
    totalSavedRemotes--;
}

// --- RMT V5 ENGINE (RAW TX & RX) ---
void transmit_ir_raw(uint16_t* pulses, int num_pulses) {
    rmt_channel_handle_t tx_chan = NULL;
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, .resolution_hz = 1000000,
        .mem_block_symbols = 64, .gpio_num = IR_TX_PIN, .trans_queue_depth = 4,
    };
    rmt_new_tx_channel(&tx_chan_config, &tx_chan);
    rmt_carrier_config_t carrier_cfg = { .duty_cycle = 0.33, .frequency_hz = 38000 };
    rmt_apply_carrier(tx_chan, &carrier_cfg);
    rmt_encoder_handle_t copy_encoder = NULL;
    rmt_copy_encoder_config_t copy_encoder_config = {};
    rmt_new_copy_encoder(&copy_encoder_config, &copy_encoder);
    rmt_enable(tx_chan);

    // Rakit peluru dari array mentah
    rmt_symbol_word_t items[200];
    int sym_idx = 0;
    for (int i = 0; i < num_pulses; i += 2) {
        items[sym_idx].level0 = 1;
        items[sym_idx].duration0 = pulses[i];
        items[sym_idx].level1 = 0;
        items[sym_idx].duration1 = (i + 1 < num_pulses) ? pulses[i+1] : 5000;
        sym_idx++;
    }

    rmt_transmit_config_t tx_config = { .loop_count = 0 };
    rmt_transmit(tx_chan, copy_encoder, items, sym_idx * sizeof(rmt_symbol_word_t), &tx_config);
    rmt_tx_wait_all_done(tx_chan, -1);
    rmt_disable(tx_chan); rmt_del_encoder(copy_encoder); rmt_del_channel(tx_chan);
}

// Fungsi nyedot data mentah
void extract_raw_pulses(rmt_symbol_word_t* symbols, size_t num_symbols) {
    int p = 0;
    for (int i = 0; i < num_symbols && p < 200; i++) {
        if (symbols[i].duration0 > 0) last_ir_data.pulses[p++] = symbols[i].duration0;
        if (symbols[i].duration1 > 0 && p < 200) last_ir_data.pulses[p++] = symbols[i].duration1;
    }
    last_ir_data.num_pulses = p;
}

static bool rmt_rx_done_cb(rmt_channel_handle_t rx_chan, const rmt_rx_done_event_data_t *edata, void *user_ctx) {
    BaseType_t high_task_wakeup = pdFALSE;
    xQueueSendFromISR((QueueHandle_t)user_ctx, edata, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

void ir_sniffer_task(void *pvParameter) {
    QueueHandle_t rx_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    rmt_channel_handle_t rx_chan = NULL;
    bool rmt_is_enabled = false;
    bool is_receiving = false;

    rmt_rx_channel_config_t rx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, .resolution_hz = 1000000, 
        .mem_block_symbols = 64, .gpio_num = IR_RX_PIN, .flags.invert_in = true, 
    };
    if (rmt_new_rx_channel(&rx_chan_config, &rx_chan) != ESP_OK) { vTaskDelete(NULL); return; }

    rmt_rx_event_callbacks_t cbs = { .on_recv_done = rmt_rx_done_cb };
    rmt_rx_register_event_callbacks(rx_chan, &cbs, rx_queue);

    rmt_symbol_word_t raw_symbols[200]; 
    
    // --- INI OBAT SAKTINYA COK! JANGAN ILANG LAGI ---
    rmt_receive_config_t receive_config = { 
        .signal_range_min_ns = 1250,       
        .signal_range_max_ns = 12000000,   // Timeout 12ms. Kalo diem 12ms, proses!
    }; 

    for(;;) {
        if (currentIRState == IR_STATE_WAITING && triggerReadIR) {
            if (!rmt_is_enabled) {
                rmt_enable(rx_chan);
                rmt_is_enabled = true;
                is_receiving = false;
                xQueueReset(rx_queue); 
                ESP_LOGW("IR_SYSTEM", "--> RMT NYALA! TEMBAK REMOT SEKARANG! <--");
            }

            if (!is_receiving) {
                if (rmt_receive(rx_chan, raw_symbols, sizeof(raw_symbols), &receive_config) == ESP_OK) {
                    is_receiving = true; 
                } else { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
            }

            rmt_rx_done_event_data_t rx_data;
            if (xQueueReceive(rx_queue, &rx_data, pdMS_TO_TICKS(200)) == pdTRUE) {
                is_receiving = false; 
                
                ESP_LOGW("IR_SYSTEM", "--> DAPET %d KEDIPAN! <--", rx_data.num_symbols);
                
                extract_raw_pulses(raw_symbols, rx_data.num_symbols);
                
                if (last_ir_data.num_pulses > 10) { 
                    ESP_LOGW("IR_SYSTEM", "SIMPEN KE SD CARD!");
                    simpan_ir_ke_sd();
                    currentIRState = IR_STATE_RESULT;
                    triggerReadIR = false;
                    rmt_disable(rx_chan); rmt_is_enabled = false;
                } else {
                    ESP_LOGE("IR_SYSTEM", "CUMA NOISE (SAMPAH)");
                }
            }
        } else {
            if (rmt_is_enabled) { rmt_disable(rx_chan); rmt_is_enabled = false; is_receiving = false; }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}


void init_ir_system() { 
    gpio_set_pull_mode(IR_RX_PIN, GPIO_PULLUP_ONLY); // Anti hantu
    
    xTaskCreatePinnedToCore(
    ir_sniffer_task, 
    "ir_sniff", 
    8192,         // Naikin angkanya di sini (dalam Bytes)
    NULL, 
    3,            // Prioritas jangan ketinggian biar nggak ganggu sistem
    NULL, 
    0             // Tetep di Core 0 sesuai kemauan lu
);
}
