#include "app_activity.h"

#include "storage_paths.h"
#include "app_context.h"
#include "wifi_export.h"
#include "app_imu.h"
#include "activity.h"
#include "activity_log.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_helper.h"
#include "ui.h"
#include "ui_data_page.h"
#include <stdio.h>
#include <time.h>

static bool start_activity_log(uint32_t id)
{
    esp_err_t err = s_storage.mounted
        ? activity_log_start(&s_act_log, &s_storage, s_activity.start_ts, id)
        : ESP_ERR_INVALID_STATE;
    if (err == ESP_OK)
        return true;

    ESP_LOGE("ACT", "activity %lu not started: storage/logger error %s",
             (unsigned long)id, esp_err_to_name(err));
    if (s_activity_mutex)
        xSemaphoreTake(s_activity_mutex, portMAX_DELAY);
    s_activity_recording = false;
    s_session_last_us = 0;
    activity_stop(&s_activity, time(NULL));
    if (s_activity_mutex)
        xSemaphoreGive(s_activity_mutex);
    (void)wifi_export_start();
    data_page_show_activity_toast(false);
    return false;
}

void activity_worker_task(void *arg)
{
    (void)arg;

    act_cmd_t cmd;

    for (;;)
    {
        if (xQueueReceive(s_act_q, &cmd, portMAX_DELAY) != pdTRUE)
            continue;

        if (s_activity_mutex)
            xSemaphoreTake(s_activity_mutex, portMAX_DELAY);

        if (cmd == ACT_CMD_START && !s_storage.mounted) {
            if (s_activity_mutex)
                xSemaphoreGive(s_activity_mutex);
            ESP_LOGE("ACT", "activity start rejected: flash storage is not mounted");
            data_page_show_activity_toast(false);
            continue;
        }

        if (cmd == ACT_CMD_START)
        {
            (void)wifi_export_stop();
            s_activity_recording = true;
            s_session_time_s = 0.0f;
            s_session_last_us = esp_timer_get_time();
            app_imu_request_session_reset();

            uint32_t id = nvs_helper_get_next_activity_id();
            activity_init(&s_activity, id);
            activity_start(&s_activity, time(NULL));
            activity_log_set_split_interval(&s_act_log, s_current_split_m);
            s_last_session_stroke_count = 0;

            if (s_activity_mutex)
                xSemaphoreGive(s_activity_mutex);

            if (!start_activity_log(id))
                continue;

            data_page_show_activity_toast(true);
            ESP_LOGI("ACT", "START id=%lu", (unsigned long)id);
            continue;
        }

        if (cmd == ACT_CMD_STOP_SAVE)
        {
            if (s_session_last_us > 0) {
                int64_t elapsed_us = esp_timer_get_time() - s_session_last_us;
                if (elapsed_us < 0)
                    elapsed_us = 0;
                uint32_t dur_ms = (uint32_t)(elapsed_us / 1000);
                if (dur_ms > s_activity.duration_ms)
                    s_activity.duration_ms = dur_ms;
            }

            activity_stop(&s_activity, time(NULL));

            activity_t snapshot = s_activity;
            if (s_activity_mutex)
                xSemaphoreGive(s_activity_mutex);

            data_page_show_activity_toast(false);
            ESP_LOGI("ACT", "STOP id=%lu Dist=%.1fm dur=%.1fs",
                     (unsigned long)snapshot.id, (double)snapshot.distance_m,
                     (double)(snapshot.duration_ms / 1000.0f));

            float final_time_s = snapshot.duration_ms / 1000.0f;
            if (s_act_log.opened)
            {
                /* Drain IMU log messages so last GPS/stroke distance is in the file
                 * before we write the remainder split. */
                activity_log_msg_t pending;
                while (s_log_q && xQueueReceive(s_log_q, &pending, 0) == pdTRUE)
                {
                    activity_log_append(&s_act_log, &pending.row);
                }

                float dist_m = snapshot.distance_m;
                if (s_act_log.last_row_distance_m > dist_m)
                    dist_m = s_act_log.last_row_distance_m;

                bool need_final_row = !s_act_log.has_last_row ||
                                      (dist_m - s_act_log.last_row_distance_m) >= 0.1f ||
                                      (final_time_s - s_act_log.last_row_time_s) >= 0.1f;

                if (need_final_row)
                {
                    activity_log_row_t final_row = {0};
                    float avg_pace_s = (snapshot.avg_speed_mps > 0.1f) ? (500.0f / snapshot.avg_speed_mps) : 0.0f;

                    final_row.rtc_time = snapshot.end_ts;
                    final_row.session_time_s = final_time_s;
                    final_row.total_distance_m = dist_m;
                    final_row.pace_500m_s = avg_pace_s;
                    final_row.spm_instant = snapshot.avg_spm;
                    final_row.avg_pace_500m_s = avg_pace_s;
                    final_row.avg_speed_mps = snapshot.avg_speed_mps;
                    final_row.stroke_length_m = 0.0f;
                    final_row.stroke_count = snapshot.stroke_count;
                    final_row.gps_lat = 0.0;
                    final_row.gps_lon = 0.0;
                    final_row.power_w = 0.0f;
                    final_row.drive_time_s = 0.0f;
                    final_row.recovery_time_s = 0.0f;
                    final_row.recovery_ratio = 0.0f;

                    activity_log_append(&s_act_log, &final_row);
                }

                activity_log_finalize(&s_act_log, dist_m, final_time_s, snapshot.avg_spm);
            }

            // STOP THE LOGGER: This flushes and closes the CSV file.
            // The file is now complete and saved to flash storage.
            activity_log_stop(&s_act_log);

            FILE *f = fopen(ACTIVITY_INDEX_PATH, "a");
            if (f)
            {
                uint32_t duration_s = (uint32_t)(snapshot.duration_ms / 1000);
                float avg_pace_s_per500 = 0.0f;
                if (snapshot.avg_speed_mps > 0.01f)
                {
                    avg_pace_s_per500 = 500.0f / snapshot.avg_speed_mps; // seconds per 500m
                }

                fprintf(f, "%lu,%ld,%lu,%.2f,%.2f,%s\n",
                        (unsigned long)snapshot.id,
                        (long)snapshot.start_ts,
                        (unsigned long)duration_s,
                        (double)snapshot.distance_m,
                        (double)avg_pace_s_per500,
                        s_act_log.filename_base);

                fclose(f);
            }

            // Publish the stopped state only after the activity data and
            // index have been flushed, so a power-off request can wait safely.
            s_activity_recording = false;
            esp_err_t ap_err = wifi_export_start();
            if (ap_err != ESP_OK) ESP_LOGE("ACT", "Wi-Fi export AP restart failed: %s", esp_err_to_name(ap_err));
        }
    }
}

esp_err_t app_activity_post_cmd(act_cmd_t cmd)
{
    if (!s_act_q)
        return ESP_ERR_INVALID_STATE;
    if (xQueueSend(s_act_q, &cmd, 0) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    return ESP_OK;
}

void activity_logger_task(void *arg)
{
    (void)arg;

    activity_log_msg_t msg;
    for (;;)
    {
        if (xQueueReceive(s_log_q, &msg, portMAX_DELAY) == pdTRUE)
        {
            // Only append if file is open
            if (s_act_log.opened)
            {
                activity_log_append(&s_act_log, &msg.row);
            }
        }
    }
}
