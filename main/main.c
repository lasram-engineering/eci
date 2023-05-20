#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <nvs_flash.h>
#include <esp_netif.h>

#include "server.h"
#include "wifi.h"
#include "app_state.h"

static httpd_handle_t server = NULL;

void app_main(void)
{
    // initialize application state
    ESP_ERROR_CHECK(app_state_init());

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    // ESP_ERROR_CHECK(example_connect());
    wifi_connect_to_station();

    // start http server
    server = start_http_server();
}
