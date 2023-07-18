#include "tasks/common.h"

#include "tasks/mau_task.h"
#include "uart_task.h"
#include "fiware_task.h"

#ifdef CONFIG_UART_TASK_ENABLE
static TaskHandle_t uart_task_handle;
#endif
#ifdef MAU_TASK
static TaskHandle_t mau_task_handle;
#endif

#ifdef CONFIG_FIWARE_TASK_ENABLE
static TaskHandle_t fiware_task_handle;
#endif

void initialize_tasks()
{
#ifdef CONFIG_UART_TASK_ENABLE
    xTaskCreate(uart_task, "uart_task", 4096, &mau_task_handle, MIN(CONFIG_UART_TASK_PRIO, configMAX - 1), &uart_task_handle);
#endif
#ifdef MAU_TASK
    xTaskCreate(mau_task, "mau_task", 4096, NULL, MAU_TASK_PRIO, &mau_task_handle);
#endif
#ifdef CONFIG_FIWARE_TASK_ENABLE
    xTaskCreate(fiware_task, "fiware_task", 2048, NULL, MIN(CONFIG_FIWARE_TASK_PRIO, configMAX_PRIORITIES - 1), &fiware_task_handle);
#endif
}