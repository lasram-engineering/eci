#include "fiware_task.h"

#include <string.h>

#include <esp_log.h>
#include <esp_check.h>
#include <esp_netif_sntp.h>

#include "iot_agent.h"
#include "fiware_idm.h"

#include "task_intercom.h"
#include "app_state.h"

static const char *TAG = "FIWARE Task";

static const char *KW_PROGRAM_UPDATE = "program_update";

static itc_iota_measurement_t fiware_incoming_measurement;
static fiware_iota_command_t fiware_incoming_command;

static itc_uart_message_t uart_message;

static FiwareAccessToken_t fiware_access_token;

void fiware_task()
{
    int ret;

    // wait for wifi to be connected
    app_state_wait_for_event(STATE_TYPE_INTERNAL, APP_STATE_INTERNAL_WIFI_CONNECTED);

    // wait for network time to be synchronized
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_sntp_sync_wait(portMAX_DELAY));

    ESP_LOGI(TAG, "Requesting access token from IdM");

    ret = fiware_idm_request_access_token(&fiware_access_token);

    if (ret == ESP_OK)
        ESP_LOGI(TAG, "Got access token: %s", fiware_access_token.token);
    else
        ESP_LOGE(TAG, "Unable to get access token");

    /* LOOP */
    while (1)
    {
        // block until a measurement payload comes in
        ret = xQueueReceive(task_intercom_fiware_measurement_queue, &fiware_incoming_measurement, pdMS_TO_TICKS(CONFIG_FIWARE_TASK_MEASUREMENT_TIMEOUT));

        // check if the queue had items
        if (ret == pdTRUE)
        {
            fiware_iota_make_measurement(fiware_incoming_measurement.payload, &fiware_access_token, NULL);
            continue;
        }

        // if there was a timeout, check the commands
        ret = xQueuePeek(task_intercom_fiware_command_queue, &fiware_incoming_command, 0);

        if (ret == pdFALSE)
            continue;

        // check for program update command
        if (strcmp(fiware_incoming_command.command_name, KW_PROGRAM_UPDATE) == 0)
        {
            ESP_LOGI(TAG, "New porgam: %s", fiware_incoming_command.command_param);

            // load the program name into the message payload
            snprintf(uart_message.payload, CONFIG_ITC_IOTA_MEASUREMENT_MESSAGE_SIZE, "PROGRAM|%s", fiware_incoming_command.command_param);

            // send the message to the queue
            ret = xQueueSend(task_intercom_uart_queue, &uart_message, 0);

            if (ret == errQUEUE_FULL)
                ESP_LOGE(TAG, "Unable to send to UART task, queue full");

            // remove the item from the queue
            xQueueReceive(task_intercom_fiware_command_queue, &fiware_incoming_command, 0);
        }
    }
}
