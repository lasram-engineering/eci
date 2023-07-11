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
#include "tasks/task_intercom.h"
#include "tasks/mau_task.h"

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
            // check the error
            process_error();

            // check the send buffer
            process_send();

            continue;
        }

        if (ret == ESP_OK)
        {
            // no error has occurred
            // process the incoming message
            ESP_LOGI(TAG, "Incoming message: %s", input_buffer);

            // check if the recv buffer is empty (MAU not busy)
            if (!is_empty_recv())
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

            // dereference the pointer
            ret = process_payload(input_buffer, *(TaskHandle_t *)arg);

            if (ret == ESP_FAIL)
            {
                ESP_LOGE(TAG, "Payload parsing failed");
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

char error[ERROR_BUF_LEN];
char send[SEND_BUF_LEN];

/**
 * @brief Processes the error message from the MAU task
 *
 */
void process_error()
{
    if (is_empty_error())
        return;

    get_error(error);

    ESP_LOGI(TAG, "MAU error: %s", error);

    // send the error to the robot controller
    // error message is: MAU ERROR:
    char error_msg[10 + ERROR_BUF_LEN];

    sprintf(error_msg, "MAU ERROR:%s", error);

    esp_err_t ret = kawasaki_write_transmission(uart_robot, error_msg);

    if (ret != ESP_OK)
    {
        const char *error = esp_err_to_name(ret);
        ESP_LOGE(TAG, "Unable to send mau error message: %d -> %s", ret, error);
    }

    set_error("");
}

/**
 * @brief Porcesses the send buffer from the MAU task
 *
 */
void process_send()
{
    if (is_empty_send())
        return;

    get_send(send);

    ESP_LOGI(TAG, "Sending message from MAU to Robot: %s", send);

    esp_err_t ret = kawasaki_write_transmission(uart_robot, send);

    if (ret != ESP_OK)
    {
        const char *error = esp_err_to_name(ret);
        ESP_LOGE(TAG, "Unable to mau send buffer: %d -> %s", ret, error);
    }

    set_send("");
}