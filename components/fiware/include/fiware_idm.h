#pragma once

#include <esp_err.h>
#include <esp_http_client.h>

#include <stdbool.h>
#include <time.h>

#define FIWARE_IDM_HEADER_AUTH_TOKEN "X-Auth-Token"

#define FIWARE_IDM_ACCESS_TOKEN_LEN 40
#define FIWARE_AUTH_HEADER_BASIC_LEN 6     // length of 'Basic '
#define FIWARE_AUTH_BASE64_ENCODED_LEN 100 // 36 byte id + ':' + 36 byte secret
#define FIWARE_AUTH_BUF_LEN FIWARE_AUTH_HEADER_BASIC_LEN + FIWARE_AUTH_BASE64_ENCODED_LEN

/**
 * @brief FIWARE access token used to access and modify data in the context broker
 */
typedef struct
{
    /// @brief Char array of the access token
    char token[FIWARE_IDM_ACCESS_TOKEN_LEN + 1];
    /// @brief Char array of the refresh token
    char refresh_token[FIWARE_IDM_ACCESS_TOKEN_LEN + 1];
    /// @brief time in seconds after which the token will expire
    time_t expires_in;
} FiwareAccessToken_t;

/**
 * @brief Enum to store the grant types
 */
typedef enum
{
    PASSWORD,
    REFRESH,
} FiwareAccessTokenGrantType;

esp_err_t fiware_idm_request_access_token(FiwareAccessToken_t *token);

esp_err_t fiware_idm_renew_access_token(FiwareAccessToken_t *token);

esp_err_t fiware_idm_attach_auth_data_to_request(FiwareAccessToken_t *token, esp_http_client_handle_t client);

bool fiware_idm_check_is_token_expired(FiwareAccessToken_t *token);