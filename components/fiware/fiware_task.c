#include "fiware_task.h"

#include <string.h>
#include <time.h>

#include <esp_log.h>
#include <esp_check.h>
#include <esp_netif_sntp.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "iot_agent.h"
#include "fiware_idm.h"

#include "task_intercom.h"
#include "wifi.h"

#define MIN(a, b) ((a) < (b)) ? (a) : (b)

static const char *TAG = "FIWARE Task";

static TaskHandle_t fiware_task_handle = NULL;

esp_err_t fiware_start_task()
{
    if (fiware_task_handle != NULL)
        return ESP_FAIL;

    int ret = xTaskCreate(
        fiware_task,
        TAG,
        CONFIG_FIWARE_TASK_STACK_DEPTH,
        NULL,
        MIN(CONFIG_FIWARE_TASK_PRIO, configMAX_PRIORITIES - 1),
        &fiware_task_handle);

    return ret == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

static const char *KW_PROGRAM_UPDATE = "update_program";

static fiware_iota_command_t fiware_incoming_command;

static FiwareAccessToken_t fiware_access_token;

void fiware_task()
{
    int ret;

    // wait for wifi to be connected
    wifi_wait_connected(portMAX_DELAY);

    // wait for network time to be synchronized
    ESP_LOGI(TAG, "Waiting for network time sync...");
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_sntp_sync_wait(portMAX_DELAY));
    ESP_LOGI(TAG, "Network time synchronized");

    ESP_LOGI(TAG, "Requesting access token from IdM");

    ret = fiware_idm_request_access_token(&fiware_access_token);

    if (ret != ESP_OK)
        ESP_LOGE(TAG, "Unable to get access token");

    itc_message_t *incoming_measurement;
    itc_message_t *fiware_command_out;
    time_t now;

    /* LOOP */
    while (1)
    {
        // block until a measurement payload comes in
        ret = xQueueReceive(task_intercom_fiware_measurement_queue, &incoming_measurement, pdMS_TO_TICKS(CONFIG_FIWARE_TASK_MEASUREMENT_TIMEOUT));

        // check if the queue had items
        if (ret == pdTRUE)
        {
            ret = fiware_iota_make_measurement(incoming_measurement->payload, &fiware_access_token, NULL);
            if (ret == ESP_OK)
                incoming_measurement->response_static = "OK";
            else
                incoming_measurement->response_static = "NO WIFI";

            // send the command back to the UART task
            xQueueSend(task_intercom_uart_queue, &fiware_command_out, portMAX_DELAY);

            continue;
        }

        // if there was a timeout, check the commands
        ret = xQueueReceive(task_intercom_fiware_command_queue, &fiware_incoming_command, 0);

        if (ret == pdFALSE)
            continue;

        // check for program update command
        if (strcmp(fiware_incoming_command.command_name, KW_PROGRAM_UPDATE) == 0)
        {
            ESP_LOGI(TAG, "New progam: %s", fiware_incoming_command.command_param);

            fiware_command_out = task_intercom_message_create();
            task_intercom_message_init(fiware_command_out);

            // set the id based on the time
            time(&now);
            fiware_command_out->message_id = now;

            // allocate and load the param into the response
            asprintf(&fiware_command_out->response, "PROGRAM|%s", fiware_incoming_command.command_param);

            // send the response to the UART task (will wait forever)
            xQueueSend(task_intercom_uart_queue, &fiware_command_out, portMAX_DELAY);
        }
    }
}
