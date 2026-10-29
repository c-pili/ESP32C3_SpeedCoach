idf_component_register(
    SRCS "gps_neo_m8n.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_uart esp_timer
)
