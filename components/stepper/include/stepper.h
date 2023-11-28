#pragma once

#include <stdbool.h>

#include <esp_err.h>
#include <esp_timer.h>

typedef struct
{
    uint8_t pin_en;
    uint8_t pin_step;
    uint8_t pin_dir;
    /// @brief the system time at which the last step was taken
    uint64_t last_step;
    /// @brief current speed in steps per second
    uint32_t speed;
    /// @brief maximum allowed rotation speed in steps / second
    uint32_t max_speed;
    /// @brief high precision hardware timer to control the motor
    esp_timer_handle_t timer;
} stepper_t;

esp_err_t stepper_init_stepper(uint8_t step, uint8_t dir, uint8_t en, esp_timer_handle_t timer, stepper_t *stepper);

esp_err_t stepper_set_max_speed(stepper_t *stepper, uint32_t max_speed);

esp_err_t stepper_turn_on(stepper_t *stepper, bool on);

esp_err_t stepper_start_task();