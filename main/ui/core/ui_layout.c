#include "ui_core_internal.h"

#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui_activity_detail_page.h"
#include "ui_activity_summary_page.h"
#include "ui_data_page.h"
#include "ui_menu_page.h"
#include "ui_settings_page.h"
#include "ui_sensors_page.h"

static void anim_set_y(void *var, int32_t v) { lv_obj_set_y((lv_obj_t *)var, (lv_coord_t)v); }

static ui_page_t s_pending_page = UI_PAGE_COUNT;
static bool s_pending_animated = true;

static void pending_go_async(void *p)
{
    (void)p;
    if (s_pending_page >= UI_PAGE_COUNT)
        return;
    ui_page_t page = s_pending_page;
    bool animated = s_pending_animated;
    s_pending_page = UI_PAGE_COUNT;
    ui_go_to_page(page, animated);
}

static void anim_done_cb(lv_anim_t *a)
{
    (void)a;
    ui_s_transitioning = false;
    ui_pages_relayout();
    if (s_pending_page < UI_PAGE_COUNT)
        lv_async_call(pending_go_async, NULL);
}

void ui_pages_relayout(void)
{
    if (!ui_s_scr)
        return;
    lv_coord_t h = lv_obj_get_height(ui_s_scr);

    if (ui_s_page_data)
    {
        lv_obj_set_size(ui_s_page_data, lv_pct(100), lv_pct(100));
        lv_obj_set_pos(ui_s_page_data, 0, 0);
        if (ui_s_current_page == UI_PAGE_DATA)
        {
            lv_obj_clear_flag(ui_s_page_data, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(ui_s_page_data, LV_OBJ_FLAG_HIDDEN);
        }
    }


    if (ui_s_page_menu)
    {
        lv_obj_set_size(ui_s_page_menu, lv_pct(100), lv_pct(100));

        if (ui_s_current_page == UI_PAGE_MENU)
        {
            lv_obj_set_pos(ui_s_page_menu, 0, 0);
            lv_obj_clear_flag(ui_s_page_menu, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_set_pos(ui_s_page_menu, 0, -h);
            lv_obj_add_flag(ui_s_page_menu, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ui_s_page_settings)
    {
        lv_obj_set_size(ui_s_page_settings, lv_pct(100), lv_pct(100));
        if (ui_s_current_page == UI_SETTINGS_PAGE)
        {
            lv_obj_set_pos(ui_s_page_settings, 0, 0);
            lv_obj_clear_flag(ui_s_page_settings, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_set_pos(ui_s_page_settings, 0, -h);
            lv_obj_add_flag(ui_s_page_settings, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ui_s_page_sensors)
    {
        lv_obj_set_size(ui_s_page_sensors, lv_pct(100), lv_pct(100));
        if (ui_s_current_page == UI_SENSORS_PAGE)
        {
            lv_obj_set_pos(ui_s_page_sensors, 0, 0);
            lv_obj_clear_flag(ui_s_page_sensors, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_set_pos(ui_s_page_sensors, 0, -h);
            lv_obj_add_flag(ui_s_page_sensors, LV_OBJ_FLAG_HIDDEN);
        }
    }


    if (ui_s_page_activity_summary)
    {
        lv_obj_set_size(ui_s_page_activity_summary, lv_pct(100), lv_pct(100));
        if (ui_s_current_page == UI_ACTIVITY_SUMMARY_PAGE)
        {
            lv_obj_set_pos(ui_s_page_activity_summary, 0, 0);
            lv_obj_clear_flag(ui_s_page_activity_summary, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_set_pos(ui_s_page_activity_summary, 0, -h);
            lv_obj_add_flag(ui_s_page_activity_summary, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ui_s_page_activity_detail)
    {
        lv_obj_set_size(ui_s_page_activity_detail, lv_pct(100), lv_pct(100));
        if (ui_s_current_page == UI_ACTIVITY_DETAIL_PAGE)
        {
            lv_obj_set_pos(ui_s_page_activity_detail, 0, 0);
            lv_obj_clear_flag(ui_s_page_activity_detail, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_set_pos(ui_s_page_activity_detail, 0, -h);
            lv_obj_add_flag(ui_s_page_activity_detail, LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_obj_t *live_bar = ui_status_bar_root(&ui_s_live_status);
    if (live_bar)
    {
        lv_obj_add_flag(live_bar, LV_OBJ_FLAG_HIDDEN);
    }


}

void ui_set_orientation(ui_orientation_t o)
{
    lvgl_port_lock(0);
    /* The Sharp panel is physically landscape. Keep LVGL's pixel coordinates
     * aligned with the 400x240 framebuffer; orientation only selects layouts. */
    lv_display_set_rotation(ui_s_disp, LV_DISPLAY_ROTATION_0);

    // Updates
    ui_pages_relayout();
    menu_page_on_orientation_changed();
    settings_page_on_orientation_changed();
    activity_summary_page_on_orientation_changed();
    activity_detail_page_on_orientation_changed();
    sensors_page_on_orientation_changed();
    if (ui_status_bar_root(&ui_s_live_status)) {
        ui_status_bar_set_orientation(&ui_s_live_status, o);
    }
    lvgl_port_unlock();

    data_page_set_orientation(o);
}

void ui_go_to_page(ui_page_t target, bool animated)
{
    if (target == ui_s_current_page)
        return;
    /* Confirm during MENU->setup animation used to drop SETUP->live nav,
     * leaving the start overlay on the setup page so PWR was ignored. */
    if (ui_s_transitioning)
    {
        s_pending_page = target;
        s_pending_animated = animated;
        return;
    }
    if (!ui_s_scr)
        return;
    if ((target == UI_PAGE_MENU) && !ui_s_page_menu)
        return;
    if ((target == UI_SETTINGS_PAGE) && !ui_s_page_settings)
        return;
    if ((target == UI_ACTIVITY_SUMMARY_PAGE) && !ui_s_page_activity_summary)
        return;
    if ((target == UI_ACTIVITY_DETAIL_PAGE) && !ui_s_page_activity_detail)
        return;
    lvgl_port_lock(0);
    if (target == UI_SENSORS_PAGE)
        ensure_sensors_page();
    if ((target == UI_SENSORS_PAGE) && !ui_s_page_sensors)
    {
        lvgl_port_unlock();
        return;
    }

    lv_coord_t h = lv_obj_get_height(ui_s_scr);

    if (!animated)
    {
        ui_s_current_page = target;
        ui_pages_relayout();
        lvgl_port_unlock();
        return;
    }

    lv_obj_t *anim_obj = NULL;
    int32_t from_y = 0, to_y = 0;
    ui_page_t next_page = target;

    // DATA -> MENU (open menu)
    if (target == UI_PAGE_MENU && ui_s_current_page == UI_PAGE_DATA)
    {
        anim_obj = ui_s_page_menu;
        lv_obj_clear_flag(ui_s_page_menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(ui_s_page_menu, -h);
        lv_obj_move_foreground(ui_s_page_menu);
        from_y = -h;
        to_y = 0;
        next_page = UI_PAGE_MENU;
    }
    // MENU -> DATA (close menu)
    else if (target == UI_PAGE_DATA && ui_s_current_page == UI_PAGE_MENU)
    {
        if (ui_s_page_data)
        {
            lv_obj_clear_flag(ui_s_page_data, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(ui_s_page_data, 0, 0);
        }
        anim_obj = ui_s_page_menu;
        from_y = 0;
        to_y = -h;
        next_page = UI_PAGE_DATA;
    }
    // MENU -> ACTIVITY_SUMMARY
    else if (target == UI_ACTIVITY_SUMMARY_PAGE && ui_s_current_page == UI_PAGE_MENU)
    {
        anim_obj = ui_s_page_activity_summary;
        lv_obj_clear_flag(anim_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(anim_obj, -h);
        lv_obj_move_foreground(anim_obj);
        from_y = -h;
        to_y = 0;
        next_page = UI_ACTIVITY_SUMMARY_PAGE;
    }
    // ACTIVITY_SUMMARY -> MENU
    else if (target == UI_PAGE_MENU && ui_s_current_page == UI_ACTIVITY_SUMMARY_PAGE)
    {
        if (ui_s_page_menu)
        {
            lv_obj_clear_flag(ui_s_page_menu, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(ui_s_page_menu, 0, 0);
        }
        anim_obj = ui_s_page_activity_summary;
        from_y = 0;
        to_y = -h;
        next_page = UI_PAGE_MENU;
    }
    // ACTIVITY_SUMMARY -> ACTIVITY_DETAIL
    else if (target == UI_ACTIVITY_DETAIL_PAGE && ui_s_current_page == UI_ACTIVITY_SUMMARY_PAGE)
    {
        anim_obj = ui_s_page_activity_detail;
        lv_obj_clear_flag(anim_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(anim_obj, -h);
        lv_obj_move_foreground(anim_obj);
        from_y = -h;
        to_y = 0;
        next_page = UI_ACTIVITY_DETAIL_PAGE;
    }
    // ACTIVITY_DETAIL -> ACTIVITY_SUMMARY
    else if (target == UI_ACTIVITY_SUMMARY_PAGE && ui_s_current_page == UI_ACTIVITY_DETAIL_PAGE)
    {
        // Make sure summary is ready behind the outgoing detail page
        lv_obj_clear_flag(ui_s_page_activity_summary, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(ui_s_page_activity_summary, 0);

        anim_obj = ui_s_page_activity_detail;
        from_y = 0;
        to_y = -h;
        next_page = UI_ACTIVITY_SUMMARY_PAGE;
    }
    // MENU -> SETTINGS (open settings)
    else if (target == UI_SETTINGS_PAGE && ui_s_current_page == UI_PAGE_MENU)
    {
        anim_obj = ui_s_page_settings;
        lv_obj_clear_flag(ui_s_page_settings, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(ui_s_page_settings, -h);
        lv_obj_move_foreground(ui_s_page_settings);
        from_y = -h;
        to_y = 0;
        next_page = UI_SETTINGS_PAGE;
    }
    // SETTINGS -> MENU (close settings)
    else if (target == UI_PAGE_MENU && ui_s_current_page == UI_SETTINGS_PAGE)
    {
        if (ui_s_page_menu)
        {
            lv_obj_clear_flag(ui_s_page_menu, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(ui_s_page_menu, 0, 0);
        }
        anim_obj = ui_s_page_settings;
        from_y = 0;
        to_y = -h;
        next_page = UI_PAGE_MENU;
    }
    else if (target == UI_SENSORS_PAGE && ui_s_current_page == UI_PAGE_MENU)
    {
        anim_obj = ui_s_page_sensors;
        lv_obj_clear_flag(anim_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(anim_obj, -h);
        lv_obj_move_foreground(anim_obj);
        from_y = -h;
        to_y = 0;
        next_page = UI_SENSORS_PAGE;
    }
    else if (target == UI_PAGE_MENU && ui_s_current_page == UI_SENSORS_PAGE)
    {
        if (ui_s_page_menu)
        {
            lv_obj_clear_flag(ui_s_page_menu, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(ui_s_page_menu, 0, 0);
        }
        anim_obj = ui_s_page_sensors;
        from_y = 0;
        to_y = -h;
        next_page = UI_PAGE_MENU;
    }
    else if (target == UI_SENSORS_PAGE && ui_s_current_page == UI_SETTINGS_PAGE)
    {
        anim_obj = ui_s_page_sensors;
        lv_obj_clear_flag(anim_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(anim_obj, -h);
        lv_obj_move_foreground(anim_obj);
        from_y = -h;
        to_y = 0;
        next_page = UI_SENSORS_PAGE;
    }
    else if (target == UI_SETTINGS_PAGE && ui_s_current_page == UI_SENSORS_PAGE)
    {
        if (ui_s_page_settings)
        {
            lv_obj_clear_flag(ui_s_page_settings, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(ui_s_page_settings, 0, 0);
        }
        anim_obj = ui_s_page_sensors;
        from_y = 0;
        to_y = -h;
        next_page = UI_SETTINGS_PAGE;
    }
    else
    {
        // Not handled yet (future pages)
        lvgl_port_unlock();
        return;
    }

    ui_s_current_page = next_page;
    ui_s_transitioning = true;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, anim_obj);
    lv_anim_set_exec_cb(&a, anim_set_y);
    lv_anim_set_values(&a, from_y, to_y);
    lv_anim_set_time(&a, 300);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&a, anim_done_cb);
    lv_anim_start(&a);

    lvgl_port_unlock();
}

void ui_yield_for_idle(void)
{
    vTaskDelay(1);
}

static void deferred_go_cb(lv_timer_t *t)
{
    ui_page_t page = (ui_page_t)(uintptr_t)lv_timer_get_user_data(t);
    if (ui_s_transitioning)
    {
        s_pending_page = page;
        s_pending_animated = true;
        return;
    }
    ui_go_to_page(page, true);
}

void ui_defer_go_to_page(ui_page_t page)
{
    lv_timer_t *timer = lv_timer_create(deferred_go_cb, 20, (void *)(uintptr_t)page);
    lv_timer_set_repeat_count(timer, 1);
}
