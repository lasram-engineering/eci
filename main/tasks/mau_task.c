#include "tasks/mau_task.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <string.h>

#include "uart.h"

static const char *TAG = "MAU";

// The spinlock allows the task to enter and exit critical sections
static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

// Used to signal to the uart receive task that the task is processing
SemaphoreHandle_t mau_lock;

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

    // allocate the semaphore
    mau_lock = xSemaphoreCreateBinary();

    if (mau_lock == NULL)
        abort();

    // give the semaphore
    xSemaphoreGive(mau_lock);

    ESP_LOGI(TAG, "Initialization done");

    esp_err_t ret;
    Mau_Response_t response;

    while (1)
    {
        // wait for the task to be notified
        // does not clear bits on entry to wait
        // clears all the bits on exit
        // does not store the incoming index
        // waits indefinitely
        xTaskNotifyWait(0x00, ULONG_MAX, NULL, portMAX_DELAY);

        ESP_LOGI(TAG, "Incoming message: %s", incoming_message);

        // take the semaphore
        xSemaphoreTake(mau_lock);

        // send the message to the MAU
        ret = uart_write_bytes(uart_num, incoming_message, sizeof(incoming_message));

        // receive the response from the MAU
        ret = uart_read_string(uart_num, uart_buffer, UART_BUFFER_LEN, UART_READ_TIMEOUT_MS / portTICK_PERIOD_MS);

        if (ret != ESP_OK)
        {
            // an error has occurred
            const char *error_msg = esp_err_to_name(ret);
            ESP_LOGW(TAG, "An error occurred while reading from MAU: %d -> %s", ret, error_msg);
            response.error_code = ret;
            strcpy(response.response, error_msg);
        }
        else
        {
            // the response was read
            strcpy(response.response, uart_buffer);
        }

        // enter critical to write to the response
        taskENTER_CRITICAL(&spinlock);
        // copy the response itself
        strcpy(response_message.response, response.response);
        // set the error flag
        response_message.error_code = response.error_code;
        taskEXIT_CRITICAL(&spinlock);

        xSemaphoreGive(mau_lock);

        // TODO: Implement notification to the UART respond task
        // TODO: add task busy lock
    }
}

/**
 * @brief Sets the message string if there is no message being processed
 *
 * @param message the message to be set
 * @return esp_err_t ESP_FAIL if the message cannot be set else ESP_OK
 */
esp_err_t set_message(const char *message)
{
    // check if the semaphore is available
    if (xSemaphoreTake(mau_lock, 0) == pdFALSE)
        return ESP_FAIL;

    // if the semaphore can be taken, update the message
    strcpy(incoming_message, message);

    return ESP_OK;
}