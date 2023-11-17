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
#include "task_intercom.h"
#include "mau_task.h"

char *TAG = "UART";

/**
 * Configuration for UART robot
 */
static uart_config_t uart_config_robot = {
    .baud_rate = CONFIG_UART_BAUD,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};

const uart_port_t uart_robot = UART_NUM_1;
QueueHandle_t uart_queue_robot;

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
    ESP_ERROR_CHECK(
        uart_set_pin(
            uart_robot,
            CONFIG_UART_TX,
            CONFIG_UART_RX,
            CONFIG_UART_RTS,
            CONFIG_UART_CTS));

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
    char *payload = NULL;

    /* LOOP */
    while (1)
    {
        // clear the payload buffer
        if (payload != NULL)
            free(payload);
        payload = NULL;

        ret = kawasaki_read_transmission(uart_robot, &payload, pdMS_TO_TICKS(UART_TIMEOUT_MS));

        if (ret == ESP_ERR_TIMEOUT)
        {
            process_incoming_messages();

            continue;
        }

        if (ret == ESP_OK)
        {

            // check if it was not an empty message
            if (strlen(payload) == 0)
            {
                ESP_LOGI(TAG, "Payload is empty.");
                continue;
            }

            // no error has occurred
            // process the incoming message
            ESP_LOGI(TAG, "Incoming message: %s", payload);

            itc_message_t *message = task_intercom_message_create();
            task_intercom_message_init(message);

            // parse the message
            ret = kawasaki_parse_transmission(payload, &message);

            // check if the message parsing was successful or not
            if (ret == ESP_FAIL)
            {
                ret = kawasaki_write_transmission(uart_robot, "INVALID HEADER");

                if (ret != ESP_OK)
                {
                    const char *error = esp_err_to_name(ret);
                    ESP_LOGE(TAG, "Error sending INVALID HEADER to robot: %d -> %s", ret, error);
                }

                continue;
            }

            ESP_LOGI(TAG, "ITC(%ld) payload: %s", message->message_id, message->payload);

            if (message->is_measurement)
                ret = xQueueSend(task_intercom_fiware_measurement_queue, (void *)&message, 0);
            else
                ret = xQueueSend(task_intercom_mau_queue, (void *)&message, 0);

            // check if the message was added to the queue
            if (ret == errQUEUE_FULL)
            {
                // respond with busy message
                message->response = "BUSY";
                ret = kawasaki_make_response(uart_robot, message);

                if (ret != ESP_OK)
                {
                    const char *error = esp_err_to_name(ret);
                    ESP_LOGE(TAG, "Error sending %s to robot: %d -> %s", message->response, ret, error);
                }
            }

            continue;
        }

        // other errors
        const char *error = esp_err_to_name(ret);
        ESP_LOGE(TAG, "An error occurred: %d -> %s", ret, error);
    }
}

/**
 * @brief Processes incoming messages from the UART queue
 *
 */
esp_err_t process_incoming_messages()
{
    itc_message_t *incoming_message;

    int ret = xQueueReceive(task_intercom_uart_queue, &incoming_message, 0);

    // return if there is no message in the queue
    if (ret == pdFALSE)
        return ESP_ERR_NOT_FOUND;

    if (incoming_message->response == NULL && incoming_message->response_static == NULL)
        return ESP_ERR_INVALID_ARG;

    const char *response = incoming_message->response != NULL ? incoming_message->response : incoming_message->response_static;

    ESP_LOGI(TAG, "Sending message to robot: (ID: %ld) %s", incoming_message->message_id, response);

    // check if the incoming message is an error message
    ret = kawasaki_make_response(uart_robot, incoming_message);

    switch (ret)
    {
    case ESP_FAIL:
        ESP_LOGW(TAG, "Transmission failed: %s", response);
        break;

    case ESP_ERR_INVALID_RESPONSE:
        ESP_LOGW(TAG, "Invalid response to: %s", response);
        break;

    case ESP_ERR_NOT_FINISHED:
        ESP_LOGW(TAG, "Possible ENQ collision during sending %s", response);
        break;

    case ESP_ERR_TIMEOUT:
        ESP_LOGW(TAG, "Message timed out: %s", response);
        break;

    default:
        break;
    }

    task_intercom_message_delete(incoming_message);

    return ret;
}