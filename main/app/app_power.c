#include "app_power.h"

#include "app_activity.h"
#include "app_context.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "gps_neo_m8n.h"
#include "mpu6050.h"
#include "pwr_key.h"
#include "ui.h"
#include "ui_data_page.h"

static const char *TAG = "app";

/* Bouton: GPIO 5 (actif bas), Hold: GPIO 10 */

static void cycle_display_preset(void)
{
    if (!s_disp) return;
    uint8_t preset = s_workout_config.display_preset;
    preset = (uint8_t)((preset + 1) % 3);
    s_workout_config.display_preset = preset;
    if (!lvgl_port_lock(0)) return;
    if (preset == 2) {
        ui_defer_go_to_page(UI_PAGE_ACCELERATION);
    } else {
        if (preset == 0) {
            const data_metric_t metrics[] = {DATA_METRIC_PACE, DATA_METRIC_TIME, DATA_METRIC_SPM,
                                             DATA_METRIC_DISTANCE, DATA_METRIC_HR};
            data_page_set_metrics(metrics, sizeof(metrics) / sizeof(metrics[0]));
        } else {
            const data_metric_t metrics[] = {DATA_METRIC_SPEED, DATA_METRIC_TIME, DATA_METRIC_SPM,
                                             DATA_METRIC_DISTANCE, DATA_METRIC_HR};
            data_page_set_metrics(metrics, sizeof(metrics) / sizeof(metrics[0]));
        }
        if (ui_get_current_page() == UI_PAGE_ACCELERATION) {
            ui_defer_go_to_page(UI_PAGE_DATA);
        } else if (ui_get_current_page() != UI_PAGE_DATA) {
            ui_defer_go_to_page(UI_PAGE_DATA);
        }
    }
    lvgl_port_unlock();
}

void on_shutdown_confirmed(void)
{
    /* Finish and close the activity file before cutting power. */
    if (s_activity_recording) {
        if (app_activity_post_cmd(ACT_CMD_STOP_SAVE) != ESP_OK) {
            ESP_LOGE(TAG, "Could not queue activity save; keeping device on");
            return;
        }

        TickType_t start = xTaskGetTickCount();
        while (s_activity_recording || s_act_log.opened) {
            if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(15000)) {
                ESP_LOGE(TAG, "Timed out saving activity; keeping device on");
                return;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    gps_neo_m8n_power_down();
    mpu6050_sleep(&s_imu);
    pwr_key_set_hold(false);
    
    /* Allow time for power latch to cut power, then enter deep sleep as fallback */
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_deep_sleep_start();
}

static void pwr_evt_cb(pwr_key_event_t evt, void *user)
{
    (void)user;
    ui_notify_user_activity();

    switch (evt)
    {
    case PWR_KEY_EVT_SHORT_PRESS:
        /* Appui bref: démarre si inactif, met en pause si en cours */
        if (!s_activity_recording) {
            ESP_LOGI(TAG, "Short press: start activity");
            esp_err_t err = app_activity_post_cmd(ACT_CMD_START);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "start post: %s", esp_err_to_name(err));
            } else if (lvgl_port_lock(0)) {
                ui_defer_go_to_page(UI_PAGE_DATA);
                lvgl_port_unlock();
            }
        } else if (!s_activity_paused) {
            ESP_LOGI(TAG, "Short press: pause activity");
            (void)app_activity_post_cmd(ACT_CMD_PAUSE);
        } else {
            ESP_LOGI(TAG, "Short press: resume activity");
            (void)app_activity_post_cmd(ACT_CMD_RESUME);
        }
        break;

    case PWR_KEY_EVT_DOUBLE_PRESS:
        /* Double appui: changement d'affichage */
        ESP_LOGI(TAG, "Double press: cycle display");
        cycle_display_preset();
        break;

    case PWR_KEY_EVT_LONG_PRESS:
        /* Appui long: si en pause -> sauvegarde + résumé; si pas d'activité -> arrêt */
        if (s_activity_recording && s_activity_paused) {
            ESP_LOGI(TAG, "Long press (paused): save activity");
            (void)app_activity_post_cmd(ACT_CMD_STOP_SAVE);
            /* activity_worker_task will show summary after save */
        } else if (!s_activity_recording) {
            ESP_LOGI(TAG, "Long press (idle): shutdown");
            on_shutdown_confirmed();
        } else {
            ESP_LOGI(TAG, "Long press ignored while recording (pause first)");
        }
        break;

    default:
        break;
    }
}

void app_pwr_key_setup(void)
{
    /* Bouton: GPIO 5 (actif bas), Hold: GPIO 10 (ESP32-C3 Super Mini) */
    pwr_key_config_t cfg = {
        .key_gpio = GPIO_NUM_5,
        .hold_gpio = GPIO_NUM_10,
        .key_active_low = true,
        .debounce_ms = 30,
        .poll_ms = 20,
        .long_press_ms = 3000,
        .double_press_ms = 350,
    };
    ESP_ERROR_CHECK(pwr_key_init(&cfg, pwr_evt_cb, NULL));
    pwr_key_set_hold(true);
}
