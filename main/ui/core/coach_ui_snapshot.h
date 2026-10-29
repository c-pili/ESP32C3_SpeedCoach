#pragma once

#include "activity.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SENSOR_LINK_NONE = 0,   /* no saved strap, or the user disconnected it */
    SENSOR_LINK_LIVE,       /* a heart-rate sample newer than 5 s */
    SENSOR_LINK_LOST,       /* saved strap, but no fresh sample */
} sensor_link_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Immutable copy of live coach metrics.
 * Produced by stroke_task; consumed only on the LVGL thread.
 */
typedef struct {
    uint32_t seq;
    bool recording;
    bool gps_connected;
    bool gps_ok;
    bool gps_stale;
    uint8_t gps_bars;
    uint8_t battery_pct;

    float time_s;
    float distance_m;
    float pace_s_per_500m;
    float avg_pace_s_per_500m;
    float speed_mps;
    float spm;
    float stroke_len_m;
    uint32_t stroke_count;

    uint32_t split_len_m;
    float split_progress_m;
    float last_split_pace_s;
    float last_split_delta_s;

    uint16_t hr_bpm;            /* 0 when no fresh sample */
    sensor_link_t sensor_link;
    bool pod_valid;
    float pod_catch_deg;
    float pod_finish_deg;
    float pod_arc_deg;
} coach_ui_snapshot_t;

void coach_ui_snapshot_init(void);

/** Any task: replace the published snapshot. */
void coach_ui_snapshot_publish(const coach_ui_snapshot_t *src);

/** LVGL thread: copy latest snapshot. Returns false if none published yet. */
bool coach_ui_snapshot_copy(coach_ui_snapshot_t *dst);

#ifdef __cplusplus
}
#endif
