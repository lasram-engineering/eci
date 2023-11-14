set(COMPONENT_REQUIRES "esp_http_client")
set(COMPONENT_PRIV_REQUIRES "esp_netif" "task_intercom" "state" "json")

if(CONFIG_FIWARE_TASK_ENABLE)
set(COMPONENT_SRCS "fiware_task.c" "iot_agent.c" "fiware_idm.c")
endif()
set(COMPONENT_ADD_INCLUDEDIRS "include")

register_component()