#include "stepper.h"

#include <string.h>

#include <esp_check.h>
#include <driver/gpio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "task_intercom.h"

#define MIN(a, b) ((a) < (b)) ? (a) : (b)

static const char *TAG = "Stepper";

static const char *KW_MOTOR = "MOTOR";
static const char *KW_ON = "ON";
static const char *KW_OFF = "OFF";
static const char *KW_SPEED = "SPEED";

static TaskHandle_t stepper_task_handle = NULL;

void timer_callback(void *param)
{
    stepper_t *stepper = (stepper_t *)param;

    gpio_set_level(stepper->pin_step, 1);
    gpio_set_level(stepper->pin_step, 0);
}

void stepper_task()
{
    stepper_t stepper;
    esp_timer_handle_t timer;
    int ret;
    itc_message_t *message = NULL;

    // create the timer
    esp_timer_create_args_t timer_args = {
        .callback = timer_callback,
        .name = "Stepper timer",
        .arg = &stepper,
    };

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));

    ESP_ERROR_CHECK(
        stepper_init_stepper(
            CONFIG_STEPPER_STEP_PIN,
            CONFIG_STEPPER_DIR_PIN,
            CONFIG_STEPPER_EN_PIN,
            timer,
            &stepper));

    //* LOOP
    while (1)
    {
        // wait for incoming messages
        xQueuePeek(task_itc_from_uart_queue, &message, portMAX_DELAY);

        // motor control message syntax
        //   0       1      2
        // MOTOR | SPEED | 100
        // MOTOR |  ON
        // MOTOR |  OFF

        // check if the message is for the stepper controller task
        if (message->token_num < 2)
            continue;

        if (strcmp(message->tokens[0], KW_MOTOR) != 0)
            continue;

        // this is a motor control message, receive it from the queue
        ret = xQueueReceive(task_itc_from_uart_queue, &message, 0);

        if (ret != pdTRUE)
        {
            ESP_LOGE(TAG, "Some other task took the message");
            continue;
        }

        message->response_static = "OK";

        //* MOTOR ON
        if (task_itc_message_token_match(message, 1, KW_ON) == ESP_OK)
        {
            ret = stepper_turn_on(&stepper, true);

            if (ret == ESP_ERR_INVALID_STATE)
                message->response_static = "ALREADY ON";
        }
        //* MOTOR OFF
        else if (task_itc_message_token_match(message, 1, KW_OFF) == ESP_OK)
        {
            ret = stepper_turn_on(&stepper, false);

            if (ret == ESP_ERR_INVALID_STATE)
                message->response_static = "ALREADY OFF";
        }
        //* MOTOR SPEED
        else if (task_itc_message_token_match(message, 1, KW_SPEED) == ESP_OK)
        {
            // parse the new speed
            int speed = atoi(message->tokens[2]);

            stepper_set_max_speed(&stepper, speed);

            stepper_turn_on(&stepper, false); // turn off timer
            stepper_turn_on(&stepper, true);  // turn on timer
        }

        xQueueSend(task_itc_to_uart_queue, &message, portMAX_DELAY);
    }
}

esp_err_t stepper_init_stepper(uint8_t step, uint8_t dir, uint8_t en, esp_timer_handle_t timer, stepper_t *stepper)
{
    // configure the GPIO ports
    gpio_config_t config = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1 << step) | (1 << dir) | (1 << en),
    };

    int ret = gpio_config(&config);

    if (ret != ESP_OK)
        return ret;

    stepper->pin_dir = dir;
    stepper->pin_en = en;
    stepper->pin_step = step;
    stepper->last_step = 0;
    stepper->speed = 0;
    stepper->max_speed = 0;
    stepper->timer = timer;

    return ESP_OK;
}

esp_err_t stepper_set_max_speed(stepper_t *stepper, uint32_t max_speed)
{
    stepper->max_speed = max_speed;

    return ESP_OK;
}

esp_err_t stepper_turn_on(stepper_t *stepper, bool on)
{
    if (!on)
        return esp_timer_stop(stepper->timer);

    if (stepper->max_speed == 0)
    {
        ESP_LOGW(TAG, "Stepper motor speed is set to 0, cannot start");
        return ESP_ERR_INVALID_ARG;
    }

    uint64_t period_us = 1000000 / stepper->max_speed;

    return esp_timer_start_periodic(stepper->timer, period_us);
}

esp_err_t stepper_start_task()
{
    if (stepper_task_handle != NULL)
        return ESP_FAIL;

    int ret = xTaskCreate(
        stepper_task,
        TAG,
        CONFIG_STEPPER_TASK_STACK_DEPTH,
        NULL,
        MIN(CONFIG_STEPPER_TASK_PRIO, configMAX_PRIORITIES - 1),
        &stepper_task_handle);

    return ret == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}