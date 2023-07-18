set(COMPONENT_REQUIRES)

if(CONFIG_FIWARE_TASK_ENABLE)
set(COMPONENT_SRCS "task_intercom.c")
endif()
set(COMPONENT_ADD_INCLUDEDIRS "include")

register_component()