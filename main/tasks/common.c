#include "tasks/common.h"

#include <freertos/task.h>
#include <esp_log.h>

#include "tasks/uart_task.h"

#ifdef UART_TASK
static TaskHandle_t uart_task_handle = NULL;
#endif

void initialize_tasks()
{
#ifdef UART_TASK
    xTaskCreate(uart_task, "uart_task", 4096, NULL, UART_TASK_PRIO, &uart_task_handle);
#endif
}