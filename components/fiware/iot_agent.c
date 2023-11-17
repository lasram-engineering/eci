#include "iot_agent.h"

#include <string.h>

#include <esp_http_client.h>
#include <esp_log.h>

#include "app_state.h"

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
esp_err_t fiware_iota_make_measurement(const char *payload, FiwareAccessToken_t *token, int *status_code)
{
    // check if wifi is not connected
    if (!app_state_get(STATE_TYPE_INTERNAL) & APP_STATE_INTERNAL_WIFI_CONNECTED)
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

esp_err_t fiware_iota_command_get_command_name(const char *raw_command, char **command_name)
{
    char *command = strdup(raw_command);

    if (command == NULL)
        return ESP_ERR_NO_MEM;

    // gets the device name and command name
    char *header = strtok(command, "|");

    if (header == NULL)
    {
        free(command);
        return ESP_FAIL;
    }

    // gets the device name
    strtok(header, "@");

    // gets the command name (remaining)
    char *token = strtok(NULL, "");

    if (token == NULL)
    {
        free(command);
        return ESP_FAIL;
    }

    *command_name = strdup(token);
    free(command);

    if (*command_name == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t fiware_iota_command_get_device_name(const char *raw_command, char **device_name)
{
    // allocate a copy of the
    char *command = strdup(raw_command);

    if (command == NULL)
        return ESP_ERR_NO_MEM;

    // gets the device name and command name
    char *header = strtok(command, "|");

    if (header == NULL)
    {
        free(command);
        return ESP_FAIL;
    }

    // gets the device name
    char *token = strtok(header, "@");

    if (token == NULL)
    {
        free(command);
        return ESP_FAIL;
    }

    *device_name = strdup(token);
    free(command);

    if (device_name == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t fiware_iota_command_get_params(const char *raw_command, char ***params, uint8_t *param_num)
{
    char *command = strdup(raw_command);

    if (command == NULL)
        return ESP_ERR_NO_MEM;

    // first token is the header
    char *token = strtok(command, "|");

    if (token == NULL)
    {
        free(command);
        return ESP_FAIL;
    }

    // get the params
    token = strtok(NULL, "");

    *params = (char **)malloc(sizeof(char *));

    *param_num = 0;

    while (token != NULL)
    {
        // append the token into the params list
        // allocate the memory and copy the string
        (*params)[*param_num] = strdup(token);

        // check if the memory allocation was successful
        if (*(params)[*param_num] == NULL)
        {
            abort(); // TODO add proper freeing
        }

        // increment the parameter count
        (*param_num)++;

        token = strtok(NULL, "|");
    }

    return ESP_OK;
}

esp_err_t fiware_iota_command_get_param_string(const char *raw_command, char **param_string)
{
    char *command = strdup(raw_command);

    if (command == NULL)
        return ESP_ERR_NO_MEM;

    // first token is the header
    char *token = strtok(command, "|");

    if (token == NULL)
    {
        free(command);
        return ESP_FAIL;
    }

    // get the params
    token = strtok(NULL, "");

    *param_string = strdup(token);

    free(command);

    if (param_string == NULL)
        return ESP_ERR_NO_MEM;

    return ESP_OK;
}

/**
 * @brief Makes a response for the FIWARE UltraLight 2.0 command
 *
 * @param raw_command the raw command payload -> <device_id>@<command>|<param1>|<param2>
 * @param response the response to the command
 * @param response_formatted pointer to a char array of the response. User is expected to free it after usage
 * @return esp_err_t
 */
esp_err_t fiware_iota_command_make_response(const char *raw_command, const char *response, char **response_formatted)
{
    char *command = (char *)malloc(sizeof(char) * (strlen(raw_command) + 1));

    if (command == NULL)
        return ESP_ERR_NO_MEM;

    strcpy(command, raw_command);

    char *header = strtok(command, "|");

    int ret = asprintf(response_formatted, "%s|%s", header, response);

    // header is copied into response_formatted, free command
    free(command);

    if (ret == -1)
    {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
