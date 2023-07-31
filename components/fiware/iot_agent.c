#include "iot_agent.h"

#include <string.h>

#include <esp_http_client.h>
#include <esp_log.h>

#include "app_state.h"

/** URL for FIWARE IoT Agent measurements */
#define FIWARE_IOTA_MEAS_URL "http://" CONFIG_IOT_AGENT_HOST ":" CONFIG_IOT_AGENT_SOUTH_PORT CONFIG_IOT_AGENT_RESOURCE

#define FIWARE_IOTA_MEAS_QUERY "?i=" CONFIG_IOT_AGENT_DEVICE_ID "&k=" CONFIG_IOT_AGENT_APIKEY

#define RESPONSE_BUF_LEN 255

static const char *TAG = "IoT Agent";

static const esp_http_client_config_t measurement_config = {
    .url = FIWARE_IOTA_MEAS_URL FIWARE_IOTA_MEAS_QUERY,
    .method = HTTP_METHOD_POST,
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
fiware_iota_make_measurement(const char *payload)
{
    // check if wifi is not connected
    if (!app_state_get(STATE_TYPE_INTERNAL) & APP_STATE_INTERNAL_WIFI_CONNECTED)
    {
        return ESP_ERR_INVALID_STATE;
    }

    // create a new client with the request
    esp_http_client_handle_t client = esp_http_client_init(&measurement_config);

    // put the payload in the post field
    esp_http_client_set_post_field(client, payload, strlen(payload));

    // set the content type header
    esp_http_client_set_header(client, "Content-Type", "text/plain");

    // process the request itself
    esp_http_client_perform(client);

    // get the status code of the event
    int status_code = esp_http_client_get_status_code(client);

    if (status_code < 400)
    {
        return ESP_OK;
    }

    if (status_code == 404)
    {
        ESP_LOGW(TAG, "Could not find device with apikey '" CONFIG_IOT_AGENT_APIKEY "' maybe service group is not registered?");
    }

    else if (status_code == 422)
    {
        ESP_LOGW(TAG, "Could not find device with id '" CONFIG_IOT_AGENT_DEVICE_ID "' maybe device is not registered or invalid poperty?");
    }

    else
    {
        ESP_LOGW(TAG, "Unexpected status from server: %d", status_code);
    }

    return status_code;
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
