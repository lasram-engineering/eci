#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <nvs_flash.h>
#include <esp_netif_sntp.h>

#include "server.h"
#include "wifi.h"
#include "app_state.h"
#include "task_manager.h"

static httpd_handle_t server = NULL;

void app_main(void)
{
    // initialize application state
    ESP_ERROR_CHECK(app_state_init());

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // synchronize network time
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&config);

    // start tasks
    initialize_tasks();

    // connect to wifi network
    // blocking call
#ifdef CONFIG_WIFI_ENABLED
    wifi_connect_to_station();

    // start http server
    server = start_http_server();
#endif
}
