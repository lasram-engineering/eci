#include "fiware_task.h"

#include <esp_log.h>

#include "iot_agent.h"
#include "task_intercom.h"

static const char *TAG = "FIWARE Task";

static itc_iota_measurement_t fiware_incoming_measurement;
static fiware_iota_command_t fiware_incoming_command;

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
        ret = xQueueReceive(task_intercom_fiware_command_queue, &fiware_incoming_command, 0);

        if (ret == pdFALSE)
            continue;

        // TODO implement additional task logic from docs
    }
}
