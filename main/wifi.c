#include "wifi.h"

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <lwip/err.h>
#include <lwip/sys.h>

#include "app_state.h"

static const char *TAG = "WIFI";

static int s_retry_count = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START: // wifi station mode started
            // connect to station
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_CONNECTED: // connected to wifi
            ESP_LOGI(TAG, "Connected to station");

            // reset the error state
            app_state_unset(STATE_TYPE_ERROR, APP_STATE_ERROR_WIFI_CONNECTION);

            // reset the retry count
            s_retry_count = 0;
            break;

        case WIFI_EVENT_STA_DISCONNECTED: // wifi disconnected
            ESP_LOGI(TAG, "Disconnected from station, trying again...");
            app_state_unset(STATE_TYPE_INTERNAL, APP_STATE_INTERNAL_WIFI_CONNECTED);

            if (s_retry_count >= CONFIG_WIFI_RETRY_ERROR_THRESHOLD)
            {
                // after the max number of retries, set the wifi error state
                app_state_set(STATE_TYPE_ERROR, APP_STATE_ERROR_WIFI_CONNECTION);
            }

            s_retry_count++;

            esp_wifi_connect();
            break;
        default:
            break;
        }
    }

    else if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));

            // set the bit to end the blocking wait
            app_state_set(STATE_TYPE_INTERNAL, APP_STATE_INTERNAL_WIFI_CONNECTED);
        }
    }
}
void wifi_connect_to_station()
{
    ESP_LOGI(TAG, "Setting up WiFi in station mode");

    // get the default handle for the net interface
    esp_netif_t *netif_handle = NULL;

    netif_handle = esp_netif_next(netif_handle);

    // set the hostname of the device
    esp_netif_set_hostname(netif_handle, CONFIG_WIFI_HOST);

    // initialize net interface
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();

    // create wifi config and initialize
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // create and register event handlers
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // create custom wifi config
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Station mode setup completed");

    // wait until the connection flag WIFI_CONNECTED_BIT is set
    // it is set in the wifi_event_handler function
    app_state_wait_for_event(STATE_TYPE_INTERNAL, APP_STATE_INTERNAL_WIFI_CONNECTED);

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
}