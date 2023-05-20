#include "app_tasks.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#include "app_state.h"

static const char *TAG = "STATE_TASKS";

void error_task(void *args)
{
    ESP_LOGI(TAG, "Starting error task");
    while (1)
    {
        // wait for the error state to be updated
        app_state_wait_for_event(STATE_TYPE_ERROR, APP_STATE_UPDATE);

        EventBits_t error_bits = app_state_get(STATE_TYPE_ERROR);

        ESP_LOGI(TAG, "Error event: %lx", error_bits);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void input_task(void *args)
{
}

void internal_task(void *args)
{
}
