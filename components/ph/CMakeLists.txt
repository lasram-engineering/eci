set(srcs "")

if(CONFIG_PH_ENABLED)
    list(APPEND srcs "ph.c" "sensor.c")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    PRIV_REQUIRES "freertos" "esp_adc" "nvs_flash" "task_intercom"
)
