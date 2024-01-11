/// @file

#include "fiware_idm.h"

#include <esp_log.h>

#include <cJSON.h>

#include "wifi.h"

#define FIWARE_IDM_URI "http://" CONFIG_FIWARE_HOST

#define FIWARE_IDM_ACCESS_TOKEN_ROUTE "/oauth2/token"

#define FIWARE_IDM_AUTH_TOKEN_PATH "/v1/auth/tokens"

#define FIWARE_IDM_GRANT_TYPE_PASSWORD "password"
#define FIWARE_IDM_GRANT_TYPE_REFRESH "refresh_token"

static const char *TAG = "FIWARE IdM";

/**
 * @brief Callback function to process the http client event
 *
 * @param event the event to be processed, its user data is the FiwareAccessToken_t that needs to be parsed
 * @return esp_err_t ESP_OK
 */
esp_err_t idm_access_token_event_handler(esp_http_client_event_handle_t event)
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
        {
            ESP_LOGW(TAG, "Error while requesting access token: %s", (char *)event->data);
            break;
        }

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
        strncpy(token->token, access_token->valuestring, FIWARE_IDM_ACCESS_TOKEN_LEN);
        strncpy(token->refresh_token, refresh_token->valuestring, FIWARE_IDM_ACCESS_TOKEN_LEN);
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

static const esp_http_client_config_t request_access_token_config = {
    .host = CONFIG_FIWARE_HOST,
    .port = CONFIG_FIWARE_IDM_PORT,
    .path = FIWARE_IDM_ACCESS_TOKEN_ROUTE,
    .method = HTTP_METHOD_POST,
    .event_handler = idm_access_token_event_handler,
    .username = CONFIG_FIWARE_IDM_APP_ID,
    .password = CONFIG_FIWARE_IDM_APP_SECRET,
    .auth_type = HTTP_AUTH_TYPE_BASIC,
    .cert_pem = NULL,
};

/**
 * @brief Sends a request to the FIWARE KeyRock to generate an access token
 *
 * @param token pointer to a FiwareAccessToken_t struct to load the token into
 * @param grant_type used to specify the grant type: either PASSWORD or REFRESH.
 *                   Use PASSWORD to request the initial token and REFRESH to refresh an expired token
 * @return esp_err_t    ESP_ERR_INVALID_ARG if the grant type was invalid,
 *                      ESP_OK if the request was successful,
 *                      ESP_FAIL if there was an error requesting the token
 */
esp_err_t fiware_idm_request_access_token_grant_type(FiwareAccessToken_t *token, FiwareAccessTokenGrantType grant_type)
{
    esp_http_client_handle_t client = esp_http_client_init(&request_access_token_config);

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

    ESP_LOGD(TAG, "Requesting access token with payload: %s", payload);

    // set the data field to the payload
    esp_http_client_set_post_field(client, payload, strlen(payload));

    // execute the request
    int ret = esp_http_client_perform(client);

    // free the payload variable
    free(payload);

    if (ret != ESP_OK)
        return ret;

    int status_code = esp_http_client_get_status_code(client);

    if (status_code < 400)
    {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Error while requesting token: %d", status_code);

    return ESP_FAIL;
}

/**
 * @brief Requests a new FIWARE access token
 *
 * @param token pointer to the FiwareAccessToken_t struct to load the token into
 * @return esp_err_t see fiware_idm_request_access_token_grant_type()
 */
esp_err_t fiware_idm_request_access_token(FiwareAccessToken_t *token)
{
    // check if wifi is not connected
    WIFI_RETURN_IF_NOT_CONNECTED(ESP_ERR_INVALID_STATE);

    return fiware_idm_request_access_token_grant_type(token, PASSWORD);
}

/**
 * @brief Request a FIWARE access token renewal
 *
 * @param token pointer to the FiwareAccessToken_t struct
 * @return esp_err_t see fiware_idm_request_access_token_grant_type()
 */
esp_err_t fiware_idm_renew_access_token(FiwareAccessToken_t *token)
{
    // check if wifi is not connected
    WIFI_RETURN_IF_NOT_CONNECTED(ESP_ERR_INVALID_STATE);

    return fiware_idm_request_access_token_grant_type(token, REFRESH);
}

/**
 * @brief Attach the necessary authentication data to an http client request
 *
 * @param token FiwareAccessToken_t token to attack
 * @param client esp http client handle
 * @return esp_err_t ESP_OK
 */
esp_err_t fiware_idm_attach_auth_data_to_request(FiwareAccessToken_t *token, esp_http_client_handle_t client)
{
    // set the X-Auth-Token header value to the tokens value
    esp_http_client_set_header(client, FIWARE_IDM_HEADER_AUTH_TOKEN, token->token);

    return ESP_OK;
}

/**
 * @brief Checks if the token is expired or not
 *
 * @param token FiwareAccessToken_t the token to check
 * @return true if the token is expired (not valid)
 * @return false if the token is valid
 */
bool fiware_idm_check_is_token_expired(FiwareAccessToken_t *token)
{
    time_t now;
    time(&now);

    return token->expires_in <= now;
}
