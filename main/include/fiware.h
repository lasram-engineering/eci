#pragma once

#include "esp_err.h"

esp_err_t fiware_update_attribute_raw(const char *raw_attr_payload);
esp_err_t fiware_update_attribute(const char name, const char *value);

esp_err_t fiware_check_health();