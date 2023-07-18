#include "task_manager.h"

#include "mau_task.h"
#include "uart_task.h"
#include "fiware_task.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifdef CONFIG_UART_TASK_ENABLE
static TaskHandle_t uart_task_handle;
#endif

#ifdef CONFIG_FIWARE_TASK_ENABLE
static TaskHandle_t fiware_task_handle;
#endif

static TaskHandle_t mau_task_handle;

void initialize_tasks()
{
    xTaskCreate(mau_task, "mau_task", 4096, NULL, MIN(CONFIG_MAU_TASK_PRIO, configMAX_PRIORITIES - 1), &mau_task_handle);

#ifdef CONFIG_UART_TASK_ENABLE
    xTaskCreate(uart_task, "uart_task", 4096, &mau_task_handle, MIN(CONFIG_UART_TASK_PRIO, configMAX_PRIORITIES - 1), &uart_task_handle);
#endif
#ifdef CONFIG_FIWARE_TASK_ENABLE
    xTaskCreate(fiware_task, "fiware_task", 4096, NULL, MIN(CONFIG_FIWARE_TASK_PRIO, configMAX_PRIORITIES - 1), &fiware_task_handle);
#endif
}