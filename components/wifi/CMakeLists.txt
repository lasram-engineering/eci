set(COMPONENT_REQUIRES "esp_wifi")
set(COMPONENT_PRIV_REQUIRES "nvs_flash" "main")

if(CONFIG_WIFI_ENABLED)
    set(COMPONENT_SRCS "wifi.c")
endif()
set(COMPONENT_ADD_INCLUDEDIRS "include")

register_component()