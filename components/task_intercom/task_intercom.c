#include "task_intercom.h"

#include <string.h>
#include <time.h>

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
    task_intercom_uart_queue = xQueueCreate(CONFIG_ITC_UART_QUEUE_SIZE, sizeof(itc_message_t));

    ESP_RETURN_ON_FALSE(task_intercom_uart_queue != NULL, ESP_FAIL, TAG, "Insufficient memory to allocate UART queue");

    task_intercom_mau_queue = xQueueCreate(CONFIG_ITC_MAU_QUEUE_SIZE, sizeof(itc_message_t));

    ESP_RETURN_ON_FALSE(task_intercom_mau_queue != NULL, ESP_FAIL, TAG, "Insufficient memory to allocate MAU queue");

    task_intercom_fiware_measurement_queue = xQueueCreate(CONFIG_ITC_IOTA_MEASUREMENT_QUEUE_SIZE, sizeof(itc_message_t));

    ESP_RETURN_ON_FALSE(task_intercom_fiware_measurement_queue != NULL, ESP_FAIL, TAG, "Insufficient memory to allocate IoT Measurement queue");

    task_intercom_fiware_command_queue = xQueueCreate(CONFIG_ITC_IOTA_COMMAND_QUEUE_SIZE, sizeof(itc_message_t));

    ESP_RETURN_ON_FALSE(task_intercom_fiware_command_queue != NULL, ESP_FAIL, TAG, "Insufficient memory to allocate IoT Command queue");

    return ESP_OK;
}

/**
 * @brief Deletes a heap-allocated ITC message
 *
 * @param message the message pointer to be deleted
 */
void task_intercom_message_delete(itc_message_t *message)
{
    if (message == NULL)
        return;

    if (message->payload != NULL)
        free(message->payload);
    if (message->response != NULL)
        free(message->response);

    free(message);
}

/**
 * @brief Allocates a message on the stack
 * @note The fields of the struct need to be allocated separately
 *
 * @return itc_message_t* the pointer to the message
 */
itc_message_t *task_intercom_message_create()
{
    return (itc_message_t *)malloc(sizeof(itc_message_t));
}

itc_message_t *task_intercom_message_copy(itc_message_t *message)
{
    itc_message_t *copy = task_intercom_message_create();

    // allocate the fields
    copy->payload = (char *)malloc(sizeof(char) * (strlen(message->payload) + 1));
    copy->response = (char *)malloc(sizeof(char) * (strlen(message->payload) + 1));

    if (message->payload != NULL)
        strcpy(copy->payload, message->payload);

    if (message->response != NULL)
        strcpy(copy->response, message->response);

    if (message->response_static != NULL)
        copy->response_static = message->response_static;

    copy->message_id = message->message_id;
    copy->is_measurement = message->is_measurement;

    return copy;
}

/**
 * @brief Initializes an ITC message
 *
 * @param message pointer to the message struct
 */
void task_intercom_message_init(itc_message_t *message)
{
    time_t now;
    time(&now);
    message->message_id = now;
    message->payload = NULL;
    message->response = NULL;
    message->response_static = NULL;
    message->is_measurement = false;
}

/**
 * @brief Checks if the message is initialized and empty
 *
 * @param message pointer to the message
 * @return true if the payload and the response arrays are NULL
 * @return false otherwise
 */
bool task_intercom_message_is_empty(itc_message_t *message)
{
    return message->payload == NULL && message->response == NULL && message->response_static == NULL;
}
