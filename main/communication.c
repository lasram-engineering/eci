#include "communication.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#include "tasks/mau_task.h"

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
    esp_err_t ret;

    // if the payload starts with "MOTOR" then send it forward to the MAU
    if (strncmp(payload, KW_MOTOR, strlen(KW_MOTOR)) == 0)
    {
        // copy the payload into the message
        ret = set_message(payload);

        // if the mau task was busy return the error
        if (ret != ESP_OK)
            return ret;

        configASSERT(mau_task_handle != NULL);

        // else notify the task that a new message is read
        // just notifies the task, notification value is not used
        xTaskNotify(mau_task_handle, 0x00, eNoAction);

        return ret;
    }

    return ESP_ERR_INVALID_ARG;
}
