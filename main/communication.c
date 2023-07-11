#include "communication.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#include "tasks/mau_task.h"
#include "tasks/task_intercom.h"

static const char *KW_MOTOR = "MOTOR";

/**
 * @brief Processes the payload coming from the Kawasaki Controller and pushes it onto the input queue for the MAU.
 *
 *
 * The payload is a list of keywords (tokens) separated by the pipe character
 *
 * @returns ESP_OK if the message was posted to the mau task
 *          ESP_FAIL if the mau task was busy
 *          ESP_ERR_INVALID_ARG if the payload was invalid
 */
esp_err_t process_payload(char *payload, TaskHandle_t mau_task_handle)
{
    // if the payload starts with "MOTOR" then send it forward to the MAU
    if (strncmp(payload, KW_MOTOR, strlen(KW_MOTOR)) == 0)
    {
        // copy the payload into the message
        set_recv(payload);

        configASSERT(mau_task_handle != NULL);

        // else notify the task that a new message is read
        xTaskNotifyGive(mau_task_handle);

        return ESP_OK;
    }

    return ESP_ERR_INVALID_ARG;
}
