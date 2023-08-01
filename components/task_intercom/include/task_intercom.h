#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "iot_agent.h"

typedef struct
{
    char payload[CONFIG_ITC_MAU_MESSAGE_SIZE];
} itc_mau_message_t;

typedef struct
{
    bool is_error;
    char payload[CONFIG_ITC_UART_MESSAGE_SIZE];
} itc_uart_message_t;

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
