#pragma once

#include <esp_err.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define UART_RX_BUFFER_SIZE 1024
#define UART_TX_BUFFER_SIZE 1024
#define UART_QUEUE_SIZE 10

#define UART_BUF_LEN 32

#define UART_TIMEOUT_MS 100

esp_err_t uart_start_task();

void uart_task(void *arg);
