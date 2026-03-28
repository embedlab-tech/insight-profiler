/**
 * @file ina228.cpp
 * @brief INA228 Driver Implementation
 *
 * Copyright (c) 2026 EmbedLab-Tech. Licensed under MIT.
 */

#include "ina228.h"
#include "esp_log.h"

static const char *TAG = "ina228";

namespace embedlab {

// Sign-extend a 20-bit two's complement value to int32_t.
static inline int32_t sign_extend_20(uint32_t val)
{
    return (val & 0x80000u) ? static_cast<int32_t>(val | 0xFFF00000u)
                            : static_cast<int32_t>(val);
}

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
        .device_address  = config_.device_address,
        .scl_speed_hz    = 400000,
    };
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(config_.bus_handle, &dev_cfg, &dev_handle_),
        TAG, "Failed to add I2C device"
    );

    // current_lsb = max_expected_current / 2^19
    current_lsb_ = config_.max_expected_current_a / static_cast<float>(1 << 19);

    // SHUNT_CAL = 13107.2e6 * current_lsb * R_shunt  (INA228 datasheet eq. 1)
    uint16_t shunt_cal = static_cast<uint16_t>(
        13107.2e6f * current_lsb_ * config_.shunt_resistance_ohm
    );
    ESP_RETURN_ON_ERROR(write_register(REG_SHUNT_CAL, shunt_cal), TAG, "SHUNT_CAL write failed");

    // ADC_CONFIG: continuous all channels, 140 µs conversion time, no averaging
    //   MODE[3:0]   = 0b1111  bits 15:12  continuous shunt + bus + temp
    //   VBUSCT[2:0] = 0b001   bits 11:9   140 µs
    //   VSHCT[2:0]  = 0b001   bits  8:6   140 µs
    //   VTCT[2:0]   = 0b001   bits  5:3   140 µs
    //   AVG[2:0]    = 0b000   bits  2:0   1 sample (no averaging)
    //   => 0b1111_001_001_001_000 = 0xF248
    ESP_RETURN_ON_ERROR(write_register(REG_ADC_CONFIG, 0xF248), TAG, "ADC_CONFIG write failed");

    ESP_LOGI(TAG, "INA228 ready (shunt=%.1f mΩ, current_lsb=%.3f µA, shunt_cal=%u)",
             config_.shunt_resistance_ohm * 1000.0f,
             current_lsb_ * 1e6f,
             shunt_cal);
    return ESP_OK;
}

esp_err_t INA228::read(Measurement &out)
{
    uint32_t raw24;

    // Bus voltage — 20-bit unsigned, bits 23:4, LSB = 195.3125 µV
    ESP_RETURN_ON_ERROR(read_register_24(REG_VBUS, raw24), TAG, "VBUS read failed");
    out.bus_voltage_v = static_cast<float>(raw24 >> 4) * 195.3125e-6f;

    // Shunt voltage — 20-bit signed, bits 23:4, LSB = 312.5 nV (ADCRANGE = 0)
    ESP_RETURN_ON_ERROR(read_register_24(REG_VSHUNT, raw24), TAG, "VSHUNT read failed");
    out.shunt_voltage_uv = static_cast<float>(sign_extend_20(raw24 >> 4)) * 312.5e-3f;

    // Current — 20-bit signed, bits 23:4, LSB = current_lsb
    ESP_RETURN_ON_ERROR(read_register_24(REG_CURRENT, raw24), TAG, "CURRENT read failed");
    out.current_ma = static_cast<float>(sign_extend_20(raw24 >> 4)) * current_lsb_ * 1000.0f;

    // Power — 24-bit unsigned (full register), LSB = 3.2 * current_lsb
    ESP_RETURN_ON_ERROR(read_register_24(REG_POWER, raw24), TAG, "POWER read failed");
    out.power_mw = static_cast<float>(raw24) * 3.2f * current_lsb_ * 1000.0f;

    return ESP_OK;
}

esp_err_t INA228::reset()
{
    ESP_LOGI(TAG, "Resetting INA228");
    // RST = bit 15 of CONFIG register
    return write_register(REG_CONFIG, 0x8000);
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

esp_err_t INA228::read_register_24(uint8_t reg, uint32_t &value)
{
    uint8_t buf[3] = {};
    ESP_RETURN_ON_ERROR(
        i2c_master_transmit_receive(dev_handle_, &reg, 1, buf, 3, -1),
        TAG, "I2C 24-bit read failed"
    );
    value = (static_cast<uint32_t>(buf[0]) << 16) |
            (static_cast<uint32_t>(buf[1]) <<  8) |
             static_cast<uint32_t>(buf[2]);
    return ESP_OK;
}

}  // namespace embedlab
