#include <stdio.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "globals.h"

// Konstanta Resistor Divider lu (440 Ohm dan 440 Ohm)


adc_oneshot_unit_handle_t adc1_handle;
adc_cali_handle_t adc1_cali_handle = NULL;
bool do_calibration = false;

// ==========================================
// FUNGSI 1: INIT ADC KALIBRASI (WAJIB BUAT ESP32-S3)
// ==========================================
static bool battery_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle) {
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, out_handle);
    return (ret == ESP_OK);
}

void init_battery() {
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_config1, &adc1_handle);

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12, 
    };
    // ADC_CHANNEL_0 = GPIO 1 di ESP32-S3
    adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &config);

    // Aktifin fitur Kalibrasi bawaan chip ESP32
    do_calibration = battery_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_12, &adc1_cali_handle);
}

// ==========================================
// FUNGSI 2: KURVA BATERAI LIPO 3.7V (LUKUP TABLE)
// ==========================================
// Ini rahasia biar dapet 99%, 98%, dst. Karena LiPo gak linier!
int get_lipo_percentage(float voltage) {
    if (voltage >= 4.20) return 100;
    if (voltage <= 3.30) return 0;

    // Titik-titik voltase LiPo pada umumnya (Discharge Curve)
    float volts[] = { 4.20, 4.10, 4.00, 3.90, 3.80, 3.70, 3.60, 3.50, 3.30 };
    int percs[]   = { 100,  90,   80,   60,   40,   20,   10,   5,    0 };

    for (int i = 0; i < 8; i++) {
        if (voltage <= volts[i] && voltage > volts[i+1]) {
            // Matematika Interpolasi (nyari nilai tengah antara titik-titik)
            float v_range = volts[i] - volts[i+1];
            float p_range = percs[i] - percs[i+1];
            float v_diff = voltage - volts[i+1];
            return percs[i+1] + (int)((v_diff / v_range) * p_range);
        }
    }
    return 0;
}

// ==========================================
// FUNGSI 3: EKSEKUSI PEMBACAAN BATERAI
// ==========================================
int read_battery_percentage() {
    int adc_raw = 0;
    uint32_t total_adc = 0; // Pake uint32 biar gak luber/overflow!

    // 1. MULTISAMPLING EXTREME: 30 kali tembak biar rata mulus!
    for(int i = 0; i < 30; i++) {
        adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &adc_raw);
        total_adc += adc_raw;
    }
    adc_raw = total_adc / 30;

    // 2. KONVERSI RAW KE MILLIVOLT PAKE KALIBRASI ESP32
    int voltage_mv = 0;
    if (do_calibration) {
        adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage_mv);
    } else {
        // Fallback kalau kalibrasi gagal nyala (Jaga-jaga)
        voltage_mv = (adc_raw * 3300) / 4095;
    }

    // 3. BALIKIN KE VOLTASE BATERAI ASLI (Berdasarkan Resistor 440 Ohm lu)
    // voltage_mv / 1000 buat dapet satuan Volt. Terus dikali 2.0 (rasio resistor divider)
    float real_voltage = ((float)voltage_mv / 1000.0) * VOLTAGE_DIVIDER_RATIO * CALIBRATION_FACTOR;

    // 4. KONVERSI KE PERSEN PAKE KURVA LIPO
    float currentPercent = (float)get_lipo_percentage(real_voltage);

    // 5. SUPER SMOOTHING (Anti Joget-Joget)
    static float smoothedPercent = -1.0; 
    if (smoothedPercent == -1.0) {
        smoothedPercent = currentPercent; // Tembakan pertama kali alat nyala
    } else {
        // Kita kunci 98% data lama, dan cuma ambil 2% data baru. 
        // Efeknya: Turunnya bakal persis kayak HP, perlahan dan kalem.
        smoothedPercent = (smoothedPercent * 0.98) + (currentPercent * 0.02);
    }

    batteryPercent = (int)smoothedPercent;
    return batteryPercent;
}
