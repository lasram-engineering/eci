set(srcs "")

if(CONFIG_VREG_ENABLED)
    list(APPEND srcs "vreg.c")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    PRIV_REQUIRES "freertos" "driver" "task_intercom"
)
