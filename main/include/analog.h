#pragma once

#include <stdint.h>
#include <soc/adc_channel.h>

void config_analog();

uint32_t analog_measure_voltage();