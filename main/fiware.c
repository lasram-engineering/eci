#include <fiware.h>

#include <stdio.h>
#include <string.h>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_http_client.h>
#include <driver/uart.h>

#define IOT_AGENT_URL "http://" CONFIG_IOT_AGENT_HOST ":" CONFIG_IOT_AGENT_PORT CONFIG_IOT_AGENT_RESOURCE
#define IOT_AGENT_QUERY "?k=" CONFIG_IOT_AGENT_APIKEY "&i=" CONFIG_IOT_AGENT_DEVICE_ID

#define IOT_AGENT_URI IOT_AGENT_URL IOT_AGENT_QUERY

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

esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_REDIRECT:
        ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
        break;
    case HTTP_EVENT_ERROR:
        ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER %s -> %s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA");
        printf("%.*s", evt->data_len, (char *)evt->data);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

static esp_http_client_config_t config = {
    .url = IOT_AGENT_URL,
    .method = HTTP_METHOD_GET,
    .event_handler = _http_event_handle,
};

esp_err_t update_attribute(const char name, const char *value)
{
    // initialize the client
    esp_http_client_handle_t client = esp_http_client_init(&config);
    ESP_LOGI(TAG, "Making request to %s...", config.url);

    esp_err_t err = esp_http_client_perform(client);

    char *attribute = make_payload(name, value);

    ESP_LOGI(TAG, "Attribute: %s", attribute);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }

    // cleanup
    esp_http_client_cleanup(client);
    free(attribute);

    return ESP_OK;
}
