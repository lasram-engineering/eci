set(COMPONENT_REQUIRES "esp_http_client" "main")

if(CONFIG_FIWARE_TASK_ENABLE)
set(COMPONENT_SRCS "fiware_task.c" "iot_agent.c")
endif()
set(COMPONENT_ADD_INCLUDEDIRS "include")

register_component()