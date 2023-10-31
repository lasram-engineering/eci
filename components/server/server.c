#include "server.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <esp_log.h>
#include <esp_http_server.h>

#include "app_state.h"
#include "task_intercom.h"

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

static char payload[CONFIG_HTTP_SERVER_PAYLOAD_LEN];
static fiware_iota_command_t incoming_command;

/**
 * @brief Handles incoming POST requests from the FIWARE IoT Agent
 *
 * @param request the incoming request
 * @return esp_err_t
 */
esp_err_t api_post_handler(httpd_req_t *request)
{
    // reset payload
    payload[0] = '\0';

    if (CONFIG_HTTP_SERVER_PAYLOAD_LEN <= request->content_len)
    {
        ESP_LOGI(TAG, "Incoming POST payload too large %d vs %d", CONFIG_HTTP_SERVER_PAYLOAD_LEN, request->content_len);
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

    ESP_LOGI(TAG, "Got command: %s", payload);

    // parse the payload
    ret = fiware_iota_parse_command(payload, &incoming_command);

    if (ret == ESP_FAIL)
    {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Could not parse command");
        return ESP_OK;
    }

    if (ret == ESP_ERR_INVALID_ARG)
    {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid arguments");
        return ESP_OK;
    }

    if (ret == ESP_ERR_INVALID_SIZE)
    {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Command name or payload too large");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Command was valid");

    // send the command to the fiware task
    ret = xQueueSend(task_intercom_fiware_command_queue, &incoming_command, 0);
    httpd_resp_set_type(request, "text/plain");

    if (ret == pdTRUE)
    {
        fiware_iota_make_command_response(incoming_command.command_name, CONFIG_IOT_AGENT_COMMAND_INIT_RESPONSE, payload, CONFIG_HTTP_SERVER_PAYLOAD_LEN);
        httpd_resp_send(request, payload, HTTPD_RESP_USE_STRLEN);
    }

    else
    {

        fiware_iota_make_command_response(incoming_command.command_name, "BUSY", payload, CONFIG_HTTP_SERVER_PAYLOAD_LEN);
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, payload);
    }

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