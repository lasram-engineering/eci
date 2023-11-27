#include "wifi.h"

#include <esp_wifi.h>
#include <esp_check.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#define MIN(a, b) ((a) < (b)) ? (a) : (b)

// check the wifi task prio
#if CONFIG_WIFI_TASK_PRIORITY >= configMAX_PRIORITIES
#warning "WiFi Task priority is set higher than the maximum priority"
#endif

static const char *TAG = "WiFi";

static TaskHandle_t wifi_task_handle = NULL;

EventGroupHandle_t wifi_events = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    static int retries = 0;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "Retrying to connect to station... (%d)", ++retries);
        esp_wifi_connect();
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        retries = 0;
        // set the event bits
        xEventGroupSetBits(wifi_events, WIFI_CONNECTED);
        return;
    }
}

void wifi_connect_to_station_task()
{
    int ret = ESP_OK;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_GOTO_ON_ERROR(
        esp_wifi_init(&init_config),
        exit_task,
        TAG,
        "Unable to initialize wifi");

    // wifi is now initialized
    xEventGroupSetBits(wifi_events, WIFI_INITIALIZED);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_STATION_NAME,
            .password = CONFIG_WIFI_STATION_PASSWORD,
        }};

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    esp_event_handler_instance_t instance_wifi_any;
    esp_event_handler_instance_t instance_ip_got_ip;

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL,
            &instance_wifi_any));

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_event_handler,
            NULL,
            &instance_ip_got_ip));

    ESP_GOTO_ON_ERROR(
        esp_wifi_start(),
        exit_task,
        TAG,
        "Unable to start wifi");

    // wait for the WIFI to connect
    xEventGroupWaitBits(wifi_events, WIFI_CONNECTED, pdFALSE, pdTRUE, portMAX_DELAY);

exit_task:
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "Error code (%d) %s", ret, esp_err_to_name(ret));

    vTaskDelete(NULL);
}

/**
 * @brief Creates a task that tries to connect to the configured access point.
 * If the task is already running the function returns.
 *
 * @return esp_err_t ESP_OK if the task has started successfully
 * @return esp_err_t ESP_FAIL if the task could not start
 */
esp_err_t wifi_connect_to_station()
{
    // check if the task handle is set to NULL
    if (wifi_task_handle != NULL)
        return ESP_OK; // connection task is already running

    if (wifi_events == NULL)
        wifi_events = xEventGroupCreate();

    if (wifi_events == NULL)
        return ESP_ERR_NO_MEM;

    // launch the wifi task
    int ret = xTaskCreate(
        wifi_connect_to_station_task,
        TAG,
        CONFIG_WIFI_TASK_STACK_DEPTH,
        NULL,
        MIN(configMAX_PRIORITIES - 1, CONFIG_WIFI_TASK_PRIORITY),
        &wifi_task_handle);

    return ret == pdPASS ? ESP_OK : ESP_FAIL;
}

bool is_wifi_initialized()
{
    if (wifi_events == NULL)
        return false;

    return xEventGroupGetBits(wifi_events) & WIFI_INITIALIZED;
}

bool is_wifi_connected()
{
    if (wifi_events == NULL)
        return false;

    return xEventGroupGetBits(wifi_events) & WIFI_CONNECTED;
}

esp_err_t wifi_wait_initialized(TickType_t ticks_to_wait)
{
    if (wifi_events == NULL)
        return ESP_ERR_INVALID_STATE;

    EventBits_t bits = xEventGroupWaitBits(wifi_events, WIFI_INITIALIZED, pdFALSE, pdTRUE, ticks_to_wait);

    return bits & WIFI_INITIALIZED ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t wifi_wait_connected(TickType_t ticks_to_wait)
{
    if (wifi_events == NULL)
        return ESP_ERR_INVALID_STATE;

    EventBits_t bits = xEventGroupWaitBits(wifi_events, WIFI_CONNECTED, pdFALSE, pdTRUE, ticks_to_wait);

    return bits & WIFI_CONNECTED ? ESP_OK : ESP_ERR_TIMEOUT;
}