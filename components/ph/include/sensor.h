#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <esp_err.h>

typedef uint32_t sensor_unit_t;

typedef struct
{
    sensor_unit_t voltage_low;
    sensor_unit_t voltage_high;
    sensor_unit_t ph_low;
    sensor_unit_t ph_high;
    sensor_unit_t ph_measurement;
} ph_sensor_t;

esp_err_t ph_sensor_init(ph_sensor_t *sensor);

esp_err_t ph_sensor_make_measurement(ph_sensor_t *sensor, sensor_unit_t *measurement);

esp_err_t ph_sensor_calibrate(ph_sensor_t *sensor, bool high_point, sensor_unit_t control_ph);