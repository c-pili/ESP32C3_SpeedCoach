#include "flash_storage.h"
#include "esp_spiffs.h"

esp_err_t flash_storage_mount(activity_storage_t *storage)
{
    if (!storage) return ESP_ERR_INVALID_ARG;
    esp_vfs_spiffs_conf_t conf = {
        .base_path = STORAGE_MOUNT, .partition_label = "storage",
        .max_files = 8, .format_if_mount_failed = false
    };
    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) return err;
    storage->mounted = true;
    storage->mount_point = STORAGE_MOUNT;
    return ESP_OK;
}
