#include <fiware.h>

#include <stdio.h>
#include <string.h>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_http_client.h>
#include <driver/uart.h>

#include "app_state.h"

#define IOT_AGENT_HEALTHCHECK_URL "http://" CONFIG_IOT_AGENT_HOST ":" CONFIG_IOT_AGENT_NORTH_PORT "/iot/about"
#define IOT_AGENT_URL "http://" CONFIG_IOT_AGENT_HOST ":" CONFIG_IOT_AGENT_SOUTH_PORT CONFIG_IOT_AGENT_RESOURCE "?k=" CONFIG_IOT_AGENT_APIKEY "&i=" CONFIG_IOT_AGENT_DEVICE_ID

static char *TAG = "FIWARE IoT Agent";

char *make_payload(const char name, const char *value)
{
    const int payload_length = snprintf(NULL, 0, "%c|%s", name, value);

    char *payload = (char *)malloc((payload_length + 1) * sizeof(char));

    if (payload == NULL)
    {
        ESP_LOGE(TAG, "Unable to allocate memory to make payload");
        return payload;
    }

    sprintf(payload, "%c|%s", name, value);

    return payload;
}

esp_err_t extend_payload(char *base_string, const char name, const char *value)
{
    const int payload_length = snprintf(NULL, 0, "%s|%c|%s", base_string, name, value);

    base_string = realloc(base_string, (payload_length + 1) * sizeof(char));

    if (base_string == NULL)
    {
        ESP_LOGE(TAG, "Unable to reallocate memory to extend payload");
        return ESP_ERR_NO_MEM;
    }

    strcat(base_string, "|");
    strncat(base_string, &name, 1);
    strcat(base_string, "|");
    strcat(base_string, value);

    return ESP_OK;
}

esp_err_t _http_silent_event_handle(esp_http_client_event_t *event)
{
    switch (event->event_id)
    {
    case HTTP_EVENT_ERROR:
        return ESP_ERR_HTTP_CONNECT;

    default:
        return ESP_OK;
    }
}

static esp_http_client_config_t update_attribute_config = {
    .url = IOT_AGENT_URL,
    .method = HTTP_METHOD_POST,
    .event_handler = _http_silent_event_handle,
};

static esp_http_client_config_t get_health_config = {
    .url = IOT_AGENT_HEALTHCHECK_URL,
    .method = HTTP_METHOD_GET,
    .event_handler = _http_silent_event_handle,
};

esp_err_t fiware_update_attribute(const char name, const char *value)
{
    char *attribute = make_payload(name, value);

    fiware_update_attribute_raw(attribute);
    free(attribute);

    return ESP_OK;
}

esp_err_t fiware_update_attribute_raw(const char *raw_attr_payload)
{
    if (!(app_state_get(STATE_TYPE_INTERNAL) & (1 << APP_STATE_INTERNAL_WIFI_CONNECTED)))
    {
        return ESP_ERR_WIFI_CONN;
    }

    // initialize the client
    esp_http_client_handle_t client = esp_http_client_init(&update_attribute_config);

    // set the POST data field
    esp_http_client_set_post_field(client, raw_attr_payload, strlen(raw_attr_payload));

    // set the header
    esp_http_client_set_header(client, "Content-Type", "text/plain");

    // send the request
    esp_err_t err = esp_http_client_perform(client);
    ESP_LOGI(TAG, "Making POST request to %s", IOT_AGENT_URL);

    esp_err_t ret;

    if (err == ESP_OK)
    {
        int status = esp_http_client_get_status_code(client);

        if (status == 404)
        {
            app_state_set(STATE_TYPE_ERROR, APP_STATE_ERROR_IOTA_DEVICE_NOT_REGISTERED);
            ESP_LOGW(
                TAG,
                "Error: IoT Device not registered (id: %s, apikey: %s)",
                CONFIG_IOT_AGENT_DEVICE_ID,
                CONFIG_IOT_AGENT_APIKEY);

            ret = ESP_ERR_HTTP_BASE;
        }
        else if (status >= 300)
        {
            app_state_set(STATE_TYPE_ERROR, APP_STATE_ERROR_IOTA_ERROR);
            ESP_LOGW(TAG, "Error while updating attribute: %d", status);

            ret = ESP_ERR_HTTP_BASE;
        }
        else
        {
            ESP_LOGI(TAG, "Set attributes of device '%s' to '%s'", CONFIG_IOT_AGENT_DEVICE_ID, raw_attr_payload);
            ret = ESP_OK;
        }
    }
    else
    {
        ret = err;
    }

    // cleanup
    esp_http_client_cleanup(client);

    return ret;
}

esp_err_t fiware_check_health()
{
    if (!(app_state_get(STATE_TYPE_INTERNAL) & (1 << APP_STATE_INTERNAL_WIFI_CONNECTED)))
    {
        return ESP_ERR_WIFI_CONN;
    }

    esp_http_client_handle_t client = esp_http_client_init(&get_health_config);

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        // get the status code
        int status_code = esp_http_client_get_status_code(client);

        if (status_code >= 300)
        {
            ESP_LOGI(TAG, "FIWARE IoT Agent: invalid response: %d", status_code);
            return ESP_ERR_HTTP_CONNECT;
        }

        ESP_LOGI(TAG, "FIWARE IoT Agent: online");
        return ESP_OK;
    }
    else
    {
        ESP_LOGI(TAG, "FIWARE IoT Agent: unreachable");
        return ESP_ERR_HTTP_CONNECT;
    }
}
