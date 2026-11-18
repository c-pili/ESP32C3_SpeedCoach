#include "flash_storage.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"

static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

esp_err_t flash_storage_mount(activity_storage_t *storage)
{
    if (!storage) return ESP_ERR_INVALID_ARG;
    esp_vfs_fat_mount_config_t conf = {
        .max_files = 8,
        .format_if_mount_failed = true,
        .allocation_unit_size = 1024,
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(STORAGE_MOUNT, "storage", &conf, &s_wl_handle);
    if (err != ESP_OK) return err;
    storage->mounted = true;
    storage->mount_point = STORAGE_MOUNT;
    return ESP_OK;
}

esp_err_t flash_storage_format(void)
{
    ESP_LOGI("flash_storage", "Formatting storage partition...");
    // Unmount first if mounted
    if (s_wl_handle != WL_INVALID_HANDLE) {
        esp_vfs_fat_spiflash_unmount_rw_wl(STORAGE_MOUNT, s_wl_handle);
        s_wl_handle = WL_INVALID_HANDLE;
    }
    // Format the partition
    esp_err_t err = esp_vfs_fat_spiflash_format_rw_wl("storage", NULL);
    if (err != ESP_OK) {
        ESP_LOGE("flash_storage", "Format failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI("flash_storage", "Storage formatted successfully");
    return ESP_OK;
}
