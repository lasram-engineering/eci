#include "fiware_task.h"

#include <string.h>

#include <esp_log.h>

#include "iot_agent.h"
#include "task_intercom.h"

static const char *TAG = "FIWARE Task";

static const char *KW_PROGRAM_UPDATE = "program_update";

static itc_iota_measurement_t fiware_incoming_measurement;
static fiware_iota_command_t fiware_incoming_command;

static itc_uart_message_t uart_message;

void fiware_task()
{
    int ret;

    /* LOOP */
    while (1)
    {
        // block until a measurement payload comes in
        ret = xQueueReceive(task_intercom_fiware_measurement_queue, &fiware_incoming_measurement, pdMS_TO_TICKS(CONFIG_FIWARE_TASK_MEASUREMENT_TIMEOUT));

        // check if the queue had items
        if (ret == pdTRUE)
        {
            fiware_iota_make_measurement(fiware_incoming_measurement.payload);
            continue;
        }

        // if there was a timeout, check the commands
        ret = xQueuePeek(task_intercom_fiware_command_queue, &fiware_incoming_command, 0);

        if (ret == pdFALSE)
            continue;

        // check for program update command
        if (strcmp(fiware_incoming_command.command_name, KW_PROGRAM_UPDATE) == 0)
        {
            ESP_LOGI(TAG, "New porgam: %s", fiware_incoming_command.command_param);

            // load the program name into the message payload
            sprintf(uart_message.payload, "PROGRAM|%s", fiware_incoming_command.command_param);

            // send the message to the queue
            ret = xQueueSend(task_intercom_uart_queue, &uart_message, 0);

            if (ret == errQUEUE_FULL)
                ESP_LOGE(TAG, "Unable to send to UART task, queue full");

            // remove the item from the queue
            xQueueReceive(task_intercom_fiware_command_queue, &fiware_incoming_command, 0);
        }
    }
}
