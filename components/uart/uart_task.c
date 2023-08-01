#include "uart_task.h"

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
#include "task_intercom.h"
#include "mau_task.h"

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

char input_buffer[UART_BUF_LEN];

/**
 * @brief Manages communication with the Kawasaki Controller and responds to messages and errors from the MAU task
 *
 * @param arg TaskHandle_t* pointer to the task handle object of the MAU task
 */
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

    ESP_LOGI(TAG, "Initialization done.");

    esp_err_t ret;

    /* LOOP */
    while (1)
    {
        ret = kawasaki_read_transmission(uart_robot, input_buffer, UART_BUF_LEN, pdMS_TO_TICKS(UART_TIMEOUT_MS));

        if (ret == ESP_ERR_TIMEOUT)
        {
            process_incoming_messages();

            continue;
        }

        if (ret == ESP_OK)
        {
            // no error has occurred
            // process the incoming message
            ESP_LOGI(TAG, "Incoming message: %s", input_buffer);

            // process and try to send the message to the mau
            ret = process_payload(input_buffer);

            if (ret == ESP_FAIL)
            {
                // respond with BUSY message
                ret = kawasaki_write_transmission(uart_robot, "BUSY");

                if (ret != ESP_OK)
                {
                    const char *error = esp_err_to_name(ret);
                    ESP_LOGE(TAG, "Error sending BUSY to robot: %d -> %s", ret, error);
                }

                continue;
            }
            if (ret == ESP_ERR_INVALID_ARG)
            {
                ESP_LOGE(TAG, "Invalid argument");
                continue;
            }

            continue;
        }

        // other errors
        const char *error = esp_err_to_name(ret);
        ESP_LOGE(TAG, "An error occurred: %d -> %s", ret, error);
    }
}

itc_uart_message_t uart_incoming_message;

/**
 * @brief Processes incoming messages from the UART queue
 *
 */
esp_err_t process_incoming_messages()
{
    int ret = xQueueReceive(task_intercom_uart_queue, &uart_incoming_message, 0);

    // return if there is no message in the queue
    if (ret == pdFALSE)
        return ESP_ERR_NOT_FOUND;

    ESP_LOGI(TAG, "Sending message to robot: %s", uart_incoming_message.payload);

    // check if the incoming message is an error message
    ret = kawasaki_write_transmission(uart_robot, uart_incoming_message.payload);

    switch (ret)
    {
    case ESP_FAIL:
        ESP_LOGW(TAG, "Transmission failed: %s", uart_incoming_message.payload);
        break;

    case ESP_ERR_INVALID_RESPONSE:
        ESP_LOGW(TAG, "Invalid response to: %s", uart_incoming_message.payload);
        break;

    case ESP_ERR_NOT_FINISHED:
        ESP_LOGW(TAG, "Possible ENQ collision during sending %s", uart_incoming_message.payload);
        break;

    case ESP_ERR_TIMEOUT:
        ESP_LOGW(TAG, "Message timed out: %s", uart_incoming_message.payload);
        break;

    default:
        break;
    }

    return ret;
}