#pragma once
#include "esp_err.h"
#include <stdint.h>

typedef enum {
    WIFI_WORKOUT_FREE = 0,
    WIFI_WORKOUT_TIME,
    WIFI_WORKOUT_DISTANCE,
    WIFI_WORKOUT_INTERVAL,
} wifi_workout_mode_t;

typedef struct {
    wifi_workout_mode_t mode;
    uint32_t duration_s;
    uint32_t distance_m;
    uint32_t interval_work_s;
    uint32_t interval_rest_s;
    uint32_t interval_reps;
    uint8_t display_preset;
} wifi_workout_config_t;

typedef esp_err_t (*wifi_config_callback_t)(const wifi_workout_config_t *config);
typedef esp_err_t (*wifi_activity_ready_callback_t)(void);
typedef esp_err_t (*wifi_activity_callback_t)(void);
typedef esp_err_t (*wifi_battery_callback_t)(int *percent);

esp_err_t wifi_export_start(void);
esp_err_t wifi_export_stop(void);
void wifi_export_set_callbacks(wifi_config_callback_t config_cb,
                               wifi_activity_ready_callback_t ready_cb,
                               wifi_activity_callback_t start_cb,
                               wifi_battery_callback_t battery_cb);
