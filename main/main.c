#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "activity.h"
#include "activity_log.h"
#include "gps_neo_m8n.h"
#include "nvs_helper.h"
#include "sensor_hub.h"
#include "rtc_pcf85063.h"
#include "flash_storage.h"
#include "wifi_export.h"

#include "ui.h"
#include "ui_data_page.h"
#include "ui_settings_page.h"
#include "ui/settings/ui_status_bar.h"

#include "app/app_activity.h"
#include "app/app_context.h"
#include "app/app_display.h"
#include "app/app_imu.h"
#include "app/app_power.h"
#include "app/app_settings.h"
#include "app/app_time.h"

static const char *TAG = "app";
static bool s_ui_ready;

#define LOG_QUEUE_LEN 32

static esp_err_t web_configure_workout(const wifi_workout_config_t *config)
{
    if (!config || s_activity_recording) return ESP_ERR_INVALID_STATE;
    s_workout_config = *config;
    if (!s_ui_ready) return ESP_OK;
    if (config->display_preset == 2) {
        ui_go_to_page(UI_PAGE_ACCELERATION, false);
    } else if (config->display_preset == 0) {
        const data_metric_t metrics[] = {DATA_METRIC_PACE, DATA_METRIC_TIME, DATA_METRIC_SPM,
                                         DATA_METRIC_DISTANCE, DATA_METRIC_HR};
        data_page_set_metrics(metrics, sizeof(metrics) / sizeof(metrics[0]));
    } else {
        const data_metric_t metrics[] = {DATA_METRIC_SPEED, DATA_METRIC_TIME, DATA_METRIC_SPM,
                                         DATA_METRIC_DISTANCE, DATA_METRIC_HR};
        data_page_set_metrics(metrics, sizeof(metrics) / sizeof(metrics[0]));
    }
    if (config->display_preset != 2 && ui_get_current_page() == UI_PAGE_ACCELERATION)
        ui_go_to_page(UI_PAGE_DATA, false);
    return ESP_OK;
}

static esp_err_t web_start_activity(void)
{
    return app_activity_post_cmd(ACT_CMD_START);
}

static esp_err_t web_activity_ready(void)
{
    if (s_activity_recording || !s_storage.mounted) return ESP_ERR_INVALID_STATE;
    return ESP_OK;
}

void app_main(void)
{
    /* The ESP-IDF Wi-Fi driver stores calibration/configuration in NVS. */
    ESP_ERROR_CHECK(nvs_helper_init());

    /* Bring up the AP before touching optional peripherals. This keeps the
     * configuration page reachable even when an IMU, GPS, RTC or LCD is absent. */
    esp_err_t ap_err = wifi_export_start();
    if (ap_err != ESP_OK) ESP_LOGE(TAG, "Wi-Fi export AP failed: %s", esp_err_to_name(ap_err));

    /* A bare board has no IMU and does not need the full coach stack. Staying
     * in this small mode preserves heap for Wi-Fi association and HTTP. */
    if (!init_imu()) {
        ESP_LOGW(TAG, "MPU6050 absent: keeping Wi-Fi setup/export active in bench mode");
        wifi_export_set_callbacks(NULL, NULL, NULL, ui_status_bar_read_battery_percent);
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (s_imu_bus.bus) (void)PCF85063_init(&s_imu_bus);
    esp_err_t sensor_err = sensor_hub_init();
    if (sensor_err != ESP_OK)
        ESP_LOGW(TAG, "BLE sensors unavailable: %s", esp_err_to_name(sensor_err));

    esp_err_t display_err = init_display_and_lvgl();
    const bool ui_ready = (display_err == ESP_OK);
    s_ui_ready = ui_ready;

    gps_neo_m8n_config_t gps_cfg = {
        .uart_num = UART_NUM_1,
        .tx_gpio = 21, // ESP32-C3 UART1 TX
        .rx_gpio = 20, // ESP32-C3 UART1 RX
        .baud = 9600,  // common NEO-M8N default
        .task_prio = 8,
        .task_stack = 4096,
        .rx_buf_size = 2048,
    };
    esp_err_t gps_err = gps_neo_m8n_init(&gps_cfg);
    if (gps_err == ESP_OK) gps_err = gps_neo_m8n_set_callback(gps_fix_cb, NULL);
    if (gps_err != ESP_OK)
        ESP_LOGW(TAG, "GPS unavailable: %s; Wi-Fi remains enabled", esp_err_to_name(gps_err));

    app_set_time_from_rtc();

    /* Activity queue must exist before pwr_key_task can post START/STOP.
     * ui_init() is heavy; a PWR press during it used to hit xQueueSend(NULL). */
    s_activity_mutex = xSemaphoreCreateMutex();
    activity_init(&s_activity, 0);
    s_session_time_s = 0.0f;
    s_session_last_us = esp_timer_get_time();

    s_act_q = xQueueCreate(4, sizeof(act_cmd_t));
    ESP_ERROR_CHECK(s_act_q ? ESP_OK : ESP_ERR_NO_MEM);
    s_log_q = xQueueCreate(LOG_QUEUE_LEN, sizeof(activity_log_msg_t));
    ESP_ERROR_CHECK(s_log_q ? ESP_OK : ESP_ERR_NO_MEM);

    xTaskCreate(activity_logger_task, "activity_logger", 6144, NULL, 6, NULL);
    xTaskCreate(activity_worker_task, "activity_worker", 8192, NULL, 9, &s_act_worker_task);
    if (ui_ready) {
        /* Create UI in separate module */
        ui_init(s_disp);

        /* Fixed dashboard metrics for button-only operation. */
        const data_metric_t metrics[] = {
            DATA_METRIC_PACE,
            DATA_METRIC_TIME,
            DATA_METRIC_SPM,
            DATA_METRIC_HR,
            DATA_METRIC_DISTANCE,
        };
        data_page_set_metrics(metrics, sizeof(metrics) / sizeof(metrics[0]));
    }
    wifi_export_set_callbacks(web_configure_workout, web_activity_ready, web_start_activity,
                              ui_status_bar_read_battery_percent);

    esp_err_t storage_err = flash_storage_mount(&s_storage);
    if (storage_err != ESP_OK)
    {
        ESP_LOGW(TAG, "Flash activity storage mount failed: %s", esp_err_to_name(storage_err));
    }
    bool saved_dark = nvs_helper_get_dark_mode();
    if (ui_ready) ui_set_dark_mode(saved_dark);
    /* The 400x240 memory LCD is landscape and the one-button UI has no
     * orientation control, so keep the physical panel in landscape. */
    if (ui_ready) ui_set_orientation(UI_ORIENT_LANDSCAPE_270);
    uint32_t saved_split = nvs_helper_get_split_len();
    s_current_split_m = saved_split;
    activity_log_init(&s_act_log);
    activity_log_set_split_interval(&s_act_log, saved_split);

    if (ui_ready) {
        ui_register_dark_mode_cb(on_dark_mode_setting_changed);
        ui_register_auto_rotate_cb(on_auto_rotate_setting_changed);
    }

    app_pwr_key_setup();

    /* Created last, so it competes for whatever internal DRAM is left after
     * LVGL/BLE. A silent pdFAIL here kills SPM with no other symptom. */
    BaseType_t stroke_ok = xTaskCreatePinnedToCore(stroke_task, "stroke",
                                                   6144, NULL, 3, NULL, 0);
    if (stroke_ok != pdPASS)
    {
        ESP_LOGE(TAG, "stroke task create FAILED, free internal=%u largest=%u",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    }
    else
    {
        ESP_LOGI(TAG, "stroke task started, free internal=%u",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }

    /* app_main can idle */
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
