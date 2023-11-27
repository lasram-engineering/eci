#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <nvs_flash.h>
#include <esp_netif_sntp.h>
#include <esp_http_server.h>

#include "server.h"
#include "wifi.h"
#include "task_manager.h"

static httpd_handle_t server = NULL;

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // synchronize network time
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&config);

    // connect to wifi network
    ESP_ERROR_CHECK(wifi_connect_to_station());

    // start tasks
    initialize_tasks();

    wifi_wait_connected(portMAX_DELAY);

    server = start_http_server();
}
