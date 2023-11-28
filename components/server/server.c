#include "server.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <esp_log.h>
#include <esp_http_server.h>

#include "iot_agent.h"
#include "task_intercom.h"

#define RESPONSE_BUFFER_LENGTH 2 * (15 + APP_STATE_LENGTH) + 1

static const char *TAG = "Server";

esp_err_t get_handler(httpd_req_t *request)
{
    httpd_resp_send(request, "WIP", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

/**
 * @brief Handles incoming POST requests from the FIWARE IoT Agent
 *
 * @param request the incoming request
 * @return esp_err_t
 */
esp_err_t api_post_handler(httpd_req_t *request)
{
    // allocate the payload buffer
    size_t payload_buffer_size = request->content_len + 1;
    char *payload = (char *)calloc(sizeof(char), payload_buffer_size);

    // check if the allocation was successful
    if (payload == NULL)
    {
        ESP_LOGI(TAG, "Incoming POST payload too large: %d", request->content_len);
        // send back an error message
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Payload too large");
        return ESP_FAIL;
    }

    // receive the data
    int ret = httpd_req_recv(request, payload, request->content_len);

    if (ret <= 0)
    {
        // check if a timeout has occurred
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        {
            // send back a 408 reuqest timeout
            httpd_resp_send_408(request);
        }

        free(payload);

        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Got command: %s", payload);

    // allocate a new message
    itc_message_t *message = task_intercom_message_create();
    // initialize the message
    task_intercom_message_init(message);

    // set the payload to the message
    message->payload = strdup(payload);

    char *response = NULL;

    if (message->payload == NULL)
    {
        fiware_iota_command_make_response(payload, "NO MEM", &response);
        httpd_resp_send(request, response, HTTPD_RESP_USE_STRLEN);

        // message could not be added to the queue, free the resource
        task_intercom_message_delete(message);

        // free the allocated resources
        free(payload);
        return ESP_OK;
    }

    // send the command to the fiware task
    ret = xQueueSend(task_intercom_fiware_command_queue, &message, 0);

    if (ret == pdTRUE)
    {
        // command appended to queue, send back default success message
        fiware_iota_command_make_response(payload, CONFIG_IOT_AGENT_COMMAND_INIT_RESPONSE, &response);
        httpd_resp_send(request, response, HTTPD_RESP_USE_STRLEN);
    }
    else
    {
        // command queue full, send back BUSY
        fiware_iota_command_make_response(payload, "BUSY", &response);
        httpd_resp_send(request, response, HTTPD_RESP_USE_STRLEN);

        // message could not be added to the queue, free the resource
        task_intercom_message_delete(message);
    }

    // free the allocated resources
    free(payload);
    if (response != NULL)
        free(response);

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