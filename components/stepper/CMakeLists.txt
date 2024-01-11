idf_component_register(
    SRCS "stepper.c"
    INCLUDE_DIRS "include"
    REQUIRES "esp_timer"
    PRIV_REQUIRES "driver" "task_intercom"
)