#include "server.h"

#include <esp_log.h>
#include <esp_http_server.h>

#include "app_state.h"

#define RESPONSE_BUFFER_LENGTH 2 * (15 + APP_STATE_LENGTH) + 1

static const char *TAG = "Server";

static char response[RESPONSE_BUFFER_LENGTH];

esp_err_t get_handler(httpd_req_t *request)
{
    strcpy(response, "Error buffer: ");

    EventBits_t error_bits = app_state_get(STATE_TYPE_ERROR);

    int i;
    for (i = APP_STATE_LENGTH - 1; 0 <= i; i--)
    {
        char *bit_str = error_bits & BIT(i) ? "1" : "0";
        strcat(response, bit_str);
    }

    strcat(response, "\nInput buffer: ");

    error_bits = app_state_get(STATE_TYPE_INPUT);

    ESP_LOGI(TAG, "APP input state %ld", error_bits);

    for (i = APP_STATE_LENGTH - 1; 0 <= i; i--)
    {
        char *bit_str = error_bits & BIT(i) ? "1" : "0";
        strcat(response, bit_str);
    }

    httpd_resp_send(request, response, HTTPD_RESP_USE_STRLEN);

    strcpy(response, "");

    return ESP_OK;
}

httpd_uri_t uri_get = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = get_handler,
    .user_ctx = NULL,
};

httpd_handle_t start_http_server()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // emtpy handle to server
    httpd_handle_t server = NULL;

    // start the server
    esp_err_t server_start_result = httpd_start(&server, &config);
    if (server_start_result == ESP_OK)
    {
        // register handlers
        httpd_register_uri_handler(server, &uri_get);
        ESP_LOGI(TAG, "HTTP server started successfully");
    }
    else
    {
        ESP_LOGW(TAG, "Unable to start server (Error code: %04x)", server_start_result);
    }

    return server;
}
void stop_http_server(httpd_handle_t server)
{
    if (server)
    {
        httpd_stop(server);
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}