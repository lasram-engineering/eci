#include "task_intercom.h"

#include <string.h>
#include <time.h>

#include <esp_log.h>
#include <esp_check.h>

#include "iot_agent.h"

#define MIN(a, b) ((a) < (b)) ? (a) : (b)

static const char *TAG = "ITC";

QueueHandle_t task_itc_to_uart_queue = NULL;

QueueHandle_t task_itc_from_uart_queue = NULL;

QueueHandle_t task_intercom_fiware_measurement_queue = NULL;

QueueHandle_t task_intercom_fiware_command_queue = NULL;

/**
 * @brief Initializes the task intercom objects
 *
 * @return esp_err_t ESP_FAIL if there was an error initializing the queues
 */
esp_err_t task_intercom_init()
{
    task_itc_to_uart_queue = xQueueCreate(CONFIG_ITC_UART_QUEUE_SIZE, sizeof(itc_message_t));

    ESP_RETURN_ON_FALSE(task_itc_to_uart_queue != NULL, ESP_FAIL, TAG, "Insufficient memory to allocate UART queue");

    task_itc_from_uart_queue = xQueueCreate(CONFIG_ITC_MAU_QUEUE_SIZE, sizeof(itc_message_t));

    ESP_RETURN_ON_FALSE(task_itc_from_uart_queue != NULL, ESP_FAIL, TAG, "Insufficient memory to allocate MAU queue");

    task_intercom_fiware_measurement_queue = xQueueCreate(CONFIG_ITC_IOTA_MEASUREMENT_QUEUE_SIZE, sizeof(itc_message_t));

    ESP_RETURN_ON_FALSE(task_intercom_fiware_measurement_queue != NULL, ESP_FAIL, TAG, "Insufficient memory to allocate IoT Measurement queue");

    task_intercom_fiware_command_queue = xQueueCreate(CONFIG_ITC_IOTA_COMMAND_QUEUE_SIZE, sizeof(itc_message_t));

    ESP_RETURN_ON_FALSE(task_intercom_fiware_command_queue != NULL, ESP_FAIL, TAG, "Insufficient memory to allocate IoT Command queue");

    return ESP_OK;
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
    message->payload_tokenized = NULL;
    message->tokens = NULL;
    message->token_num = 0;
    message->response = NULL;
    message->response_static = NULL;
    message->is_measurement = false;
}

esp_err_t task_itc_message_add_token(itc_message_t *message, char *token)
{
    if (message->token_num == 0)
        message->tokens = (char **)malloc(sizeof(char *));

    else
    {
        char **allocated = (char **)realloc(message->tokens, sizeof(char *) * (message->token_num + 1));

        if (allocated == NULL)
            return ESP_ERR_NO_MEM;

        message->tokens = allocated;
    }

    message->tokens[message->token_num] = token;
    message->token_num++;

    return ESP_OK;
}

esp_err_t task_itc_message_token_match(itc_message_t *message, int token_num, const char *match)
{
    if (token_num >= message->token_num)
        return ESP_ERR_INVALID_ARG;

    uint8_t ret = strncmp(message->tokens[token_num], match, MIN(strlen(message->tokens[token_num]), strlen(match)));

    return ret == 0 ? ESP_OK : ESP_FAIL;
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

    // free the payload
    if (message->payload != NULL)
        free(message->payload);

    // free the payload copy holding the tokens
    if (message->payload_tokenized != NULL)
        free(message->payload_tokenized);

    // free the array holding the pointers to the tokens
    if (message->tokens != NULL)
        free(message->tokens);

    if (message->response != NULL)
        free(message->response);

    free(message);

    message = NULL;
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
