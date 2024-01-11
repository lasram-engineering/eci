#include "ph.h"

#include <esp_log.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "task_intercom.h"

#include "sensor.h"

#define MIN(a, b) ((a) < (b)) ? (a) : (b)

static const char *TAG = "Acid etching";

static const char *KW_PH = "PH";
static const char *KW_MEASURE = "MEASURE";
static const char *KW_CALIBRATE = "CALIBRATE";
static const char *KW_HIGH = "HIGH";

static TaskHandle_t ph_task_handle = NULL;

void ph_task()
{
    int ret;
    sensor_unit_t measurement;
    ph_sensor_t sensor;
    itc_message_t *message;

    ESP_ERROR_CHECK(ph_sensor_init(&sensor));

    while (1)
    {
        xQueuePeek(task_itc_from_uart_queue, &message, portMAX_DELAY);

        // PH Command syntax
        //  0       1        2         3
        // PH | MEASURE
        // PH | CALIBRATE | HIGH | <ph value>
        // PH | CALIBRATE | LOW  | <ph value>

        if (message->token_num < 2)
            continue;

        if (task_itc_message_token_match(message, 0, KW_PH) != ESP_OK)
            continue;

        ret = xQueueReceive(task_itc_from_uart_queue, &message, 0);

        if (ret != pdTRUE)
        {
            ESP_LOGW(TAG, "Error receiving message from queue");
            continue;
        }

        //* pH Measurement
        if (task_itc_message_token_match(message, 1, KW_MEASURE) == ESP_OK)
        {
            ESP_LOGI(TAG, "Measuring pH value...");

            ret = ph_sensor_make_measurement(&sensor, &measurement);

            if (ret == ESP_OK)
            {
                ESP_LOGI(TAG, "Measured pH: %lu", measurement);
                asprintf(&message->response, "%lu", measurement);
            }
            else
            {
                const char *err_message = esp_err_to_name(ret);
                ESP_LOGW(TAG, "Error while measuring pH: (%d) %s", ret, err_message);

                message->response_static = err_message;
            }
        }
        //* calibration
        else if (task_itc_message_token_match(message, 1, KW_CALIBRATE) == ESP_OK)
        {
            ESP_LOGI(TAG, "Starting calibration...");

            sensor_unit_t control_ph = atoi(message->tokens[3]);

            if (task_itc_message_token_match(message, 2, KW_HIGH) == ESP_OK)
                ret = ph_sensor_calibrate(&sensor, true, control_ph);

            else
                ret = ph_sensor_calibrate(&sensor, false, control_ph);

            if (ret == ESP_OK)
                message->response_static = "OK";

            else
                message->response_static = esp_err_to_name(ret);
        }

        xQueueSend(task_itc_to_uart_queue, &message, portMAX_DELAY);
    }
}

esp_err_t ph_start_task()
{
    if (ph_task_handle != NULL)
        return ESP_ERR_INVALID_STATE;

    int ret = xTaskCreate(
        ph_task,
        TAG,
        CONFIG_PH_TASK_STACK_DEPTH,
        NULL,
        MIN(CONFIG_PH_TASK_PRIO, configMAX_PRIORITIES - 1),
        &ph_task_handle);

    return ret == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
