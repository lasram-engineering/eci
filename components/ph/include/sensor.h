#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <esp_err.h>

/// @brief 32 bit integer value to store either mV or mpH (pH / 1000)
typedef uint32_t sensor_unit_t;

/// @brief pH sensor calibration data
/// @details uses linear interpolation to measure pH based on voltage values
typedef struct
{
    /// @brief lower voltage point
    sensor_unit_t voltage_low;
    /// @brief higher voltage point
    sensor_unit_t voltage_high;
    /// @brief lower pH value
    sensor_unit_t ph_low;
    /// @brief higher pH value
    sensor_unit_t ph_high;
    /// @brief sensor_unit_t to store the measured value
    sensor_unit_t ph_measurement;
} ph_sensor_t;

esp_err_t ph_sensor_init(ph_sensor_t *sensor);

esp_err_t ph_sensor_make_measurement(ph_sensor_t *sensor, sensor_unit_t *measurement);

esp_err_t ph_sensor_calibrate(ph_sensor_t *sensor, bool high_point, sensor_unit_t control_ph);