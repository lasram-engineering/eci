#include "app_state.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

static app_state_t app_state_global = {};
static SemaphoreHandle_t lock = NULL;

esp_err_t app_state_init()
{
    app_state_global = (app_state_t){
        .error = APP_STATE_NO_ERROR,
        .wifi_connected = false,
        .error_msg = APP_STATE_NO_ERROR_MSG,
    };
    lock = xSemaphoreCreateMutex();

    if (lock == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void app_state_modify(app_state_handler_t handler, void *handler_extra_args)
{
    // acquire the mutex lock
    xSemaphoreTake(lock, portMAX_DELAY);

    // update the state
    handler(&app_state_global, handler_extra_args);

    // release the mutex
    xSemaphoreGive(lock);
}

/// @brief Returns a copy of the current state struct
/// @return The current state
app_state_t app_state_get()
{
    app_state_t state_copy;

    // acquire the mutex lock
    xSemaphoreTake(lock, portMAX_DELAY);

    // update the state
    state_copy = app_state_global;

    assert(&state_copy != &app_state_global);

    // release the mutex
    xSemaphoreGive(lock);

    return state_copy;
}
