# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "analog.c" "fiware.c" "server.c" "main.c" "app_state.c" "wifi.c" "app_tasks.c" "io.c")
set(COMPONENT_ADD_INCLUDEDIRS "include")

register_component()
