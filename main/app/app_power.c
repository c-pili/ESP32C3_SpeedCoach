#include "app_power.h"

#include "app_activity.h"
#include "app_context.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "pwr_key.h"
#include "ui.h"

static const char *TAG = "app";

void on_shutdown_confirmed(void)
{
    // A long press is the only shutdown confirmation on the button-only unit.
    // Finish and close the activity file before cutting power.
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
    pwr_key_set_hold(false);
}

static void pwr_evt_cb(pwr_key_event_t evt, void *user)
{
    (void)user;

    switch (evt)
    {
    case PWR_KEY_EVT_ACTIVITY_TOGGLE:
    {
        ui_notify_user_activity();
        if (!s_activity_recording)
        {
            // Button-only operation always starts a standard activity and
            // returns the display to the live metrics page.
            esp_err_t err = app_activity_post_cmd(ACT_CMD_START);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "start post: %s", esp_err_to_name(err));
            } else if (lvgl_port_lock(0)) {
                ui_defer_go_to_page(UI_PAGE_DATA);
                lvgl_port_unlock();
            }
        }
        else
        {
            // Save immediately: there is no second button/touch confirmation.
            esp_err_t err = app_activity_post_cmd(ACT_CMD_STOP_SAVE);
            if (err != ESP_OK)
                ESP_LOGW(TAG, "stop/save post: %s", esp_err_to_name(err));
        }
        break;
    }

    case PWR_KEY_EVT_SHUTDOWN:
        ESP_LOGI(TAG, "Long press: save activity if needed and power off");
        on_shutdown_confirmed();
        break;

    default:
        break;
    }
}

void app_pwr_key_setup(void)
{
    // Start the power key task
    pwr_key_config_t cfg = {
        .key_gpio = GPIO_NUM_6,
        .hold_gpio = GPIO_NUM_7,
        .key_active_low = true,
        .debounce_ms = 30,
        .poll_ms = 20,
        .shutdown_hold_ms = 5000,
    };
    ESP_ERROR_CHECK(pwr_key_init(&cfg, pwr_evt_cb, NULL));

    // Keep power latched on
    pwr_key_set_hold(true);
}
