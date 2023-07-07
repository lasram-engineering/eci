#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// UART pins for MAU
#define UART_RX_MAU 2
#define UART_TX_MAU 0
#define UART_RTS_MAU 4
#define UART_CTS_MAU 16
#define UART_BAUD_MAU 115200

#define UART_RX_BUFFER_SIZE 1024
#define UART_TX_BUFFER_SIZE 1024
#define UART_QUEUE_SIZE 10

/** Specifies the maximum length of the uart buffer */
#define UART_BUFFER_LEN 32
/** Specifies the time after a timeout occurs while waiting for a read */
#define UART_READ_TIMEOUT_MS 1000

typedef struct
{
    char response[UART_BUFFER_LEN];
    esp_err_t error_code;
} Mau_Response_t;

/** Stores the message from the UART receive task (do not set directly) */
static char incoming_message[UART_BUFFER_LEN];

/** Stores the response to the UART respond task */
static Mau_Response_t response_message;

void mau_task(void *arg);

esp_err_t set_message(const char *message);