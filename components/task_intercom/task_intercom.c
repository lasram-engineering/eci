#include "task_intercom.h"

#include <string.h>

#include <esp_log.h>
#include <esp_check.h>

static const char *TAG = "ITC";

QueueHandle_t task_intercom_uart_queue = NULL;

QueueHandle_t task_intercom_mau_queue = NULL;

QueueHandle_t task_intercom_fiware_measurement_queue = NULL;

QueueHandle_t task_intercom_fiware_command_queue = NULL;

/**
 * @brief Initializes the task intercom objects
 *
 * @return esp_err_t ESP_FAIL if there was an error initializing the queues
 */
esp_err_t task_intercom_init()
{
    task_intercom_uart_queue = xQueueCreate(CONFIG_ITC_UART_QUEUE_SIZE, sizeof(itc_mau_message_t));

    ESP_RETURN_ON_FALSE(task_intercom_uart_queue != NULL, ESP_FAIL, TAG, "Insufficient memory to allocate UART queue");

    task_intercom_mau_queue = xQueueCreate(CONFIG_ITC_MAU_QUEUE_SIZE, sizeof(itc_mau_message_t));

    ESP_RETURN_ON_FALSE(task_intercom_mau_queue != NULL, ESP_FAIL, TAG, "Insufficient memory to allocate MAU queue");

    task_intercom_fiware_measurement_queue = xQueueCreate(CONFIG_ITC_IOTA_MEASUREMENT_QUEUE_SIZE, sizeof(itc_iota_measurement_t));

    ESP_RETURN_ON_FALSE(task_intercom_fiware_measurement_queue != NULL, ESP_FAIL, TAG, "Insufficient memory to allocate IoT Measurement queue");

    task_intercom_fiware_command_queue = xQueueCreate(CONFIG_ITC_IOTA_COMMAND_QUEUE_SIZE, sizeof(fiware_iota_command_t));

    ESP_RETURN_ON_FALSE(task_intercom_fiware_command_queue != NULL, ESP_FAIL, TAG, "Insufficient memory to allocate IoT Command queue");

    return ESP_OK;
}
