#include "server.h"

#include <esp_log.h>
#include <esp_http_server.h>

#include "app_state.h"
#include "task_intercom.h"

#define RESPONSE_BUFFER_LENGTH 2 * (15 + APP_STATE_LENGTH) + 1

#define PAYLOAD_LENGTH 255

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

static char payload[PAYLOAD_LENGTH];

/**
 * @brief Handles incoming POST requests from the FIWARE IoT Agent
 *
 * @param request the incoming request
 * @return esp_err_t
 */
esp_err_t api_post_handler(httpd_req_t *request)
{

    if (sizeof(payload) < request->content_len)
    {
        ESP_LOGI(TAG, "Incoming POST payload too large %d vs %d", PAYLOAD_LENGTH, request->content_len);
        // send back an error message
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Payload too large");
        return ESP_FAIL;
    }

    int ret = httpd_req_recv(request, payload, sizeof(payload));

    if (ret <= 0)
    {
        // check if a timeout has occurred
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        {
            // send back a 408 reuqest timeout
            httpd_resp_send_408(request);
        }

        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Got payload: %s", payload);

    // parse the payload
    // TODO parse the payload

    // check if the incoming command buffer is not empty
    if (!is_empty_incoming_command())
    {
        ESP_LOGI(TAG, "Incoming command buffer not empty, returning error...");
        httpd_resp_send_500(request);
        return ESP_FAIL;
    }

    // copy the payload into the incoming command buffer
    set_incoming_command(payload);

    httpd_resp_send(request, CONFIG_IOT_AGENT_COMMAND_INIT_RESPONSE, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

httpd_uri_t uri_get = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = get_handler,
    .user_ctx = NULL,
};

httpd_uri_t uri_api_post = {
    .uri = CONFIG_IOT_DEVICE_ENDPOINT,
    .method = HTTP_POST,
    .handler = api_post_handler,
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
        httpd_register_uri_handler(server, &uri_api_post);
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