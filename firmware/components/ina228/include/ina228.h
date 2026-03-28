/**
 * @file ina228.h
 * @brief INA228 High-Precision Power Monitor Driver
 *
 * Provides I2C communication with the TI INA228 for measuring bus voltage,
 * shunt voltage, current, and power.
 *
 * Copyright (c) 2026 EmbedLab-Tech. Licensed under MIT.
 */

#pragma once

#include <cstdint>
#include "esp_err.h"
#include "driver/i2c_master.h"

namespace embedlab {

/**
 * @brief Driver for the TI INA228 high-precision power/energy monitor.
 */
class INA228 {
public:
    static constexpr uint8_t DEFAULT_ADDRESS = 0x40;

    struct Config {
        i2c_master_bus_handle_t bus_handle;
        uint8_t device_address = DEFAULT_ADDRESS;
        float shunt_resistance_ohm = 0.010f;  // 10 mOhm default
        float max_expected_current_a = 3.2f;
    };

    struct Measurement {
        float bus_voltage_v;
        float shunt_voltage_uv;
        float current_ma;
        float power_mw;
    };

    explicit INA228(const Config &config);
    ~INA228();

    INA228(const INA228 &) = delete;
    INA228 &operator=(const INA228 &) = delete;

    /** @brief Initialize the device and configure calibration registers. */
    esp_err_t init();

    /** @brief Read all measurement channels in a single burst. */
    esp_err_t read(Measurement &out);

    /** @brief Reset the device to factory defaults. */
    esp_err_t reset();

private:
    esp_err_t write_register(uint8_t reg, uint16_t value);
    esp_err_t read_register(uint8_t reg, uint16_t &value);

    Config config_;
    i2c_master_dev_handle_t dev_handle_ = nullptr;
    float current_lsb_ = 0.0f;
};

}  // namespace embedlab
