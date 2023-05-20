#include "server.h"

#include <esp_log.h>
#include <esp_http_server.h>

#include <fiware.h>

static const char *TAG = "Server";

esp_err_t get_handler(httpd_req_t *request)
{
    const char response[] = "GET Response: Running";
    httpd_resp_send(request, response, HTTPD_RESP_USE_STRLEN);

    update_attribute('c', "1");

    return ESP_OK;
}

httpd_uri_t uri_get = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = get_handler,
    .user_ctx = NULL,
};

httpd_handle_t
start_http_server()
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