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
static bool s_sensor_hub_init_attempted;

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

static esp_err_t web_start_sensor_hub(void)
{
    if (sensor_hub_available()) return ESP_OK;
    if (s_sensor_hub_init_attempted) return ESP_ERR_INVALID_STATE;
    s_sensor_hub_init_attempted = true;
    esp_err_t err = sensor_hub_init();
    if (err != ESP_OK)
        ESP_LOGW(TAG, "BLE sensor startup failed: %s", esp_err_to_name(err));
    return err;
}

void app_main(void)
{
    /* The ESP-IDF Wi-Fi driver stores calibration/configuration in NVS. */
    ESP_ERROR_CHECK(nvs_helper_init());

    /* Activity queues and callbacks are initialized before starting the Wi-Fi AP
     * and UI so web requests and button presses are safe immediately. */
    s_activity_mutex = xSemaphoreCreateMutex();
    activity_init(&s_activity, 0);
    s_session_time_s = 0.0f;
    s_session_last_us = esp_timer_get_time();

    uint32_t saved_split = nvs_helper_get_split_len();
    s_current_split_m = saved_split;
    activity_log_init(&s_act_log);
    activity_log_set_split_interval(&s_act_log, saved_split);

    s_act_q = xQueueCreate(4, sizeof(act_cmd_t));
    ESP_ERROR_CHECK(s_act_q ? ESP_OK : ESP_ERR_NO_MEM);
    s_log_q = xQueueCreate(LOG_QUEUE_LEN, sizeof(activity_log_msg_t));
    ESP_ERROR_CHECK(s_log_q ? ESP_OK : ESP_ERR_NO_MEM);

    wifi_export_set_callbacks(web_configure_workout, web_activity_ready, web_start_activity,
                              ui_status_bar_read_battery_percent, web_start_sensor_hub);

    /* Bring up the AP before touching optional peripherals. This keeps the
     * configuration page reachable even when an IMU, GPS, RTC or LCD is absent. */
    esp_err_t ap_err = wifi_export_start();
    if (ap_err != ESP_OK) ESP_LOGE(TAG, "Wi-Fi export AP failed: %s", esp_err_to_name(ap_err));

    const bool imu_ready = init_imu();
    if (!imu_ready)
        ESP_LOGW(TAG, "MPU6050 absent at boot: stroke task will retry automatically");

    if (s_imu_bus.bus && PCF85063_init(&s_imu_bus) == ESP_OK) {
        app_set_time_from_rtc();
    }

    /* Initialize the Sharp LS027 display and LVGL regardless of IMU status so
     * the screen always works on the breadboard. */
    esp_err_t display_err = ESP_ERR_NOT_SUPPORTED;
    if (heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) >= 20000)
    {
        display_err = init_display_and_lvgl();
    }
    else
    {
        ESP_LOGW(TAG, "Skipping local display: insufficient internal RAM (largest=%u)",
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    }
    const bool ui_ready = (display_err == ESP_OK);
    s_ui_ready = ui_ready;

    if (ui_ready) {
        ui_init(s_disp);

        const data_metric_t metrics[] = {
            DATA_METRIC_PACE,
            DATA_METRIC_TIME,
            DATA_METRIC_SPM,
            DATA_METRIC_HR,
            DATA_METRIC_DISTANCE,
        };
        data_page_set_metrics(metrics, sizeof(metrics) / sizeof(metrics[0]));

        bool saved_dark = nvs_helper_get_dark_mode();
        ui_set_dark_mode(saved_dark);
        /* The 400x240 memory LCD is landscape and the one-button UI has no
         * orientation control, so keep the physical panel in landscape. */
        ui_set_orientation(UI_ORIENT_LANDSCAPE_270);
        ui_register_dark_mode_cb(on_dark_mode_setting_changed);
        ui_register_auto_rotate_cb(on_auto_rotate_setting_changed);
        app_pwr_key_setup();
    }

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

    BaseType_t logger_ok = xTaskCreate(activity_logger_task, "activity_logger",
                                       3072, NULL, 6, NULL);
    BaseType_t worker_ok = xTaskCreate(activity_worker_task, "activity_worker",
                                       3072, NULL, 9, &s_act_worker_task);
    if (logger_ok != pdPASS || worker_ok != pdPASS) {
        ESP_LOGE(TAG, "Activity task creation failed (logger=%d worker=%d)",
                 (int)logger_ok, (int)worker_ok);
    }

    /* Always start stroke_task so live UI snapshots (GPS, time, HR, and IMU
     * strokes/acceleration) are published continuously. */
    BaseType_t stroke_ok = xTaskCreatePinnedToCore(stroke_task, "stroke",
                                                   3072, NULL, 3, NULL, 0);
    if (stroke_ok != pdPASS) {
        ESP_LOGE(TAG, "stroke task create FAILED, free internal=%u largest=%u",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    } else {
        ESP_LOGI(TAG, "stroke task started, free internal=%u",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }

    /* Mount SPIFFS at idle priority so a first-boot partition format shares
     * CPU 0 with IDLE0 and never triggers task_wdt or starves Wi-Fi/LVGL. */
    UBaseType_t orig_prio = uxTaskPriorityGet(NULL);
    vTaskPrioritySet(NULL, tskIDLE_PRIORITY);
    esp_err_t storage_err = flash_storage_mount(&s_storage);
    vTaskPrioritySet(NULL, orig_prio);
    if (storage_err != ESP_OK)
    {
        ESP_LOGW(TAG, "Flash activity storage mount failed: %s", esp_err_to_name(storage_err));
    }

    /* app_main can idle */
    while (1)
    {
        app_activity_monitor_targets();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
