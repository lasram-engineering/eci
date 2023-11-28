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
static const char *KW_REMOTE_COMMAND = "execute";

static FiwareAccessToken_t fiware_access_token;

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define FIWARE_IOT_COMMAND_PROGRAM_PARAMS 1

#define FREE_CMD_PARAMS(param, param_num) \
    {                                     \
        int i;                            \
        for (i = 0; i < param_num; i++)   \
        {                                 \
            free(param[i]);               \
        }                                 \
        free(param);                      \
    }

esp_err_t fiware_process_command(const itc_message_t *command)
{
    esp_err_t ret = ESP_OK;
    char *command_name = NULL;
    char *device_name = NULL;
    uint8_t param_num;
    char **params = NULL;
    itc_message_t *uart_message = NULL;

    ESP_GOTO_ON_ERROR(fiware_iota_command_get_command_name(command->payload, &command_name), cleanup, TAG, "Unable to get command name");
    ESP_GOTO_ON_ERROR(fiware_iota_command_get_device_name(command->payload, &device_name), cleanup, TAG, "Unable to get device name");
    ESP_GOTO_ON_ERROR(fiware_iota_command_get_params(command->payload, &params, &param_num), cleanup, TAG, "Unable ti get command params");
    ESP_GOTO_ON_FALSE(
        strncmp(device_name, CONFIG_IOT_AGENT_DEVICE_ID, MIN(strlen(device_name), strlen(CONFIG_IOT_AGENT_DEVICE_ID))) == 0,
        ESP_ERR_INVALID_ARG,
        cleanup,
        TAG,
        "IoT Command device ID mismatch: %s vs %s",
        CONFIG_IOT_AGENT_DEVICE_ID,
        device_name);

    ESP_LOGI(TAG, "Incoming command: %s", command->payload);

    // create message to UART
    uart_message = task_intercom_message_create();
    task_intercom_message_init(uart_message);

    //* PROGRAM UPDATE
    if (strcmp(command_name, KW_PROGRAM_UPDATE) == 0)
    {
        if (param_num == FIWARE_IOT_COMMAND_PROGRAM_PARAMS)
        {

            ESP_LOGI(TAG, "New progam: %s", params[0]);

            ret = asprintf(&uart_message->response, "PROGRAM|%s", params[0]);

            if (ret == -1)
                ret = ESP_ERR_NO_MEM;
            else
                ret = ESP_OK;
        }
        else
        {
            ESP_LOGW(TAG, "Invalid number of parameters (%d vs %d)", FIWARE_IOT_COMMAND_PROGRAM_PARAMS, param_num);

            ret = ESP_ERR_INVALID_ARG;
        }
    }

    //* REMOTE COMMAND
#ifdef CONFIG_IOT_AGENT_REMOTE_COMMANDS
    if (strcmp(command_name, KW_REMOTE_COMMAND) == 0)
    {
        char *command_params;
        ret = fiware_iota_command_get_param_string(command->payload, &command_params);

        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "Executing remote command with arguments: %s", command_params);
            uart_message->payload = strdup(command_params);
            uart_message->message_id = IOT_AGENT_REMOTE_COMMAND_ID;
            free(command_params);
        }
    }
#endif

cleanup:
    if (command_name != NULL)
        free(command_name);
    if (device_name != NULL)
        free(device_name);
    if (params != NULL)
        FREE_CMD_PARAMS(params, param_num);

    // if the message is empty and allocated free it
    if (task_intercom_message_is_empty(uart_message))
    {
        task_intercom_message_delete(uart_message);
    }
    else
    {
        xQueueSend(task_intercom_uart_queue, &uart_message, portMAX_DELAY);
    }

    return ret;
}

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

    itc_message_t *incoming_message;

    /* LOOP */
    while (1)
    {
        // block until a measurement payload comes in
        ret = xQueueReceive(task_intercom_fiware_measurement_queue, &incoming_message, pdMS_TO_TICKS(CONFIG_FIWARE_TASK_MEASUREMENT_TIMEOUT));

        // check if the queue had items
        if (ret == pdTRUE)
        {
            ret = fiware_iota_make_measurement(incoming_message->payload, &fiware_access_token, NULL);
            if (ret == ESP_OK)
                incoming_message->response_static = "OK";
            else
                incoming_message->response_static = "NO WIFI";

            // send the message back to the UART task
            xQueueSend(task_itc_to_uart_queue, &incoming_message, portMAX_DELAY);

            continue;
        }

        // if there was a timeout, check the commands
        ret = xQueueReceive(task_intercom_fiware_command_queue, &incoming_message, 0);

        if (ret == pdFALSE)
            continue;

        // check for program update command
        ret = fiware_process_command(incoming_message);

        if (ret != ESP_OK)
        {
            ESP_LOGI(TAG, "Error while executing command: %s", esp_err_to_name(ret));
        }

        task_intercom_message_delete(incoming_message);
    }
}
