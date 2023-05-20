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

#define WIFI_CONNECTED_BIT BIT0

typedef struct
{
    int32_t error_flag;
    char *error_msg;
} wifi_error_data;

void set_app_state_on_wifi_error(app_state_t *state, void *error)
{
    wifi_error_data *error_data = (wifi_error_data *)error;
    state->error |= error_data->error_flag;
    strcpy(state->error_msg, error_data->error_msg);
}

void set_app_state_on_wifi_connection(app_state_t *state, void *connected)
{
    state->error &= ~APP_STATE_WIFI_CONNECT_ERROR;
    state->wifi_connected = *(bool *)connected;
}

static int s_retry_count = 0;

static EventGroupHandle_t s_wifi_event_group;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        bool connected;
        switch (event_id)
        {
        case WIFI_EVENT_STA_START: // wifi station mode started
            // connect to station
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_CONNECTED: // connected to wifi
            ESP_LOGI(TAG, "Connected to station");
            // reset the retry count
            s_retry_count = 0;
            connected = true;
            app_state_modify(set_app_state_on_wifi_connection, &connected);
            break;

        case WIFI_EVENT_STA_DISCONNECTED: // wifi disconnected
            ESP_LOGI(TAG, "Disconnected from station, trying again...");
            connected = false;
            app_state_modify(set_app_state_on_wifi_connection, &connected);

            if (s_retry_count >= CONFIG_WIFI_RETRY_ERROR_THRESHOLD)
            {
                // after the max number of retries, set the wifi error state
                wifi_error_data data = {
                    .error_flag = APP_STATE_WIFI_CONNECT_ERROR,
                    .error_msg = "Unable to connect to WiFi",
                };
                app_state_modify(set_app_state_on_wifi_error, &data);
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
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}
void wifi_connect_to_station()
{
    ESP_LOGI(TAG, "Setting up WiFi in station mode");
    s_wifi_event_group = xEventGroupCreate();

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
    xEventGroupWaitBits(s_wifi_event_group,
                        WIFI_CONNECTED_BIT,
                        pdFALSE,
                        pdFALSE,
                        portMAX_DELAY);

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    vEventGroupDelete(s_wifi_event_group);
}