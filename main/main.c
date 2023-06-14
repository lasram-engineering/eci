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
#include "tasks/common.h"

#define ERROR_TASK_ENABLED 1
#define INPUT_TASK_ENABLED 1
#define ANALOG_TASK_ENABLED 1

static httpd_handle_t server = NULL;

void app_main(void)
{
    // initialize application state
    ESP_ERROR_CHECK(app_state_init());

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // start tasks
    initialize_tasks();

    // connect to wifi network
    // blocking call
#ifdef CONFIG_WIFI_ENABLED
    wifi_connect_to_station();
    // start http server
    server = start_http_server();
#endif

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
