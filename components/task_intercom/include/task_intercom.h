#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

/// @brief Inter Task Communication message
/// @details Use this struct to pass messages to and from tasks via the provided queues
typedef struct
{
    /// @brief The ID of the message
    uint32_t message_id;
    /// @brief The raw payload string of the message
    char *payload;
    /// @brief Tokenized payload string of the message
    char *payload_tokenized;
    /// @brief Array of pointers to the start of each token
    char **tokens;
    /// @brief The number of tokens
    uint8_t token_num;
    char *response;
    const char *response_static;
    bool is_measurement;
} itc_message_t;

typedef struct
{
    char payload[CONFIG_ITC_IOTA_MEASUREMENT_MESSAGE_SIZE];
} itc_iota_measurement_t;

/** @brief Queue to store the messages to the Kawasaki controller */
extern QueueHandle_t task_itc_to_uart_queue;
/** @brief Queue to store the messages to the MAU */
extern QueueHandle_t task_itc_from_uart_queue;
/** @brief Queue to store the measurements to the FIWARE IoT Agent */
extern QueueHandle_t task_intercom_fiware_measurement_queue;
/** @brief Queue to store the commands from the FIWARE IoT Agent */
extern QueueHandle_t task_intercom_fiware_command_queue;

esp_err_t task_intercom_init();

itc_message_t *task_intercom_message_create();

void task_intercom_message_init(itc_message_t *message);

esp_err_t task_itc_message_add_token(itc_message_t *message, char *token);

esp_err_t task_itc_message_token_match(itc_message_t *message, int token_num, const char *match);

void task_intercom_message_delete(itc_message_t *message);

bool task_intercom_message_is_empty(itc_message_t *message);
