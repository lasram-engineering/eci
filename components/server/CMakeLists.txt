set(COMPONENT_REQUIRES "esp_http_server")
set(COMPONENT_PRIV_REQUIRES "state")

set(COMPONENT_SRCS "server.c")
set(COMPONENT_ADD_INCLUDEDIRS "include")

register_component()