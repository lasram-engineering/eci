set(COMPONENT_REQUIRES "mau")

if(CONFIG_UART_TASK_ENABLE)
set(COMPONENT_SRCS "uart_task.c" "kawasaki.c")
endif()
set(COMPONENT_ADD_INCLUDEDIRS "include")

register_component()