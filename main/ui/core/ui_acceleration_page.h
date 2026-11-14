#pragma once
#include "lvgl.h"
#include <stdint.h>

void acceleration_page_create(lv_obj_t *parent);
void acceleration_page_add_sample(float acceleration_mps2);
void acceleration_page_apply_theme(void);
void acceleration_page_set_hr(uint8_t hr_bpm);
void acceleration_page_set_spm(float spm);
