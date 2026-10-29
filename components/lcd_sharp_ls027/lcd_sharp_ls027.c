#include "lcd_sharp_ls027.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <string.h>

#define LCD_BYTES_PER_LINE (LCD_H_RES / 8)
#define LCD_DRAW_LINES 16
#define SHARP_CMD_WRITE 0x01u
#define SHARP_CMD_VCOM  0x02u
#define SHARP_CMD_CLEAR 0x04u

static const char *TAG = "lcd_sharp_ls027";

static spi_device_handle_t s_spi;
static SemaphoreHandle_t s_bus_lock;
static bool s_vcom;
static bool s_initialized;
static uint8_t s_framebuffer[LCD_V_RES][LCD_BYTES_PER_LINE];
_Alignas(4) static uint8_t s_draw_buf1[LCD_H_RES * LCD_DRAW_LINES * 2];
_Alignas(4) static uint8_t s_draw_buf2[LCD_H_RES * LCD_DRAW_LINES * 2];
static uint8_t s_line_tx[1 + 1 + LCD_BYTES_PER_LINE + 2];

static esp_err_t sharp_send(const uint8_t *data, size_t len)
{
    spi_transaction_t trans = {0};
    trans.length = len * 8;
    trans.tx_buffer = data;

    gpio_set_level(CONFIG_LCD_SHARP_PIN_SCS, 1);
    esp_rom_delay_us(3); /* Sharp SCS setup time before the first SCLK edge. */
    esp_err_t err = spi_device_polling_transmit(s_spi, &trans);
    if (len == 3) {
        /* Hold mode-only and clear commands for a conservative 220 us pulse. */
        esp_rom_delay_us(220);
    }
    esp_rom_delay_us(4); /* Meet SCS hold time and 220 us data-update minimum. */
    gpio_set_level(CONFIG_LCD_SHARP_PIN_SCS, 0);
    return err;
}

static uint8_t sharp_command(uint8_t command)
{
    return (uint8_t)(command | (s_vcom ? SHARP_CMD_VCOM : 0u));
}

static esp_err_t sharp_clear(void)
{
    uint8_t tx[3] = {sharp_command(SHARP_CMD_CLEAR), 0, 0};
    return sharp_send(tx, sizeof(tx));
}

static esp_err_t sharp_write_line(uint16_t y)
{
    s_line_tx[0] = sharp_command(SHARP_CMD_WRITE);
    s_line_tx[1] = (uint8_t)(y + 1); /* Sharp line addresses are 1-based. */
    memcpy(&s_line_tx[2], s_framebuffer[y], LCD_BYTES_PER_LINE);
    s_line_tx[2 + LCD_BYTES_PER_LINE] = 0;
    s_line_tx[3 + LCD_BYTES_PER_LINE] = 0;
    return sharp_send(s_line_tx, sizeof(s_line_tx));
}

static void vcom_task(void *arg)
{
    (void)arg;
    const TickType_t period = pdMS_TO_TICKS(500);
    for (;;) {
        vTaskDelay(period);
        if (xSemaphoreTake(s_bus_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
            s_vcom = !s_vcom;
            /* Display-mode NOP carries the new VCOM phase; >13 clocks required. */
            uint8_t tx[3] = {sharp_command(0), 0, 0};
            esp_err_t err = sharp_send(tx, sizeof(tx));
            xSemaphoreGive(s_bus_lock);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "VCOM update failed: %s", esp_err_to_name(err));
            }
        }
    }
}

