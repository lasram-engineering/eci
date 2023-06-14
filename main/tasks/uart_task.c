#include "tasks/uart_task.h"

#include <string.h>
#include <stdio.h>

#include <driver/uart.h>
#include <esp_log.h>

#define UART_RX 2
#define UART_TX 0
#define UART_RTX 3
#define UART_CTS 15

#define UART_RX_BUFFER_SIZE 1024
#define UART_TX_BUFFER_SIZE 1024
#define UART_QUEUE_SIZE 10

#define UART_MAX_RESPONSE_LENGTH 32

static char *TAG = "UART";
static char uart_response[UART_MAX_RESPONSE_LENGTH];

static uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};

const uart_port_t uart_num = UART_NUM_2;

QueueHandle_t uart_queue;

/**
 * Reads from the UART device until a null termination character is found
 * The found string is stored in the uart_response char array
 */
esp_err_t uart_read_string()
{
    int i;
    for (i = 0; i < UART_MAX_RESPONSE_LENGTH; i++)
    {
        // block until a response is sent
        uart_read_bytes(uart_num, &uart_response[i], 1, portMAX_DELAY);

        if (uart_response[i] == '\n')
            break;
    }

    // replace the terminating newline with a zero
    uart_response[i] = '\0';

    return ESP_OK;
}

void uart_task(void *arg)
{
    ESP_LOGI(TAG, "Initializing UART...");

    // setup the UART parameters
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

    // set the pins to the uart
    ESP_ERROR_CHECK(uart_set_pin(uart_num, UART_TX, UART_RX, UART_RTX, UART_CTS));

    // install driver
    ESP_ERROR_CHECK(
        uart_driver_install(
            uart_num,
            UART_RX_BUFFER_SIZE,
            UART_TX_BUFFER_SIZE,
            UART_QUEUE_SIZE,
            &uart_queue, 0));

    ESP_LOGI(TAG, "Initialization done.");

    bool motorRun = false;
    char *startMotor = "DO|MOTOR ON";
    char *stopMotor = "DO|MOTOR OFF";

    while (1)
    {
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        if (motorRun)
        {
            ESP_LOGI(TAG, "Turning on motor");
            uart_write_bytes(uart_num, startMotor, strlen(startMotor));
        }
        else
        {
            ESP_LOGI(TAG, "Turning motor off");
            uart_write_bytes(uart_num, stopMotor, strlen(stopMotor));
        }

        // check response
        esp_err_t ret = uart_read_string();

        if (ret == ESP_OK)
        {
            if (strcmp(uart_response, "OK") == 0)
            {
                ESP_LOGI(TAG, "Success");
            }
            else if (strcmp(uart_response, "ERROR") == 0)
            {
                ESP_LOGW(TAG, "Error");
            }
        }
        motorRun = !motorRun;
    }
}