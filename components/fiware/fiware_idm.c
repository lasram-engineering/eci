#include "fiware_idm.h"

#include <esp_http_client.h>
#include <esp_log.h>

#include <cJSON.h>

#include "app_state.h"

#define FIWARE_IDM_URI "http://" CONFIG_FIWARE_HOST ":" CONFIG_FIWARE_IDM_PORT

#define FIWARE_IDM_TOKEN_ROUTE "/oauth2/token"

#define FIWARE_IDM_GRANT_TYPE_PASSWORD "password"
#define FIWARE_IDM_GRANT_TYPE_REFRESH "refresh_token"

static const char *TAG = "FIWARE IdM";

esp_err_t http_event_get_handler(esp_http_client_event_handle_t event)
{
    switch (event->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        // parse the incoming token data
        /**
         * payload syntax:
         * {
         *  access_token:...,
         *  token_type: ...,
         *  refresh_token: ...,
         *  expires_in: ...,
         * }
         *
         */

        // if the status is not OK then break from the switch
        if (esp_http_client_get_status_code(event->client) >= 300)
            break;

        // cast the access token from the request user data
        FiwareAccessToken_t *token = (FiwareAccessToken_t *)event->user_data;

        // parse the json string
        cJSON *payload = cJSON_Parse(event->data);

        // get the attributes from the JSON object
        cJSON *access_token = cJSON_GetObjectItem(payload, "access_token");
        cJSON *refresh_token = cJSON_GetObjectItem(payload, "refresh_token");
        cJSON *expires_in = cJSON_GetObjectItem(payload, "expires_in");

        // initialize time
        time_t now;
        time(&now); // get the seconds since 1970. 01. 01.

        // load the attributes into the token
        snprintf(token->token, FIWARE_IDM_TOKEN_LEN, "%s", access_token->valuestring);
        snprintf(token->refresh_token, FIWARE_IDM_TOKEN_LEN, "%s", refresh_token->valuestring);
        token->expires_in = now + expires_in->valueint;

        ESP_LOGI(
            TAG,
            "Token: %s\n\tRefresh: %s\n\tExpires in: %lld",
            token->token,
            token->refresh_token,
            token->expires_in);

        // free the cJSON object
        cJSON_Delete(payload);
        break;

    default:
        break;
    }

    return ESP_OK;
}

static const esp_http_client_config_t request_token_config = {
    .url = FIWARE_IDM_URI FIWARE_IDM_TOKEN_ROUTE,
    .method = HTTP_METHOD_POST,
    .event_handler = http_event_get_handler,
    .username = CONFIG_FIWARE_IDM_APP_ID,
    .password = CONFIG_FIWARE_IDM_APP_SECRET,
    .auth_type = HTTP_AUTH_TYPE_BASIC,
    .cert_pem = NULL,
};

esp_err_t fiware_idm_request_access_token_grant_type(FiwareAccessToken_t *token, FiwareAccessTokenGrantType grant_type)
{
    esp_http_client_handle_t client = esp_http_client_init(&request_token_config);

    // pass the token to be available during the callbacks
    esp_http_client_set_user_data(client, token);

    // set the content type header
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");

    // declare the payload variable
    char *payload;

    // allocate the buffer and load the formatted data into it
    switch (grant_type)
    {
    case PASSWORD:
        asprintf(
            &payload,
            "username=%s&password=%s&grant_type=%s",
            CONFIG_FIWARE_IDM_USERNAME,
            CONFIG_FIWARE_IDM_PASSWORD,
            FIWARE_IDM_GRANT_TYPE_PASSWORD);
        break;
    case REFRESH:
        asprintf(
            &payload,
            "refresh_token=%s&grant_type=%s",
            token->refresh_token,
            FIWARE_IDM_GRANT_TYPE_REFRESH);
        break;

    default:
        return ESP_ERR_INVALID_ARG;
    }

    // set the data field to the payload
    esp_http_client_set_post_field(client, payload, strlen(payload));

    // execute the request
    esp_http_client_perform(client);

    int status_code = esp_http_client_get_status_code(client);

    // free the payload variable
    free(payload);

    if (status_code < 400)
    {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Error while requesting token: %d", status_code);

    return ESP_FAIL;
}

esp_err_t fiware_idm_request_access_token(FiwareAccessToken_t *token)
{
    // check if wifi is not connected
    if (!app_state_get(STATE_TYPE_INTERNAL) & APP_STATE_INTERNAL_WIFI_CONNECTED)
    {
        return ESP_ERR_INVALID_STATE;
    }

    return fiware_idm_request_access_token_grant_type(token, PASSWORD);
}

esp_err_t fiware_idm_renew_access_token(FiwareAccessToken_t *token)
{
    // check if wifi is not connected
    if (!app_state_get(STATE_TYPE_INTERNAL) & APP_STATE_INTERNAL_WIFI_CONNECTED)
    {
        return ESP_ERR_INVALID_STATE;
    }

    return fiware_idm_request_access_token_grant_type(token, REFRESH);
}

esp_err_t fiware_idm_attach_auth_data_to_request(FiwareAccessToken_t *token, esp_http_client_t client)
{
    // set the auth type to basic
    esp_http_client_set_authtype(client, HTTP_AUTH_TYPE_BASIC);
    // add the username
    esp_http_client_set_username(client, CONFIG_FIWARE_IDM_APP_ID);
    // add the password
    esp_http_client_set_password(client, CONFIG_FIWARE_IDM_APP_SECRET);

    // set the X-Auth-Token header value to the tokens value
    esp_http_client_set_header(client, FIWARE_IDM_HEADER_AUTH_TOKEN, token->token);

    return ESP_OK;
}

bool fiware_idm_check_is_token_expired(FiwareAccessToken_t *token)
{
    time_t now;
    time(&now);

    return token->expires_in <= now;
}
