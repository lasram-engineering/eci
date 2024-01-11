/// @file
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_netif_sntp.h>
#include <esp_http_server.h>

#include "server.h"
#include "wifi.h"
#include "task_intercom.h"
#include "uart_task.h"
#include "fiware_task.h"
#include "stepper.h"
#include "vreg.h"
#include "ph.h"

static const char *TAG = "Main";

static httpd_handle_t server = NULL;

/**
 * @brief Main entrypoint of the application
 *
 * @details The app initializes the Non-volatile storage (NVS).
 *  Then it sets up the ntp time sync.
 *  WiFi and inter-task communication is initialized.
 *  The following tasks are spawned: UART, Stepper, Voltage regulator, pH measurement, FIWARE
 *  After these tasks are successfully started, the http server is started
 *
 *  @see Task functions: uart_start_task(), stepper_start_task(), vreg_start_task(), ph_start_task(), fiware_start_task()
 *  @see Other initializing functions: wifi_connect_to_station(), task_intercom_init(), start_http_server()
 */
void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGI(TAG, "Erasing NVS Flash");
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

#ifdef CONFIG_STEPPER_MOTOR_ENABLED
    ESP_ERROR_CHECK(stepper_start_task());
#endif

#ifdef CONFIG_VREG_ENABLED
    ESP_ERROR_CHECK(vreg_start_task());
#endif

#ifdef CONFIG_PH_ENABLED
    ESP_ERROR_CHECK(ph_start_task());
#endif

#ifdef CONFIG_FIWARE_TASK_ENABLE
    ESP_ERROR_CHECK(fiware_start_task());
#endif

    wifi_wait_connected(portMAX_DELAY);

    server = start_http_server();
}
