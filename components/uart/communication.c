#include "communication.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string.h>

#include "mau_task.h"
#include "task_intercom.h"
#include "fiware_task.h"

static const char *TAG = "UART PAYLOAD";

static const char *KW_MOTOR = "MOTOR";
static const char *KW_START = "START";
static const char *KW_END = "END";

itc_mau_message_t mau_message;

/*
* UART payload syntax

The tokens are separated by a pipe "|" character

First tokens can be MOTOR, START, END

* MOTOR token:  MOTOR|<ON or OFF or the desired number for motor speed>
* START token:  START|<ACID or SAND or ANOD>
* END token:    END|<SUCCESS or ERROR>

*/

/**
 * @brief Processes the payload coming from the Kawasaki Controller and pushes it onto the input queue for the MAU.
 * The payload is a list of keywords (tokens) separated by the pipe character
 *
 * @returns ESP_OK if the message was posted to the mau task
 *          ESP_FAIL if the mau task was busy
 *          ESP_ERR_INVALID_ARG if the payload was invalid
 */
esp_err_t process_payload(char *payload)
{
    int ret;

    // if the payload starts with "MOTOR" then send it forward to the MAU
    if (strncmp(payload, KW_MOTOR, strlen(KW_MOTOR)) == 0)
    {
        // copy the payload into the message
        strcpy(mau_message.payload, payload);

        // send the payload to the queue
        ret = xQueueSend(task_intercom_mau_queue, &mau_message, 0);

        if (ret == errQUEUE_FULL)
            return ESP_FAIL;

        return ESP_OK;
    }

    if (strncmp(payload, KW_START, strlen(KW_START)) == 0)
    {

        // split the START from the beginning of the string
        char *token = strtok(payload, "|");

        if (token == NULL)
            return ESP_ERR_INVALID_ARG;

        // the next token has to be the station name
        token = strtok(NULL, "|");

        if (strlen(token) >= CONFIG_ITC_IOTA_MEASUREMENT_MESSAGE_SIZE)
        {
            ESP_LOGE(TAG, "Token (%d) is larger than max measurement length of %d", strlen(token), CONFIG_ITC_IOTA_MEASUREMENT_MESSAGE_SIZE);
            return ESP_ERR_INVALID_ARG;
        }

        ESP_LOGI(TAG, "Starting process at %s", token);

#ifdef CONFIG_FIWARE_TASK_ENABLE

        char measurement[CONFIG_ITC_IOTA_MEASUREMENT_MESSAGE_SIZE];

        sprintf(measurement, "s|%s", token);

        // add the measurement to the queue
        xQueueSend(task_intercom_fiware_measurement_queue, measurement, 0);
#endif

        return ESP_OK;
    }

    // TODO: Add END command processing

    return ESP_ERR_INVALID_ARG;
}
