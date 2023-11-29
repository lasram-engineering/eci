#include "vreg.h"

#include <string.h>

#include <esp_check.h>
#include <driver/uart.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "task_intercom.h"

#define MIN(a, b) ((a) < (b)) ? (a) : (b)

static const char *TAG = "Voltage Regulator";

static const char *KW_VOLTAGE = "VOLTAGE";

static TaskHandle_t vreg_task_handle = NULL;

static QueueHandle_t vreg_uart_queue;

esp_err_t vreg_set_voltage(uart_port_t uart_num, uint16_t mvolts)
{
    char *vreg_message;
    int ret;

    ret = asprintf(&vreg_message, "V1 %f\r\n", (float)mvolts / 1000);

    if (ret == -1)
        return ESP_ERR_NO_MEM;

    uart_write_bytes(uart_num, vreg_message, strlen(vreg_message));

    // TODO imlpement waiting for a response

    free(vreg_message);

    return ESP_OK;
}

void vreg_task()
{
    // setup the uart interface
    uart_port_t uart_num = UART_NUM_2;
    uart_config_t uart_config = {
        .baud_rate = CONFIG_VREG_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

    // set up the pins
    ESP_ERROR_CHECK(
        uart_set_pin(
            uart_num,
            CONFIG_VREG_TX_PIN,
            CONFIG_VREG_RX_PIN,
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE));

    // install drivers
    ESP_ERROR_CHECK(
        uart_driver_install(
            uart_num,
            CONFIG_VREG_RX_BUFFER_SIZE,
            CONFIG_VREG_TX_BUFFER_SIZE,
            CONFIG_VREG_QUEUE_SIZE,
            &vreg_uart_queue,
            0));

    int ret;
    itc_message_t *message;

    //* LOOP
    while (1)
    {
        xQueuePeek(task_itc_from_uart_queue, &message, portMAX_DELAY);

        // Voltage regulator controls
        //    0         1
        // VOLTAGE | <value>

        if (message->token_num != 2)
            continue;

        if (task_itc_message_token_match(message, 0, KW_VOLTAGE) != ESP_OK)
            continue;

        // matching message, remove it from the queue
        ret = xQueueReceive(task_itc_from_uart_queue, &message, 0);

        if (ret != pdTRUE)
        {
            ESP_LOGE(TAG, "Error receiving message from queue");
            continue;
        }

        uint16_t mvolts = atoi(message->tokens[1]);

        ESP_LOGI(TAG, "Setting voltage to %fV", (float)mvolts / 1000);

        ret = vreg_set_voltage(uart_num, mvolts);

        if (ret == ESP_OK)
            message->response_static = "OK";

        else
            message->response_static = esp_err_to_name(ret);

        xQueueSend(task_itc_to_uart_queue, &message, portMAX_DELAY);
    }
}

esp_err_t vreg_start_task()
{
    if (vreg_task_handle != NULL)
        return ESP_ERR_INVALID_STATE;

    int ret = xTaskCreate(
        vreg_task,
        TAG,
        CONFIG_VREG_TASK_STACK_DEPTH,
        NULL,
        MIN(CONFIG_VREG_TASK_PRIO, configMAX_PRIORITIES - 1),
        &vreg_task_handle);

    return ret == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
