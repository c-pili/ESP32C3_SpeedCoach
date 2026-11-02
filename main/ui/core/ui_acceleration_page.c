#include "ui_acceleration_page.h"
#include "ui_theme.h"
#include <math.h>

static lv_chart_series_t *s_series;
static lv_obj_t *s_chart;

void acceleration_page_create(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Acceleration en direct (m/s²)");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);
    s_chart = lv_chart_create(parent);
    lv_obj_set_size(s_chart, lv_pct(94), lv_pct(72));
    lv_obj_align(s_chart, LV_ALIGN_CENTER, 0, 16);
    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, 100);
    lv_chart_set_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, -50, 50);
    lv_chart_set_div_line_count(s_chart, 5, 5);
    s_series = lv_chart_add_series(s_chart, ui_theme_color_accent(), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_values(s_chart, s_series, 0);
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
