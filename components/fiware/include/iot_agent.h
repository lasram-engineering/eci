#pragma once

#include <esp_err.h>

#include "fiware_idm.h"
#include "task_intercom.h"

#ifdef CONFIG_IOT_AGENT_REMOTE_COMMANDS
#define IOT_AGENT_REMOTE_COMMAND_ID 0xFFFFFFFF
#endif

typedef struct
{
    char *command_name;
    char *payload;
} fiware_iota_command_t;

esp_err_t fiware_iota_make_measurement(const char *payload, FiwareAccessToken_t *token, int *status_code);

esp_err_t fiware_iota_command_get_command_name(const char *raw_command, char **command_name);

esp_err_t fiware_iota_command_get_device_name(const char *raw_command, char **device_name);

esp_err_t fiware_iota_command_get_params(const char *raw_command, char ***params, uint8_t *param_num);

esp_err_t fiware_iota_command_get_param_string(const char *raw_command, char **param_string);

esp_err_t fiware_iota_command_make_response(const char *raw_command, const char *response, char **response_formatted);