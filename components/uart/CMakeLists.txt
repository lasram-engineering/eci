set(COMPONENT_PRIV_REQUIRES "driver" "task_intercom" "fiware")

if(CONFIG_UART_TASK_ENABLE)
set(COMPONENT_SRCS "uart_task.c" "kawasaki.c")
endif()
set(COMPONENT_ADD_INCLUDEDIRS "include")

register_component()