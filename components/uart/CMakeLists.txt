set(COMPONENT_REQUIRES )

if(CONFIG_UART_TASK_ENABLE)
set(COMPONENT_SRCS "uart_task.c")
endif()
set(COMPONENT_ADD_INCLUDEDIRS "include")

register_component()