/**
 * @file main.cpp
 * @brief Insight Profiler — ESP32-S3 Firmware Entry Point
 *
 * Reads power measurements from the INA228 over I2C and streams
 * 16-byte binary frames over USB CDC at ~1 kHz.
 *
 * Frame layout (little-endian):
 *   [0:4]   uint32  timestamp_us
 *   [4:8]   float32 bus_voltage_v
 *   [8:12]  float32 current_ma
 *   [12:16] float32 power_mw
 *
 * Copyright (c) 2026 EmbedLab-Tech. Licensed under MIT.
 */

#include <cstdio>
#include <cstring>
#include <cinttypes>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_idf_version.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"

#include "ina228.h"

static const char *TAG = "insight_profiler";

// ---------------------------------------------------------------------------
// Hardware Pin Configuration — adjust to match PCB layout
// ---------------------------------------------------------------------------

static constexpr gpio_num_t I2C_SDA_PIN = GPIO_NUM_8;
static constexpr gpio_num_t I2C_SCL_PIN = GPIO_NUM_9;

// ---------------------------------------------------------------------------
// USB CDC
// ---------------------------------------------------------------------------

static uint8_t rx_buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];

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

static esp_err_t usb_cdc_init(void)
{
    ESP_LOGI(TAG, "Initializing USB CDC (Native USB)");

    const tinyusb_config_t tusb_cfg = {
        .string_descriptor       = nullptr,
        .string_descriptor_count = 0,
        .external_phy            = false,
        .configuration_descriptor = nullptr,
        .self_powered            = false,
        .vbus_monitor_io         = 0,
    };
    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&tusb_cfg), TAG, "TinyUSB install failed");

    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev               = TINYUSB_USBDEV_0,
        .cdc_port              = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz      = 64,
        .callback_rx           = &cdc_rx_callback,
        .callback_rx_wanted_char        = nullptr,
        .callback_line_state_changed    = nullptr,
        .callback_line_coding_changed   = nullptr,
    };
    ESP_RETURN_ON_ERROR(tusb_cdc_acm_init(&acm_cfg), TAG, "CDC ACM init failed");

    ESP_LOGI(TAG, "USB CDC initialized — ready to stream");
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// I2C Master Bus
// ---------------------------------------------------------------------------

static i2c_master_bus_handle_t i2c_bus = nullptr;

static esp_err_t i2c_master_init(void)
{
    ESP_LOGI(TAG, "Initializing I2C master (SDA=GPIO%d, SCL=GPIO%d)", I2C_SDA_PIN, I2C_SCL_PIN);

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port          = I2C_NUM_0,
        .sda_io_num        = I2C_SDA_PIN,
        .scl_io_num        = I2C_SCL_PIN,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority     = 0,
        .trans_queue_depth = 0,
        .flags             = { .enable_internal_pullup = true },
    };
    ESP_RETURN_ON_ERROR(
        i2c_new_master_bus(&bus_cfg, &i2c_bus),
        TAG, "I2C master bus init failed"
    );

    ESP_LOGI(TAG, "I2C master bus ready");
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Streaming Task
// ---------------------------------------------------------------------------

struct __attribute__((packed)) SampleFrame {
    uint32_t timestamp_us;
    float    bus_voltage_v;
    float    current_ma;
    float    power_mw;
};
static_assert(sizeof(SampleFrame) == 16, "SampleFrame must be 16 bytes");

static void streaming_task(void *arg)
{
    auto *sensor = static_cast<embedlab::INA228 *>(arg);
    ESP_LOGI(TAG, "Streaming task started");

    while (true) {
        embedlab::INA228::Measurement m;
        if (sensor->read(m) == ESP_OK) {
            SampleFrame frame = {
                .timestamp_us  = static_cast<uint32_t>(esp_timer_get_time()),
                .bus_voltage_v = m.bus_voltage_v,
                .current_ma    = m.current_ma,
                .power_mw      = m.power_mw,
            };
            tinyusb_cdcacm_write_queue(
                TINYUSB_CDC_ACM_0,
                reinterpret_cast<uint8_t *>(&frame),
                sizeof(frame)
            );
            tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(10));
        }
        vTaskDelay(pdMS_TO_TICKS(1));  // ~1 kHz; INA228 configured for 140 µs conversion
    }
}

// ---------------------------------------------------------------------------
// System Info
// ---------------------------------------------------------------------------

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
             (chip_info.features & CHIP_FEATURE_BLE)      ? " BLE"  : "",
             (chip_info.features & CHIP_FEATURE_IEEE802154) ? " 802.15.4" : "");

    uint32_t flash_size = 0;
    if (esp_flash_get_size(nullptr, &flash_size) == ESP_OK) {
        ESP_LOGI(TAG, "Flash: %" PRIu32 " MB (%s)",
                 flash_size / (1024 * 1024),
                 (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    }

    ESP_LOGI(TAG, "Free heap: %" PRIu32 " bytes", esp_get_free_heap_size());
}

// ---------------------------------------------------------------------------
// Application Entry
// ---------------------------------------------------------------------------

extern "C" void app_main(void)
{
    print_system_info();

    ESP_ERROR_CHECK(usb_cdc_init());
    ESP_ERROR_CHECK(i2c_master_init());

    static embedlab::INA228 sensor(embedlab::INA228::Config{
        .bus_handle             = i2c_bus,
        .device_address         = embedlab::INA228::DEFAULT_ADDRESS,
        .shunt_resistance_ohm   = 0.010f,
        .max_expected_current_a = 3.2f,
    });
    ESP_ERROR_CHECK(sensor.init());

    xTaskCreatePinnedToCore(streaming_task, "stream", 4096, &sensor, 5, nullptr, 1);
}
