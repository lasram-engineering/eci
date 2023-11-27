idf_component_register(
    SRCS "wifi.c"
    INCLUDE_DIRS "include"
    REQUIRES "freertos"
    PRIV_REQUIRES "esp_wifi"
)