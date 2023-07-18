set(COMPONENT_PRIV_REQUIRES "driver" "task_intercom")

set(COMPONENT_SRCS "mau_task.c" "uart.c")
set(COMPONENT_ADD_INCLUDEDIRS "include")

register_component()