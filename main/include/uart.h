#pragma once

#include <driver/uart.h>
#include <esp_log.h>

esp_err_t uart_read_string(uart_port_t port, char *buffer, const unsigned int buffer_length, TickType_t delay_ticks);