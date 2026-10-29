#include "ui_core_internal.h"

lv_disp_t *ui_s_disp = NULL;
lv_obj_t *ui_s_scr = NULL;
lv_obj_t *ui_s_page_data = NULL;
lv_obj_t *ui_s_page_settings = NULL;
lv_obj_t *ui_s_page_menu = NULL;
lv_obj_t *ui_s_page_activity_summary = NULL;
lv_obj_t *ui_s_page_activity_detail = NULL;
lv_obj_t *ui_s_page_sensors = NULL;


ui_page_t ui_s_current_page = UI_PAGE_DATA;
bool ui_s_transitioning = false;
ui_dark_mode_cb_t ui_s_dark_mode_cb = NULL;
ui_auto_rotate_cb_t ui_s_auto_rotate_cb = NULL;
bool ui_s_dark_mode = true;
ui_status_bar_t ui_s_live_status;
