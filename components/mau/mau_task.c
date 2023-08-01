#include "mau_task.h"

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <driver/uart.h>
#include <esp_log.h>

#include "uart.h"
#include "task_intercom.h"

static const char *TAG = "MAU";

/**
 * Configuration for UART MAU
 */
static uart_config_t uart_config = {
    .baud_rate = UART_BAUD_MAU,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};

const uart_port_t uart_num = UART_NUM_2;
QueueHandle_t uart_queue;

char uart_buffer[UART_BUFFER_LEN];
itc_mau_message_t mau_incoming_message;
itc_uart_message_t uart_message;

/**
 * Task function for interaction with the MAU (Measurement and Actuation Unit)
 *
 * @param uart_respond_handle the task handle to the respond task
 */
void mau_task(void *arg)
{
    ESP_LOGI(TAG, "Initializing MAU communication task");

    // setup the UART params
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

    // bind the pins to the UART interface
    ESP_ERROR_CHECK(uart_set_pin(uart_num, UART_TX_MAU, UART_RX_MAU, UART_RTS_MAU, UART_CTS_MAU));

    // install the driver
    ESP_ERROR_CHECK(
        uart_driver_install(
            uart_num,
            UART_RX_BUFFER_SIZE,
            UART_TX_BUFFER_SIZE,
            UART_QUEUE_SIZE,
            &uart_queue, 0));

    ESP_LOGI(TAG, "Initialization done");

    esp_err_t ret;

    while (1)
    {
        // wait for the queue to have items
        xQueueReceive(task_intercom_mau_queue, &mau_incoming_message, portMAX_DELAY);

        ESP_LOGI(TAG, "Incoming message: %s", mau_incoming_message.payload);

        uart_message.is_error = false;

        // send the message to the MAU
        ret = uart_write_bytes(uart_num, mau_incoming_message.payload, strlen(mau_incoming_message.payload));

        // receive the response from the MAU
        ret = uart_read_string(uart_num, uart_buffer, UART_BUFFER_LEN, UART_READ_TIMEOUT_MS / portTICK_PERIOD_MS);

        if (ret != ESP_OK)
        {
            // an error has occurred
            const char *error_msg = esp_err_to_name(ret);
            ESP_LOGW(TAG, "An error occurred while reading from MAU: %d -> %s", ret, error_msg);

            // check if the error message fits the buffer
            ESP_ERROR_CHECK(strlen(error_msg) <= CONFIG_ITC_UART_MESSAGE_SIZE);

            strcpy(uart_message.payload, error_msg);
        }
        else
        {
            // the response was read
            // set the response to the send buffer
            strcpy(uart_message.payload, uart_buffer);
        }

        // append the uart message to the queue
        xQueueSend(task_intercom_uart_queue, &uart_message, portMAX_DELAY);
    }
}