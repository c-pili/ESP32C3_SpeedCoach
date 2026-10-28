#include "pwr_key.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <limits.h>

static pwr_key_config_t s_cfg;
static pwr_key_cb_t s_cb = NULL;
static void *s_user = NULL;

static bool s_hold_state = false;

static inline bool key_is_pressed_raw(void)
{
    int lvl = gpio_get_level(s_cfg.key_gpio);
    return s_cfg.key_active_low ? (lvl == 0) : (lvl != 0);
}

void pwr_key_set_hold(bool on)
{
    s_hold_state = on;
    gpio_set_level(s_cfg.hold_gpio, on ? 1 : 0);
}

bool pwr_key_get_hold(void)
{
    return s_hold_state;
}

static void pwr_key_task(void *arg)
{
    (void)arg;

    bool last_raw = false;
    bool debounced = false;
    int64_t last_change_ms = 0;

    bool pressed = false;
    int64_t press_start_ms = -1;

    bool shutdown_fired = false;

    while (1) {
        int64_t now_ms = esp_timer_get_time() / 1000;

        bool raw = key_is_pressed_raw();
        if (raw != last_raw) {
            last_raw = raw;
            last_change_ms = now_ms;
        }

        if ((now_ms - last_change_ms) >= (int64_t)s_cfg.debounce_ms) {
            debounced = raw;
        }

        // press edge
        if (debounced && !pressed) {
            pressed = true;
            press_start_ms = now_ms;
            shutdown_fired = false;
        }

        // A held press powers the device off without requiring a touchscreen.
        if (pressed && press_start_ms >= 0 && !shutdown_fired) {
            int64_t held = now_ms - press_start_ms;
            if ((uint32_t)held >= s_cfg.shutdown_hold_ms) {
                shutdown_fired = true;
                if (s_cb) s_cb(PWR_KEY_EVT_SHUTDOWN, s_user);
            }
        }

        // release edge
        if (!debounced && pressed) {
            pressed = false;
            // Every release before the shutdown hold threshold toggles the
            // activity. There is no separate display-sleep action on a
            // button-only device.
            if (!shutdown_fired && s_cb)
                s_cb(PWR_KEY_EVT_ACTIVITY_TOGGLE, s_user);

            press_start_ms = -1;
        }

        vTaskDelay(pdMS_TO_TICKS(s_cfg.poll_ms));
    }
}

esp_err_t pwr_key_init(const pwr_key_config_t *cfg, pwr_key_cb_t cb, void *user)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;

    s_cfg = *cfg;
    s_cb = cb;
    s_user = user;

    if (s_cfg.debounce_ms == 0) s_cfg.debounce_ms = 30;
    if (s_cfg.poll_ms == 0) s_cfg.poll_ms = 20;
    if (s_cfg.shutdown_hold_ms == 0) s_cfg.shutdown_hold_ms = 5000;

    // KEY input
    gpio_config_t in = {
        .pin_bit_mask = 1ULL << s_cfg.key_gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = s_cfg.key_active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = s_cfg.key_active_low ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&in));

    // HOLD output
    gpio_config_t out = {
        .pin_bit_mask = 1ULL << s_cfg.hold_gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&out));

    // You can set this true immediately if you want the device to stay on.
    // pwr_key_set_hold(true);

    xTaskCreate(pwr_key_task, "pwr_key", 4096, NULL, 10, NULL);
    return ESP_OK;
}
