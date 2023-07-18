# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS "analog.c" "server.c" "main.c" "app_state.c" "tasks/task_intercom.c" "tasks/common.c")
set(COMPONENT_ADD_INCLUDEDIRS "include")

register_component()
