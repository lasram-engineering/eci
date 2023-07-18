#include "fiware_task.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_log.h>

#include "iot_agent.h"

static const char *TAG = "FIWARE Task";

QueueHandle_t fiware_iota_measurement_queue;

void fiware_task()
{
    // allocate the measurement queue
    fiware_iota_measurement_queue = xQueueCreate(CONFIG_FIWARE_TASK_MEASUREMENT_QUEUE_SIZE, sizeof(char) * CONFIG_FIWARE_TASK_MEASUREMENT_SIZE);

    // if the queue cannot be allocated, abort the program
    if (fiware_iota_measurement_queue == NULL)
    {
        ESP_LOGE(TAG, "Cannot allocate measurement queue");
        abort();
    }

    char payload[CONFIG_FIWARE_TASK_MEASUREMENT_SIZE];

    /* LOOP */
    while (1)
    {
        // block until a measurement payload comes in
        xQueueReceive(fiware_iota_measurement_queue, &payload, portMAX_DELAY);

        fiware_iota_make_measurement(payload);
    }
}
