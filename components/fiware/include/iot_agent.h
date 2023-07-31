#pragma once

#include <esp_err.h>

esp_err_t fiware_iota_make_measurement(const char *payload);

typedef struct
{
    char command_name[CONFIG_IOT_AGENT_COMMAND_NAME_LEN];
    char command_param[CONFIG_IOT_AGENT_COMMAND_PAYLOAD_LEN];
} fiware_iota_command_t;

esp_err_t fiware_iota_parse_command(char *raw_command, fiware_iota_command_t *command);