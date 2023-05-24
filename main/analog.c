#include "analog.h"

#include <driver/adc.h>
#include <esp_adc_cal.h>

static esp_adc_cal_characteristics_t adc1_chars;

void config_analog()
{
    // configure the attenuation on the channel
    adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_11);

    // calibrate the analog input
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);

    // configure the resolution
    adc1_config_width(ADC_WIDTH_BIT_DEFAULT);
}

uint32_t analog_measure_voltage()
{
    // get the raw value
    int adc_value = adc1_get_raw(ADC1_CHANNEL_3);

    return esp_adc_cal_raw_to_voltage(adc_value, &adc1_chars);
}
