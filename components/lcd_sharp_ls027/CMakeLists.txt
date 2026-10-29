idf_component_register(
    SRCS "lcd_sharp_ls027.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_spi esp_driver_gpio lvgl__lvgl
)
