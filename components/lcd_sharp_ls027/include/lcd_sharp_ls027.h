#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"

#define LCD_H_RES 400
#define LCD_V_RES 240

esp_err_t lcd_sharp_ls027_init(void);
lv_display_t *lcd_sharp_ls027_add_lvgl_display(void);
void lcd_sharp_ls027_set_display_enabled(bool enabled);

