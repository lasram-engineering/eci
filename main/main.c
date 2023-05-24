#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <nvs_flash.h>
#include <esp_netif.h>

#include "server.h"
#include "wifi.h"
#include "app_state.h"
#include "app_tasks.h"
#include "io.h"
#include "fiware.h"

#define ERROR_TASK_ENABLED 1
#define INPUT_TASK_ENABLED 1
#define ANALOG_TASK_ENABLED 1

static httpd_handle_t server = NULL;

// task handles
static TaskHandle_t error_task_handle = NULL;
static TaskHandle_t input_task_handle = NULL;
static TaskHandle_t analog_task_handle = NULL;

void app_main(void)
{
    // initialize application state
    ESP_ERROR_CHECK(app_state_init());

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

// start tasks
#if ERROR_TASK_ENABLED
    xTaskCreate(error_task, "Error task", 4096, NULL, 10, &error_task_handle);
#endif
#if INPUT_TASK_ENABLED
    xTaskCreate(input_task, "Input task", 4096, NULL, 10, &input_task_handle);
#endif
#if ANALOG_TASK_ENABLED
    xTaskCreate(analog_measure_task, "Analog task", 4096, NULL, 10, &analog_task_handle);
#endif

    // connect to wifi network
    // blocking call
    wifi_connect_to_station();

    // start http server
    server = start_http_server();

    // check the health of the server
    esp_err_t server_online = fiware_check_health();

    if (server_online != ESP_OK)
        app_state_set(STATE_TYPE_ERROR, APP_STATE_ERROR_IOTA_OFFLINE);

    while (server_online != ESP_OK)
    {
        server_online = fiware_check_health();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    app_state_unset(STATE_TYPE_ERROR, APP_STATE_ERROR_IOTA_OFFLINE);
}
