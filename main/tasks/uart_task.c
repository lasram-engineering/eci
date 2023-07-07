#include "tasks/uart_task.h"

#include <string.h>
#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_check.h>

#include "app_state.h"
#include "kawasaki.h"
#include "communication.h"
#include "tasks/mau_task.h"

/** Describes a valid uart response of some kind of error */
#define ESP_ERR_UART_NEG_RESPONSE -2

char *TAG = "UART";

/**
 * Configuration for UART robot
 */
static uart_config_t uart_config_robot = {
    .baud_rate = UART_BAUD_ROBOT,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};

const uart_port_t uart_robot = UART_NUM_1;
QueueHandle_t uart_queue_robot;

SemaphoreHandle_t uart_lock;

void uart_task(void *arg)
{
    ESP_LOGI(TAG, "Initializing UART...");

    // setup the UART parameters
    ESP_ERROR_CHECK(uart_param_config(uart_robot, &uart_config_robot));

    // set the pins to the uart
    ESP_ERROR_CHECK(uart_set_pin(uart_robot, UART_TX, UART_RX, UART_RTS, UART_CTS));

    // install driver
    ESP_ERROR_CHECK(
        uart_driver_install(
            uart_robot,
            UART_RX_BUFFER_SIZE,
            UART_TX_BUFFER_SIZE,
            UART_QUEUE_SIZE,
            &uart_queue_robot, 0));

    // create the semaphore handle
    uart_lock = xSemaphoreCreateBinary();

    if (uart_lock == NULL)
        abort();

    ESP_LOGI(TAG, "Initialization done.");

    xTaskCreate(uart_receive, "uart_receive", 4096, arg, 20, NULL);
    xTaskCreate(uart_respond, "uart_respond", 4096, NULL, 20, NULL);

    vTaskDelete(NULL);
}

char input_buffer[UART_BUF_LEN];

void uart_receive(void *arg)
{
    esp_err_t ret;
    while (1)
    {
        ret = kawasaki_read_transmission(uart_robot, input_buffer, UART_BUF_LEN, uart_lock);

        if (ret == ESP_OK)
        {
            ret = process_payload(input_buffer, *(TaskHandle_t *)arg);

            if (ret == ESP_FAIL)
            {
                // mau task was busy
                ESP_LOGI(TAG, "MAU task busy");
                ESP_ERROR_CHECK_WITHOUT_ABORT(kawasaki_write_transmission(uart_robot, "BUSY", uart_lock));
            }
            else if (ret == ESP_ERR_INVALID_ARG)
            {
                ESP_LOGI(TAG, "Invalid command format: %s", input_buffer);
                ESP_ERROR_CHECK_WITHOUT_ABORT(kawasaki_write_transmission(uart_robot, "INVALID", uart_lock));
            }
        }
    }
}

char *output_buffer[UART_BUF_LEN];

void uart_respond()
{
    while (1)
    {
        vTaskDelay(5000);
    }
}