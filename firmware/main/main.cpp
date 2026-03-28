/**
 * @file main.cpp
 * @brief Insight Profiler — ESP32-S3 Firmware Entry Point
 *
 * Initializes Native USB CDC for high-speed data streaming and sets up
 * ADC continuous sampling for real-time power measurement.
 *
 * Copyright (c) 2026 EmbedLab-Tech. Licensed under MIT.
 */

#include <cstdio>
#include <cstring>
#include <cinttypes>

#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "esp_adc/adc_continuous.h"

static const char *TAG = "insight_profiler";

// ---------------------------------------------------------------------------
// USB CDC Configuration
// ---------------------------------------------------------------------------

static uint8_t rx_buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];

/**
 * @brief Callback invoked when data is received over USB CDC.
 */
static void cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    size_t rx_size = 0;
    esp_err_t ret = tinyusb_cdcacm_read(
        static_cast<tinyusb_cdcacm_itf_t>(itf), rx_buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size
    );
    if (ret == ESP_OK && rx_size > 0) {
        rx_buf[rx_size] = '\0';
        ESP_LOGI(TAG, "CDC RX [%d bytes]: %s", rx_size, rx_buf);
    }
}

/**
 * @brief Initialize TinyUSB stack with CDC ACM for data streaming.
 */
static esp_err_t usb_cdc_init(void)
{
    ESP_LOGI(TAG, "Initializing USB CDC (Native USB)");

    const tinyusb_config_t tusb_cfg = {
        .string_descriptor = nullptr,
        .string_descriptor_count = 0,
        .external_phy = false,
        .configuration_descriptor = nullptr,
        .self_powered = false,
        .vbus_monitor_io = 0,
    };
    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&tusb_cfg), TAG, "TinyUSB install failed");

    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = 64,
        .callback_rx = &cdc_rx_callback,
        .callback_rx_wanted_char = nullptr,
        .callback_line_state_changed = nullptr,
        .callback_line_coding_changed = nullptr,
    };
    ESP_RETURN_ON_ERROR(tusb_cdc_acm_init(&acm_cfg), TAG, "CDC ACM init failed");

    ESP_LOGI(TAG, "USB CDC initialized — ready to stream");
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// ADC Continuous Sampling (Placeholder)
// ---------------------------------------------------------------------------

static adc_continuous_handle_t adc_handle = nullptr;

/**
 * @brief Initialize ADC in continuous mode for power measurement.
 *
 * TODO: Configure actual ADC channels connected to the INA228 output
 *       or direct shunt measurement path.
 */
static esp_err_t adc_continuous_init(void)
{
    ESP_LOGI(TAG, "Initializing ADC continuous sampling");

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = 256,
        .flags = { .flush_pool = true },
    };
    ESP_RETURN_ON_ERROR(
        adc_continuous_new_handle(&adc_config, &adc_handle),
        TAG, "ADC handle creation failed"
    );

    // Configure sampling pattern — channels to be defined per hardware rev
    adc_digi_pattern_config_t adc_pattern = {
        .atten = ADC_ATTEN_DB_12,
        .channel = ADC_CHANNEL_0,
        .unit = ADC_UNIT_1,
        .bit_width = ADC_BITWIDTH_12,
    };

    adc_continuous_config_t dig_cfg = {
        .pattern_num = 1,
        .adc_pattern = &adc_pattern,
        .sample_freq_hz = 20000,   // 20 kHz — adjust per profiling needs
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };
    ESP_RETURN_ON_ERROR(
        adc_continuous_config(adc_handle, &dig_cfg),
        TAG, "ADC config failed"
    );

    ESP_LOGI(TAG, "ADC continuous sampling configured at %d Hz", dig_cfg.sample_freq_hz);
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Streaming Task
// ---------------------------------------------------------------------------

/**
 * @brief FreeRTOS task that reads ADC samples and streams them over USB CDC.
 */
static void streaming_task(void *arg)
{
    uint8_t result[256] = {};
    uint32_t ret_num = 0;

    ESP_LOGI(TAG, "Streaming task started");

    while (true) {
        esp_err_t ret = adc_continuous_read(adc_handle, result, sizeof(result), &ret_num, pdMS_TO_TICKS(100));
        if (ret == ESP_OK && ret_num > 0) {
            // TODO: Pack samples into a binary frame and send over CDC
            tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, result, ret_num);
            tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(10));
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ---------------------------------------------------------------------------
// Hello World — Chip & System Info
// ---------------------------------------------------------------------------

/**
 * @brief Print chip info, flash size, memory stats, and IDF version.
 *        Useful for verifying the firmware boots correctly on new hardware.
 */
static void print_system_info(void)
{
    ESP_LOGI(TAG, "=== Insight Profiler v0.1.0 — EmbedLab-Tech ===");
    ESP_LOGI(TAG, "ESP-IDF version: %s", esp_get_idf_version());

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "Chip: %s | Cores: %d | Rev: %d.%d",
             CONFIG_IDF_TARGET,
             chip_info.cores,
             chip_info.revision / 100,
             chip_info.revision % 100);

    ESP_LOGI(TAG, "Features:%s%s%s",
             (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? " WiFi" : "",
             (chip_info.features & CHIP_FEATURE_BLE) ? " BLE" : "",
             (chip_info.features & CHIP_FEATURE_IEEE802154) ? " 802.15.4" : "");

    uint32_t flash_size = 0;
    if (esp_flash_get_size(nullptr, &flash_size) == ESP_OK) {
        ESP_LOGI(TAG, "Flash: %" PRIu32 " MB (%s)",
                 flash_size / (1024 * 1024),
                 (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    }

    ESP_LOGI(TAG, "Free heap: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Min free heap: %" PRIu32 " bytes", esp_get_minimum_free_heap_size());
}

// ---------------------------------------------------------------------------
// Application Entry
// ---------------------------------------------------------------------------

extern "C" void app_main(void)
{
    print_system_info();

    // TODO: Uncomment when hardware is ready
    // ESP_ERROR_CHECK(usb_cdc_init());
    // ESP_ERROR_CHECK(adc_continuous_init());
    // ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
    // xTaskCreatePinnedToCore(streaming_task, "stream", 4096, nullptr, 5, nullptr, 1);

    // Hello world heartbeat — proves the firmware is alive
    int count = 0;
    while (true) {
        ESP_LOGI(TAG, "Hello from Insight Profiler! [%d] | heap: %" PRIu32,
                 count++, esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
