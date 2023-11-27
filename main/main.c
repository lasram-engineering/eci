#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <nvs_flash.h>
#include <esp_netif_sntp.h>
#include <esp_http_server.h>

#include "server.h"
#include "wifi.h"
#include "task_intercom.h"
#include "uart_task.h"
#include "mau_task.h"
#include "fiware_task.h"

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

    // create the itc queues
    ESP_ERROR_CHECK(task_intercom_init());

#ifdef CONFIG_UART_TASK_ENABLE
    ESP_ERROR_CHECK(uart_start_task());
#endif

#ifdef CONFIG_FIWARE_TASK_ENABLE
    ESP_ERROR_CHECK(fiware_start_task());
#endif

    ESP_ERROR_CHECK(mau_start_task());

    wifi_wait_connected(portMAX_DELAY);

    server = start_http_server();
}
