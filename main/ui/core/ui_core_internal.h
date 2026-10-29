#pragma once

#include "ui.h"
#include "lvgl.h"
#include "ui_status_bar.h"
#include <stdbool.h>

extern lv_disp_t *ui_s_disp;
extern lv_obj_t *ui_s_scr;
extern lv_obj_t *ui_s_page_data;
extern lv_obj_t *ui_s_page_settings;
extern lv_obj_t *ui_s_page_menu;
extern lv_obj_t *ui_s_page_activity_summary;
extern lv_obj_t *ui_s_page_activity_detail;
extern lv_obj_t *ui_s_page_sensors;


extern ui_page_t ui_s_current_page;
extern bool ui_s_transitioning;
extern ui_dark_mode_cb_t ui_s_dark_mode_cb;
extern ui_auto_rotate_cb_t ui_s_auto_rotate_cb;
extern bool ui_s_dark_mode;
extern ui_status_bar_t ui_s_live_status;

void ui_pages_relayout(void);
void create_pages_ui(void);
void ensure_sensors_page(void);
