#include "iot_agent.h"

#include <string.h>

#include <esp_http_client.h>
#include <esp_log.h>

#include "wifi.h"

#define FIWARE_IOTA_MEAS_QUERY "?i=" CONFIG_IOT_AGENT_DEVICE_ID "&k=" CONFIG_IOT_AGENT_APIKEY

#define RESPONSE_BUF_LEN 255

static const char *TAG = "IoT Agent";

esp_err_t fiware_iota_http_event_handler(esp_http_client_event_handle_t event)
{
    switch (event->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG, "Incoming data: %s", (char *)event->data);

        // char *data_buffer = event->user_data;

        // int content_len = esp_http_client_get_content_length(event->client);

        // data_buffer = malloc(sizeof(char) * (content_len + 1));

        // strcpy(data_buffer, event->data);
        break;

    default:
        break;
    }

    return ESP_OK;
}

static const esp_http_client_config_t measurement_config = {
    .host = CONFIG_FIWARE_HOST,
    .port = CONFIG_IOT_AGENT_SOUTH_PORT,
    .path = CONFIG_IOT_AGENT_RESOURCE FIWARE_IOTA_MEAS_QUERY,
    .method = HTTP_METHOD_POST,
    .event_handler = fiware_iota_http_event_handler,
    .cert_pem = NULL,
};

/**
 * @brief Upload an IoT Device measurement to the IoT Agent
 *
 * @param payload the payload formatted in Ultralight 2.0
 * @return esp_err_t    ESP_OK if the operation was successful,
 *                      ESP_ERR_INVALID_STATE if wifi connection is not available
 */
esp_err_t
fiware_iota_make_measurement(const char *payload, FiwareAccessToken_t *token, int *status_code)
{
    // check if wifi is not connected
    if (!is_wifi_connected())
    {
        return ESP_ERR_INVALID_STATE;
    }

    // create a new client with the request
    esp_http_client_handle_t client = esp_http_client_init(&measurement_config);

    // if the token is not null, attach the auth values to the request
    if (token != NULL)
    {
        // check if the token is expired
        if (fiware_idm_check_is_token_expired(token))
        {
            ESP_LOGW(TAG, "FIWARE Auth token expired, renewing token...");
            fiware_idm_renew_access_token(token);
        }

        fiware_idm_attach_auth_data_to_request(token, client);
    }

    // put the payload in the post field
    esp_http_client_set_post_field(client, payload, strlen(payload));

    // set the content type header
    esp_http_client_set_header(client, "Content-Type", "text/plain");

    // process the request itself
    int ret = esp_http_client_perform(client);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error sending request.");
        esp_http_client_cleanup(client);
        return ret;
    }

    // get the status code of the event
    ret = esp_http_client_get_status_code(client);

    if (ret == 404)
    {
        ESP_LOGW(TAG, "Could not find device with apikey '" CONFIG_IOT_AGENT_APIKEY "' maybe service group is not registered?");
    }
    else if (ret == 422)
    {
        ESP_LOGW(TAG, "Could not find device with id '" CONFIG_IOT_AGENT_DEVICE_ID "' maybe device is not registered or invalid poperty?");
    }
    else if (ret == 401)
    {
        ESP_LOGW(TAG, "Unauthorized.");
    }
    else if (ret >= 300)
    {
        ESP_LOGW(TAG, "Unexpected status from server: %d", ret);
    }

    if (status_code != NULL)
    {
        *status_code = ret;
    }

    esp_http_client_cleanup(client);

    return ESP_OK;
}

/**
 * @brief Parses UltraLight 2.0 commands from the IoT Agent
 *
 * @param raw_command the raw command string to parse
 * @param command the command type to load the values into
 * @return esp_err_t    ESP_ERR_INVALID_ARG if the command contained illegal fields
 *                      ESP_FAIL if the command could not be parsed
 *                      ESP_ERR_INVALID_SIZE if the command name or payload was too large
 */
esp_err_t fiware_iota_parse_command(char *raw_command, fiware_iota_command_t *command)
{
    // * command syntax: <device_id>@<command_name>|<param1>|<param2>|...

    // retrieve the token before the @ symbol
    char *token = strtok(raw_command, "@");

    // if the token is not found, the command is invalid
    if (token == NULL)
    {
        ESP_LOGI(TAG, "Invalid command syntax: %s", raw_command);
        return ESP_FAIL;
    }

    // check if the device id matches with the config device id
    if (strcmp(token, CONFIG_IOT_AGENT_DEVICE_ID) != 0)
    {
#ifdef CONFIG_IOT_AGENT_COMMAND_STRICT
        ESP_LOGE(TAG, "Incoming command device id does not match stored id: '%s' vs '%s'", token, CONFIG_IOT_AGENT_DEVICE_ID);
        return ESP_ERR_INVALID_ARG;
#else
        ESP_LOGW(TAG, "Incoming command device id does not match stored id: '%s' vs '%s'", token, CONFIG_IOT_AGENT_DEVICE_ID);
#endif
    }

    // check the first token before the pipe symbol (command name)
    token = strtok(NULL, "|");

    if (token == NULL)
    {
        ESP_LOGI(TAG, "Invalid command syntax: %s", raw_command);
        return ESP_FAIL;
    }

    int token_len = strlen(token);

    // check if the token fits in the allocated buffer
    if (token_len >= CONFIG_IOT_AGENT_COMMAND_NAME_LEN)
    {
        ESP_LOGE(TAG, "Command name too long: %s (%d > %d)", token, token_len, CONFIG_IOT_AGENT_COMMAND_NAME_LEN);
        return ESP_ERR_INVALID_SIZE;
    }

    // copy the command name into the buffer
    strcpy(command->command_name, token);

    // load the payload
    token = strtok(NULL, "|");

    if (token == NULL)
    {
        ESP_LOGI(TAG, "Invalid command syntax: %s", raw_command);
        return ESP_FAIL;
    }

    token_len = strlen(token);

    if (token_len >= CONFIG_IOT_AGENT_COMMAND_PAYLOAD_LEN)
    {
        ESP_LOGE(TAG, "Command payload too long: %s (%d > %d)", token, token_len, CONFIG_IOT_AGENT_COMMAND_PAYLOAD_LEN);
        return ESP_ERR_INVALID_SIZE;
    }

    // copy the payload into the buffer
    strcpy(command->command_param, token);

    return ESP_OK;
}

esp_err_t fiware_iota_make_command_response(const char *command_name, const char *result, char *buffer, int buffer_len)
{
    // check if the buffer is long enough
    // the +3 is for the @ and | and the trailing zero
    if (strlen(CONFIG_IOT_AGENT_DEVICE_ID) + strlen(command_name) + strlen(result) + 3 >= buffer_len)
        return ESP_FAIL;

    // copy the device id
    strcpy(buffer, CONFIG_IOT_AGENT_DEVICE_ID);

    // copy the @ symbol
    strcat(buffer, "@");

    // copy the command name
    strcat(buffer, command_name);

    // copy the pipe symbol
    strcat(buffer, "|");

    // copy the buffer
    strcat(buffer, result);

    return ESP_OK;
}