esp_err_t lcd_sharp_ls027_init(void)
{
    if (s_initialized) return ESP_OK;

    s_bus_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_bus_lock, ESP_ERR_NO_MEM, TAG, "mutex allocation failed");

    gpio_config_t gpio_cfg = {
        .pin_bit_mask = (1ULL << CONFIG_LCD_SHARP_PIN_SCS) |
                        (1ULL << CONFIG_LCD_SHARP_PIN_DISP),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&gpio_cfg), TAG, "GPIO setup failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(CONFIG_LCD_SHARP_PIN_SCS, 0), TAG, "SCS low failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(CONFIG_LCD_SHARP_PIN_DISP, 0), TAG, "DISP low failed");

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = CONFIG_LCD_SHARP_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = CONFIG_LCD_SHARP_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = sizeof(s_line_tx),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO),
                        TAG, "SPI bus init failed");

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = CONFIG_LCD_SHARP_SPI_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = -1, /* Sharp SCS is active high; controlled around each transaction. */
        .queue_size = 1,
        .flags = SPI_DEVICE_BIT_LSBFIRST,
    };
    ESP_RETURN_ON_ERROR(spi_bus_add_device(SPI2_HOST, &dev_cfg, &s_spi),
                        TAG, "SPI device add failed");

    memset(s_framebuffer, 0xFF, sizeof(s_framebuffer));
    vTaskDelay(pdMS_TO_TICKS(1));
    if (xSemaphoreTake(s_bus_lock, pdMS_TO_TICKS(100)) != pdTRUE) return ESP_ERR_TIMEOUT;
    esp_err_t err = sharp_clear();
    if (err == ESP_OK) err = sharp_clear();
    xSemaphoreGive(s_bus_lock);
    ESP_RETURN_ON_ERROR(err, TAG, "panel clear failed");
    vTaskDelay(pdMS_TO_TICKS(50)); /* Allow at least one frame for pixel-memory initialization. */
    ESP_RETURN_ON_ERROR(gpio_set_level(CONFIG_LCD_SHARP_PIN_DISP, 1), TAG, "DISP enable failed");

    s_initialized = true;
    if (xTaskCreate(vcom_task, "lcd_vcom", 2048, NULL, 4, NULL) != pdPASS) {
        s_initialized = false;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Sharp LS027B7DH01 initialized at %d x %d", LCD_H_RES, LCD_V_RES);
    return ESP_OK;
}

static bool rgb565_is_white(const uint8_t *pixel)
{
    uint16_t color = (uint16_t)pixel[0] | ((uint16_t)pixel[1] << 8);
    uint8_t r = (uint8_t)(((color >> 11) & 0x1F) * 255 / 31);
    uint8_t g = (uint8_t)(((color >> 5) & 0x3F) * 255 / 63);
    uint8_t b = (uint8_t)((color & 0x1F) * 255 / 31);
    uint16_t luminance = (uint16_t)(77u * r + 150u * g + 29u * b) >> 8;
    return luminance >= 128;
}

static void sharp_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    int32_t x1 = area->x1 < 0 ? 0 : area->x1;
    int32_t y1 = area->y1 < 0 ? 0 : area->y1;
    int32_t x2 = area->x2 >= LCD_H_RES ? LCD_H_RES - 1 : area->x2;
    int32_t y2 = area->y2 >= LCD_V_RES ? LCD_V_RES - 1 : area->y2;
    int32_t area_width = area->x2 - area->x1 + 1;

    if (area_width > 0 && x1 <= x2 && y1 <= y2) {
        for (int32_t y = y1; y <= y2; y++) {
            int32_t src_y = y - area->y1;
            for (int32_t x = x1; x <= x2; x++) {
                int32_t src_x = x - area->x1;
                const uint8_t *pixel = &px_map[((src_y * area_width) + src_x) * 2];
                uint8_t mask = (uint8_t)(1u << (x & 7)); /* LSB-first pixels on Sharp panel. */
                uint8_t *dst = &s_framebuffer[y][x >> 3];
                if (rgb565_is_white(pixel)) *dst |= mask;
                else *dst &= (uint8_t)~mask;
            }
        }

        if (xSemaphoreTake(s_bus_lock, portMAX_DELAY) == pdTRUE) {
            for (int32_t y = y1; y <= y2; y++) {
                esp_err_t err = sharp_write_line((uint16_t)y);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "line %ld write failed: %s", (long)y, esp_err_to_name(err));
                    break;
                }
            }
            xSemaphoreGive(s_bus_lock);
        }
    }
    lv_display_flush_ready(display);
}

lv_display_t *lcd_sharp_ls027_add_lvgl_display(void)
{
    lv_display_t *display = lv_display_create(LCD_H_RES, LCD_V_RES);
    if (!display) return NULL;

    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, sharp_flush_cb);
    lv_display_set_buffers(display, s_draw_buf1, s_draw_buf2, sizeof(s_draw_buf1),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    return display;
}

void lcd_sharp_ls027_set_display_enabled(bool enabled)
{
    if (s_initialized) gpio_set_level(CONFIG_LCD_SHARP_PIN_DISP, enabled ? 1 : 0);
}
