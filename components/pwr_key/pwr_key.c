#include "pwr_key.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

static pwr_key_config_t s_cfg = {0};
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
    bool long_fired = false;
    int64_t press_start_ms = -1;

    bool pending_single = false;
    int64_t release_ms = 0;

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

        /* Press edge */
        if (debounced && !pressed) {
            pressed = true;
            press_start_ms = now_ms;
            long_fired = false;

            if (pending_single) {
                /* Second press within window -> double press */
                pending_single = false;
                if (s_cb) s_cb(PWR_KEY_EVT_DOUBLE_PRESS, s_user);
                /* Consume this press; don't treat release as another event */
                press_start_ms = -1;
            }
        }

        /* Long press while held */
        if (pressed && press_start_ms >= 0 && !long_fired) {
            int64_t held = now_ms - press_start_ms;
            if ((uint32_t)held >= s_cfg.long_press_ms) {
                long_fired = true;
                pending_single = false;
                if (s_cb) s_cb(PWR_KEY_EVT_LONG_PRESS, s_user);
            }
        }

        /* Release edge */
        if (!debounced && pressed) {
            pressed = false;
            if (press_start_ms >= 0 && !long_fired) {
                /* Wait to distinguish single vs double press */
                pending_single = true;
                release_ms = now_ms;
            }
            press_start_ms = -1;
        }

        /* Single press timeout */
        if (pending_single && (now_ms - release_ms) >= (int64_t)s_cfg.double_press_ms) {
            pending_single = false;
            if (s_cb) s_cb(PWR_KEY_EVT_SHORT_PRESS, s_user);
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
    if (s_cfg.long_press_ms == 0) s_cfg.long_press_ms = 3000;
    if (s_cfg.double_press_ms == 0) s_cfg.double_press_ms = 350;

    gpio_config_t in = {
        .pin_bit_mask = 1ULL << s_cfg.key_gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = s_cfg.key_active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = s_cfg.key_active_low ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&in));

    gpio_config_t out = {
        .pin_bit_mask = 1ULL << s_cfg.hold_gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&out));

    xTaskCreate(pwr_key_task, "pwr_key", 4096, NULL, 10, NULL);
    return ESP_OK;
}
