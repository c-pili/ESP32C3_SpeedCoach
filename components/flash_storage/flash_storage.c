#include "flash_storage.h"
#include "esp_vfs_fat.h"

esp_err_t flash_storage_mount(activity_storage_t *storage)
{
    if (!storage) return ESP_ERR_INVALID_ARG;
    esp_vfs_fat_mount_config_t conf = {
        .max_files = 8,
        .format_if_mount_failed = true,
        .allocation_unit_size = 1024,
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(STORAGE_MOUNT, "storage", &conf, NULL);
    if (err != ESP_OK) return err;
    storage->mounted = true;
    storage->mount_point = STORAGE_MOUNT;
    return ESP_OK;
}
