#include "app_display.h"

#include "app_context.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lcd_sharp_ls027.h"

static const char *TAG = "app";

void init_display_and_lvgl(void)
{
    ESP_LOGI(TAG, "Init Sharp LS027B7DH01 monochrome memory LCD...");
    ESP_ERROR_CHECK(lcd_sharp_ls027_init());

    ESP_LOGI(TAG, "Init LVGL port...");
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_cfg.task_stack = 12288; /* Sharp 400x240 UI plus status and history pages. */
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    ESP_LOGI(TAG, "Add LVGL display with RGB565-to-monochrome flush...");
    s_disp = lcd_sharp_ls027_add_lvgl_display();
    ESP_ERROR_CHECK(s_disp ? ESP_OK : ESP_ERR_NO_MEM);
    lv_disp_set_default(s_disp);
}

