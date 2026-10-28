#pragma once

#include "esp_err.h"
#include "gps_neo_m8n.h"

esp_err_t app_set_time_from_rtc(void);
void gps_fix_cb(const gps_fix_t *fix, void *user);
