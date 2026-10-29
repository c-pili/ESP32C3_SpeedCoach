#include "ui_core_internal.h"

#include "ui_activity_detail_page.h"
#include "ui_activity_summary_page.h"
#include "ui_data_page.h"
#include "ui_menu_page.h"
#include "ui_settings_page.h"
#include "ui_sensors_page.h"
#include "ui_theme.h"

void create_pages_ui(void)
{
    ui_s_scr = lv_disp_get_scr_act(ui_s_disp);
    lv_obj_clean(ui_s_scr);
    ui_theme_apply_screen(ui_s_scr);

    // 1. Data Page
    ui_s_page_data = lv_obj_create(ui_s_scr);
    lv_obj_set_size(ui_s_page_data, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(ui_s_page_data, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ui_s_page_data, 0, 0);
    lv_obj_set_style_bg_opa(ui_s_page_data, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_s_page_data, 0, 0);
    data_page_create(ui_s_page_data);

    // 2. Menu Page (NEW)
    ui_s_page_menu = lv_obj_create(ui_s_scr);
    lv_obj_set_size(ui_s_page_menu, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(ui_s_page_menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ui_s_page_menu, 0, 0);
    lv_obj_set_style_border_width(ui_s_page_menu, 0, 0);
    ui_theme_apply_screen(ui_s_page_menu);
    menu_page_create(ui_s_page_menu);

    // 3. Settings Page
    ui_s_page_settings = lv_obj_create(ui_s_scr);
    lv_obj_set_size(ui_s_page_settings, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(ui_s_page_settings, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ui_s_page_settings, 0, 0);
    lv_obj_set_style_border_width(ui_s_page_settings, 0, 0);
    ui_theme_apply_screen(ui_s_page_settings);
    settings_page_create(ui_s_page_settings);
    // Activity Summary page
    ui_s_page_activity_summary = lv_obj_create(ui_s_scr);
    lv_obj_set_size(ui_s_page_activity_summary, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(ui_s_page_activity_summary, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ui_s_page_activity_summary, 0, 0);
    lv_obj_set_style_border_width(ui_s_page_activity_summary, 0, 0);
    ui_theme_apply_screen(ui_s_page_activity_summary);
    activity_summary_page_create(ui_s_page_activity_summary);

    // Activity Detail page
    ui_s_page_activity_detail = lv_obj_create(ui_s_scr);
    lv_obj_set_size(ui_s_page_activity_detail, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(ui_s_page_activity_detail, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ui_s_page_activity_detail, 0, 0);
    lv_obj_set_style_border_width(ui_s_page_activity_detail, 0, 0);
    ui_theme_apply_screen(ui_s_page_activity_detail);
    activity_detail_page_create(ui_s_page_activity_detail);

    ui_s_current_page = UI_PAGE_DATA;
    ui_pages_relayout();
}

void ensure_sensors_page(void)
{
    if (ui_s_page_sensors)
        return;
    if (!ui_s_scr)
        return;

    ui_s_page_sensors = lv_obj_create(ui_s_scr);
    lv_obj_set_size(ui_s_page_sensors, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(ui_s_page_sensors, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(ui_s_page_sensors, 0, 0);
    lv_obj_set_style_border_width(ui_s_page_sensors, 0, 0);
    ui_theme_apply_screen(ui_s_page_sensors);
    sensors_page_create(ui_s_page_sensors);

    ui_pages_relayout();
}
