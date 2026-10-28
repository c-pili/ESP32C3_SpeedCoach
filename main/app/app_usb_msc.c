#include "app_usb_msc.h"

/* ESP32-C3 USB Serial/JTAG provides CDC and JTAG only, not a USB MSC device. */
bool app_usb_msc_is_active(void)
{
    return false;
}

esp_err_t app_usb_msc_init(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t app_usb_msc_last_error(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t app_usb_msc_request_enter(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t app_usb_msc_request_leave(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}
