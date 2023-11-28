set(COMPONENT_REQUIRES "esp_http_server")
set(COMPONENT_PRIV_REQUIRES "task_intercom" "fiware")

set(COMPONENT_SRCS "server.c")
set(COMPONENT_ADD_INCLUDEDIRS "include")

register_component()