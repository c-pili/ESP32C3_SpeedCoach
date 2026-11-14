#include "app_display.h"

#include "app_context.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lcd_sharp_ls027.h"

static const char *TAG = "app";

esp_err_t init_display_and_lvgl(void)
{
    ESP_LOGI(TAG, ">>> ENTERING init_display_and_lvgl <<<");
    ESP_LOGI(TAG, "Init Sharp LS027B7DH01 monochrome memory LCD...");
    ESP_LOGI(TAG, "LCD pins: SCLK=%d, MOSI=%d, SCS=%d, SPI_CLK=%d",
             CONFIG_LCD_SHARP_PIN_SCLK, CONFIG_LCD_SHARP_PIN_MOSI, CONFIG_LCD_SHARP_PIN_SCS, CONFIG_LCD_SHARP_SPI_CLOCK_HZ);
    esp_err_t err = lcd_sharp_ls027_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "LCD unavailable (%s); continuing without local display", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Init LVGL port...");
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_cfg.task_stack = 8192;
    lvgl_cfg.task_priority = 5;
    err = lvgl_port_init(&lvgl_cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "LVGL unavailable (%s); continuing without local display", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Add LVGL display with RGB565-to-monochrome flush...");
    s_disp = lcd_sharp_ls027_add_lvgl_display();
    if (!s_disp) {
        ESP_LOGW(TAG, "LVGL display allocation failed; continuing without local display");
        return ESP_ERR_NO_MEM;
    }
    lv_disp_set_default(s_disp);
    ESP_LOGI(TAG, "Display and LVGL initialized successfully");

    // IMMEDIATE TEST PATTERN - draw white background with black text
    ESP_LOGI(TAG, "Drawing test pattern (white bg, black text)...");
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "SpeedCoach\nDisplay Test\nOK!");
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_center(label);
    
    // Force render
    lv_refr_now(s_disp);
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "Test pattern drawn");

    return ESP_OK;
}
