#include "ui_acceleration_page.h"
#include "ui_theme.h"
#include <math.h>

static lv_chart_series_t *s_series = NULL;
static lv_obj_t *s_chart = NULL;
static lv_obj_t *s_hr_label = NULL;
static lv_obj_t *s_spm_label = NULL;

void acceleration_page_create(lv_obj_t *parent)
{
    s_chart = lv_chart_create(parent);
    lv_obj_set_size(s_chart, lv_pct(100), lv_pct(100));
    lv_obj_align(s_chart, LV_ALIGN_CENTER, 0, 0);
    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, 200);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, -50, 50);
    lv_chart_set_div_line_count(s_chart, 5, 5);
    s_series = lv_chart_add_series(s_chart, ui_theme_color_accent(), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_values(s_chart, s_series, 0);
    lv_obj_set_style_border_width(s_chart, 0, 0);
    lv_obj_set_style_pad_all(s_chart, 0, 0);

    /* BPM top-left */
    s_hr_label = lv_label_create(s_chart);
    lv_obj_set_style_text_font(s_hr_label, ui_font_value_lg(), 0);
    ui_theme_apply_label(s_hr_label, false);
    lv_obj_align(s_hr_label, LV_ALIGN_TOP_LEFT, 8, 8);
    lv_label_set_text(s_hr_label, "HR: --");

    /* SPM top-right */
    s_spm_label = lv_label_create(s_chart);
    lv_obj_set_style_text_font(s_spm_label, ui_font_value_lg(), 0);
    ui_theme_apply_label(s_spm_label, false);
    lv_obj_align(s_spm_label, LV_ALIGN_TOP_RIGHT, -8, 8);
    lv_label_set_text(s_spm_label, "SPM: --");

    acceleration_page_apply_theme();
}

void acceleration_page_apply_theme(void)
{
    if (s_chart) {
        ui_theme_apply_surface_border(s_chart);
        lv_obj_set_style_line_color(s_chart, ui_theme_palette()->border, LV_PART_MAIN);
        if (s_series) {
            lv_chart_set_series_color(s_chart, s_series, ui_theme_color_accent());
        }
        lv_chart_refresh(s_chart);
    }
    if (s_hr_label) {
        ui_theme_apply_label(s_hr_label, false);
    }
    if (s_spm_label) {
        ui_theme_apply_label(s_spm_label, false);
    }
}

void acceleration_page_add_sample(float acceleration_mps2)
{
    if (s_series && isfinite(acceleration_mps2)) {
        int32_t value = (int32_t)(acceleration_mps2 * 10.0f);
        if (value < -50) value = -50;
        if (value > 50) value = 50;
        lv_chart_set_next_value(s_chart, s_series, value);
    }
}

void acceleration_page_set_hr(uint8_t hr_bpm)
{
    if (s_hr_label) {
        if (hr_bpm > 0) {
            lv_label_set_text_fmt(s_hr_label, "HR: %d", hr_bpm);
        } else {
            lv_label_set_text(s_hr_label, "HR: --");
        }
    }
}

void acceleration_page_set_spm(float spm)
{
    if (s_spm_label) {
        if (isfinite(spm) && spm > 0) {
            lv_label_set_text_fmt(s_spm_label, "SPM: %.0f", spm);
        } else {
            lv_label_set_text(s_spm_label, "SPM: --");
        }
    }
}
