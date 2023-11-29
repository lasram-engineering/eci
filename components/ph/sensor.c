#include "sensor.h"

#include <esp_log.h>
#include <esp_check.h>
#include <esp_adc/adc_oneshot.h>
#include <nvs.h>
#include <nvs_flash.h>

#define PH_SENSOR_NVS_STORAGE_NAME "storage"
#define PH_SENSOR_NVS_V_LOW_NAME "voltage_low"
#define PH_SENSOR_NVS_V_HIGH_NAME "voltage_high"
#define PH_SENSOR_NVS_PH_LOW_NAME "ph_low"
#define PH_SENSOR_NVS_PH_HIGH_NAME "ph_high"

#define ADC_BIT_WIDTH ADC_BITWIDTH_12
#define ADC_RESOLUTION ((1 << ADC_BIT_WIDTH) - 1)
#define ADC_VOLTAGE_MV 3300

static const char *TAG = "pH Sensor";

esp_err_t ph_sensor_load_from_nvs(ph_sensor_t *sensor)
{
    nvs_handle_t nvs_handle;
    int ret;

    ret = nvs_open(PH_SENSOR_NVS_STORAGE_NAME, NVS_READONLY, &nvs_handle);

    if (ret != ESP_OK)
        return ret;

    // read the values from the NVS
    ret = ESP_OK;

    ESP_GOTO_ON_ERROR(
        nvs_get_u32(nvs_handle, PH_SENSOR_NVS_V_LOW_NAME, &sensor->voltage_low),
        cleanup,
        TAG,
        "Unable to get value: " PH_SENSOR_NVS_V_LOW_NAME);
    ESP_GOTO_ON_ERROR(
        nvs_get_u32(nvs_handle, PH_SENSOR_NVS_V_HIGH_NAME, &sensor->voltage_high),
        cleanup,
        TAG,
        "Unable to get value: " PH_SENSOR_NVS_V_HIGH_NAME);
    ESP_GOTO_ON_ERROR(
        nvs_get_u32(nvs_handle, PH_SENSOR_NVS_PH_LOW_NAME, &sensor->ph_low),
        cleanup,
        TAG,
        "Unable to get value: " PH_SENSOR_NVS_PH_LOW_NAME);
    ESP_GOTO_ON_ERROR(
        nvs_get_u32(nvs_handle, PH_SENSOR_NVS_PH_HIGH_NAME, &sensor->ph_high),
        cleanup,
        TAG,
        "Unable to get value: " PH_SENSOR_NVS_PH_HIGH_NAME);

cleanup:
    nvs_close(nvs_handle);

    return ret;
}

esp_err_t ph_sensor_store_in_nvs(ph_sensor_t *sensor)
{
    nvs_handle_t nvs_handle;
    int ret;

    ret = nvs_open(PH_SENSOR_NVS_STORAGE_NAME, NVS_READWRITE, &nvs_handle);

    if (ret != ESP_OK)
        return ret;

    ret = ESP_OK;

    ESP_GOTO_ON_ERROR(
        nvs_set_u32(nvs_handle, PH_SENSOR_NVS_V_LOW_NAME, sensor->voltage_low),
        cleanup,
        TAG,
        "Unable to get value: " PH_SENSOR_NVS_V_LOW_NAME);
    ESP_GOTO_ON_ERROR(
        nvs_set_u32(nvs_handle, PH_SENSOR_NVS_V_HIGH_NAME, sensor->voltage_high),
        cleanup,
        TAG,
        "Unable to get value: " PH_SENSOR_NVS_V_HIGH_NAME);
    ESP_GOTO_ON_ERROR(
        nvs_set_u32(nvs_handle, PH_SENSOR_NVS_PH_LOW_NAME, sensor->ph_low),
        cleanup,
        TAG,
        "Unable to get value: " PH_SENSOR_NVS_PH_LOW_NAME);
    ESP_GOTO_ON_ERROR(
        nvs_set_u32(nvs_handle, PH_SENSOR_NVS_PH_HIGH_NAME, sensor->ph_high),
        cleanup,
        TAG,
        "Unable to get value: " PH_SENSOR_NVS_PH_HIGH_NAME);

cleanup:
    if (ret == ESP_OK)
        nvs_commit(nvs_handle);

    nvs_close(nvs_handle);

    return ret;
}

