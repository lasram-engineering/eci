#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// UART pins for robot
#define UART_RX 17
#define UART_TX 5
#define UART_RTS 18
#define UART_CTS 19
#define UART_BAUD_ROBOT 9600

#define UART_RX_BUFFER_SIZE 1024
#define UART_TX_BUFFER_SIZE 1024
#define UART_QUEUE_SIZE 10

#define UART_BUF_LEN 32

void uart_task(void *arg);

void uart_receive(void *arg);
void uart_respond();