#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "iot_agent.h"

typedef struct
{
    uint32_t message_id;
    char *payload;
    char *response;
    const char *response_static;
    bool is_measurement;
} itc_message_t;

typedef struct
{
    char payload[CONFIG_ITC_IOTA_MEASUREMENT_MESSAGE_SIZE];
} itc_iota_measurement_t;

/** Queue to store the messages to the Kawasaki controller */
extern QueueHandle_t task_intercom_uart_queue;
/** Queue to store the messages to the MAU */
extern QueueHandle_t task_intercom_mau_queue;
/** Queue to store the measurements to the FIWARE IoT Agent */
extern QueueHandle_t task_intercom_fiware_measurement_queue;

extern QueueHandle_t task_intercom_fiware_command_queue;

esp_err_t task_intercom_init();

void task_intercom_message_delete(itc_message_t *message);

itc_message_t *task_intercom_message_create();

itc_message_t *task_intercom_message_copy(itc_message_t *message);

void task_intercom_message_init(itc_message_t *message);

bool task_intercom_message_is_empty(itc_message_t *message);
