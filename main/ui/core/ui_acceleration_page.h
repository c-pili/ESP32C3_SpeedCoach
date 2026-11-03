#pragma once
#include "lvgl.h"

void acceleration_page_create(lv_obj_t *parent);
void acceleration_page_add_sample(float acceleration_mps2);
void acceleration_page_apply_theme(void);
