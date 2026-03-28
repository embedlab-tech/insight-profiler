/**
 * @file ina228.cpp
 * @brief INA228 Driver Implementation (Stub)
 *
 * Copyright (c) 2026 EmbedLab-Tech. Licensed under MIT.
 */

#include "ina228.h"
#include "esp_log.h"

static const char *TAG = "ina228";

namespace embedlab {

INA228::INA228(const Config &config) : config_(config) {}

INA228::~INA228()
{
    if (dev_handle_) {
        i2c_master_bus_rm_device(dev_handle_);
    }
}

esp_err_t INA228::init()
{
    ESP_LOGI(TAG, "Initializing INA228 at address 0x%02X", config_.device_address);

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config_.device_address,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(config_.bus_handle, &dev_cfg, &dev_handle_),
        TAG, "Failed to add I2C device"
    );

    current_lsb_ = config_.max_expected_current_a / (1 << 19);

    // TODO: Write calibration register (REG 0x05)
    // TODO: Configure ADC conversion time and averaging

    ESP_LOGI(TAG, "INA228 initialized (shunt=%.3f Ohm, current_lsb=%.6f A)",
             config_.shunt_resistance_ohm, current_lsb_);
    return ESP_OK;
}

esp_err_t INA228::read(Measurement &out)
{
    // TODO: Implement actual register reads
    out = {0.0f, 0.0f, 0.0f, 0.0f};
    return ESP_OK;
}

esp_err_t INA228::reset()
{
    ESP_LOGI(TAG, "Resetting INA228");
    // TODO: Write reset bit to configuration register
    return ESP_OK;
}

esp_err_t INA228::write_register(uint8_t reg, uint16_t value)
{
    uint8_t buf[3] = {reg, static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value & 0xFF)};
    return i2c_master_transmit(dev_handle_, buf, sizeof(buf), -1);
}

esp_err_t INA228::read_register(uint8_t reg, uint16_t &value)
{
    uint8_t buf[2] = {};
    ESP_RETURN_ON_ERROR(
        i2c_master_transmit_receive(dev_handle_, &reg, 1, buf, 2, -1),
        TAG, "I2C read failed"
    );
    value = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
    return ESP_OK;
}

}  // namespace embedlab