esp_err_t ph_sensor_init(ph_sensor_t *sensor)
{
    int ret;

    sensor->ph_measurement = 0;

    // load the data (if any) from the NVS
    ret = ph_sensor_load_from_nvs(sensor);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Calibration data loaded from NVS");
        ESP_LOGI(TAG, "Low voltage: %lu, high voltage: %lu", sensor->voltage_low, sensor->voltage_high);
        ESP_LOGI(TAG, "Low pH: %lu, high pH: %lu", sensor->ph_low, sensor->ph_high);
        return ESP_OK;
    }

    if (ret != ESP_ERR_NVS_NOT_FOUND)
        ESP_LOGW(TAG, "Unknown NVS error: (%d) %s", ret, esp_err_to_name(ret));

    sensor->ph_high = 7;
    sensor->voltage_high = 5000;

    sensor->ph_low = 1;
    sensor->voltage_low = 1000;

    return ESP_OK;
}

esp_err_t ph_sensor_measure_voltage(ph_sensor_t *sensor, sensor_unit_t *voltage)
{
    // configure adc
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t adc_init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    ESP_RETURN_ON_ERROR(
        adc_oneshot_new_unit(&adc_init_config, &adc_handle),
        TAG,
        "Error while initializing ADC");

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BIT_WIDTH,
        .atten = ADC_ATTEN_DB_0,
    };

    ESP_RETURN_ON_ERROR(
        adc_oneshot_config_channel(adc_handle, CONFIG_PH_ADC_CHANNEL, &config),
        TAG,
        "Error while configuring ADC channel");

    // read the value
    int value;
    ESP_RETURN_ON_ERROR(
        adc_oneshot_read(adc_handle, CONFIG_PH_ADC_CHANNEL, &value),
        TAG,
        "Error while reading analog value");

    // dispose of the adc
    adc_oneshot_del_unit(adc_handle);

    *voltage = (float)(value) / ADC_RESOLUTION * ADC_VOLTAGE_MV;

    return ESP_OK;
}

/**
 * @brief Measures the pH value provided by the sensor
 *
 * @param sensor pointer to the sensor struct
 * @param measurement pointer to the unit to store measurement, set to NULL if not needed
 * @return esp_err_t
 */
esp_err_t ph_sensor_make_measurement(ph_sensor_t *sensor, sensor_unit_t *measurement)
{
    int ret;
    sensor_unit_t voltage;

    ret = ph_sensor_measure_voltage(sensor, &voltage);

    ESP_LOGI(TAG, "Measured voltage: %lu", voltage);

    if (ret != ESP_OK)
        return ret;

    // convert the measurement to pH value
    float slope = (float)(sensor->ph_high - sensor->ph_low) / (int32_t)(sensor->voltage_high - sensor->voltage_low);

    sensor->ph_measurement = slope * (int32_t)(voltage - sensor->voltage_low) + sensor->ph_low;

    if (measurement != NULL)
        *measurement = sensor->ph_measurement;

    return ESP_OK;
}

esp_err_t ph_sensor_calibrate(ph_sensor_t *sensor, bool high_point, sensor_unit_t control_ph)
{
    if (control_ph < 1000 || control_ph > 7000)
        return ESP_ERR_INVALID_ARG;

    sensor_unit_t voltage;

    int ret = ph_sensor_measure_voltage(sensor, &voltage);

    if (ret != ESP_OK)
        return ret;

    if (high_point)
    {
        sensor->ph_high = control_ph;
        sensor->voltage_high = voltage;
    }
    else
    {
        sensor->ph_low = control_ph;
        sensor->voltage_low = voltage;
    }

    ESP_LOGI(TAG, "%s point voltage set to: %lu, control pH is %lu", high_point ? "High" : "Low", voltage, control_ph);

    ret = ph_sensor_store_in_nvs(sensor);

    if (ret != ESP_OK)
        ESP_LOGI(TAG, "Unable to store settings in NVS: %s", esp_err_to_name(ret));

    return ret;
}