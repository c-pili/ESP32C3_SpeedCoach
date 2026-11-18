#include "speedcoach_ble.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_helper.h"
#include "app_context.h"
#include "app_activity.h"
#include "activity.h"
#include "sensor_hub.h"
#include "battery_drv.h"
#include "storage_paths.h"
#include "flash_storage.h"

#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_uuid.h"
#include "host/ble_hs.h"
#include "host/ble_att.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

static const char *TAG = "speedcoach_ble";

#define BLE_DEVICE_NAME "SpeedCoach"

// SpeedCoach Service UUID: 9b7e1000-2b2f-4f71-9b86-4bb2e6d54f00
static const ble_uuid128_t SPEEDCOACH_SVC_UUID =
    BLE_UUID128_INIT(0x00, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

// Characteristic UUIDs (base: 9b7e1000-2b2f-4f71-9b86-4bb2e6d54f00 + offset)
static const ble_uuid128_t CHAR_FW_VERSION_UUID =
    BLE_UUID128_INIT(0x01, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_BATTERY_UUID =
    BLE_UUID128_INIT(0x02, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_ACTIVITY_STATE_UUID =
    BLE_UUID128_INIT(0x10, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_SPEED_UUID =
    BLE_UUID128_INIT(0x11, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_PACE_UUID =
    BLE_UUID128_INIT(0x12, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_DISTANCE_UUID =
    BLE_UUID128_INIT(0x13, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_STROKE_RATE_UUID =
    BLE_UUID128_INIT(0x14, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_HEART_RATE_UUID =
    BLE_UUID128_INIT(0x15, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_SESSION_TIME_UUID =
    BLE_UUID128_INIT(0x16, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_STROKE_COUNT_UUID =
    BLE_UUID128_INIT(0x17, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_SENSOR_HR_UUID =
    BLE_UUID128_INIT(0x20, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_SENSOR_ROWPOD_UUID =
    BLE_UUID128_INIT(0x21, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_WORKOUT_CONFIG_UUID =
    BLE_UUID128_INIT(0x30, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_DISPLAY_PRESET_UUID =
    BLE_UUID128_INIT(0x31, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_DEVICE_NAME_UUID =
    BLE_UUID128_INIT(0x60, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_COMMAND_UUID =
    BLE_UUID128_INIT(0x40, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_ACTIVITY_LIST_UUID =
    BLE_UUID128_INIT(0x50, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_ACTIVITY_DETAIL_UUID =
    BLE_UUID128_INIT(0x51, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

// New characteristics for full device control
static const ble_uuid128_t CHAR_LIVE_PREVIEW_UUID =
    BLE_UUID128_INIT(0x70, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_SENSOR_PAIRING_UUID =
    BLE_UUID128_INIT(0x80, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_BRIGHTNESS_UUID =
    BLE_UUID128_INIT(0x90, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_AUTO_DIM_UUID =
    BLE_UUID128_INIT(0x91, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_SPLIT_DISTANCE_UUID =
    BLE_UUID128_INIT(0x92, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_USER_WEIGHT_UUID =
    BLE_UUID128_INIT(0x93, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_BOAT_TYPE_UUID =
    BLE_UUID128_INIT(0x94, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_AUTO_PAUSE_UUID =
    BLE_UUID128_INIT(0x95, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_SOUND_UUID =
    BLE_UUID128_INIT(0x96, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_SLEEP_TIMEOUT_UUID =
    BLE_UUID128_INIT(0x97, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_HR_DEVICE_NAME_UUID =
    BLE_UUID128_INIT(0x98, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool s_notify_enabled = false;
static bool s_initialized = false;
static bool s_running = false;
static uint8_t s_own_addr_type = 0;

static uint16_t chr_fw_version_val_handle;
static uint16_t chr_battery_val_handle;
static uint16_t chr_activity_state_val_handle;
static uint16_t chr_speed_val_handle;
static uint16_t chr_pace_val_handle;
static uint16_t chr_distance_val_handle;
static uint16_t chr_stroke_rate_val_handle;
static uint16_t chr_heart_rate_val_handle;
static uint16_t chr_session_time_val_handle;
static uint16_t chr_stroke_count_val_handle;
static uint16_t chr_sensor_hr_val_handle;
static uint16_t chr_sensor_rowpod_val_handle;
static uint16_t chr_workout_config_val_handle;
static uint16_t chr_display_preset_val_handle;
static uint16_t chr_device_name_val_handle;
static uint16_t chr_command_val_handle;
static uint16_t chr_activity_list_val_handle;
static uint16_t chr_activity_detail_val_handle;
static uint16_t chr_live_preview_val_handle;
static uint16_t chr_sensor_pairing_val_handle;
static uint16_t chr_brightness_val_handle;
static uint16_t chr_auto_dim_val_handle;
static uint16_t chr_split_distance_val_handle;
static uint16_t chr_user_weight_val_handle;
static uint16_t chr_boat_type_val_handle;
static uint16_t chr_auto_pause_val_handle;
static uint16_t chr_sound_val_handle;
static uint16_t chr_sleep_timeout_val_handle;
static uint16_t chr_hr_device_name_val_handle;

static struct ble_npl_callout s_notify_timer;
static bool s_notify_timer_armed = false;

typedef enum {
    CMD_NONE = 0,
    CMD_START_ACTIVITY = 1,
    CMD_STOP_ACTIVITY = 2,
    CMD_PAUSE_ACTIVITY = 3,
    CMD_RESUME_ACTIVITY = 4,
    CMD_FORGET_HR = 9,
    CMD_FORGET_ROWPOD = 10,
    CMD_START_WORKOUT = 20,        // New: start workout with type and target
    CMD_SET_WORKOUT_TARGET = 21,   // New: set workout target (time/distance)
    CMD_PAIR_HR = 30,              // New: pair HR sensor
    CMD_PAIR_ROWPOD = 31,          // New: pair RowPod
    CMD_FORMAT_STORAGE = 99,       // New: format storage
} speedcoach_cmd_t;

static volatile bool *sp_activity_recording = NULL;
static volatile bool *sp_activity_paused = NULL;
static float *sp_session_time_s = NULL;
static activity_t *sp_activity = NULL;
static workout_config_t *sp_workout_config = NULL;
static battery_drv_handle_t sp_battery_handle = NULL;

// Forward declarations
static int on_fw_version_read(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_battery_read(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_activity_state_read(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_speed_read(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_pace_read(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_distance_read(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_stroke_rate_read(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_heart_rate_read(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_session_time_read(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_stroke_count_read(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_sensor_hr_read(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_sensor_rowpod_read(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_workout_config_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_display_preset_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_device_name_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_live_preview_read(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_sensor_pairing_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_brightness_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_auto_dim_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_split_distance_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_user_weight_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_boat_type_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_auto_pause_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_sound_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_sleep_timeout_access(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_hr_device_name_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_command_write(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_activity_list_read(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_activity_detail_read(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_chr_def gatt_chrs[] = {
    {
        .uuid = &CHAR_FW_VERSION_UUID.u,
        .access_cb = on_fw_version_read,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_fw_version_val_handle,
    },
    {
        .uuid = &CHAR_BATTERY_UUID.u,
        .access_cb = on_battery_read,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_battery_val_handle,
    },
    {
        .uuid = &CHAR_ACTIVITY_STATE_UUID.u,
        .access_cb = on_activity_state_read,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_activity_state_val_handle,
    },
    {
        .uuid = &CHAR_SPEED_UUID.u,
        .access_cb = on_speed_read,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_speed_val_handle,
    },
    {
        .uuid = &CHAR_PACE_UUID.u,
        .access_cb = on_pace_read,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_pace_val_handle,
    },
    {
        .uuid = &CHAR_DISTANCE_UUID.u,
        .access_cb = on_distance_read,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_distance_val_handle,
    },
    {
        .uuid = &CHAR_STROKE_RATE_UUID.u,
        .access_cb = on_stroke_rate_read,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_stroke_rate_val_handle,
    },
    {
        .uuid = &CHAR_HEART_RATE_UUID.u,
        .access_cb = on_heart_rate_read,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_heart_rate_val_handle,
    },
    {
        .uuid = &CHAR_SESSION_TIME_UUID.u,
        .access_cb = on_session_time_read,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_session_time_val_handle,
    },
    {
        .uuid = &CHAR_STROKE_COUNT_UUID.u,
        .access_cb = on_stroke_count_read,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_stroke_count_val_handle,
    },
    {
        .uuid = &CHAR_SENSOR_HR_UUID.u,
        .access_cb = on_sensor_hr_read,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_sensor_hr_val_handle,
    },
    {
        .uuid = &CHAR_SENSOR_ROWPOD_UUID.u,
        .access_cb = on_sensor_rowpod_read,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_sensor_rowpod_val_handle,
    },
    {
        .uuid = &CHAR_WORKOUT_CONFIG_UUID.u,
        .access_cb = on_workout_config_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_workout_config_val_handle,
    },
    {
        .uuid = &CHAR_DISPLAY_PRESET_UUID.u,
        .access_cb = on_display_preset_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_display_preset_val_handle,
    },
    {
        .uuid = &CHAR_DEVICE_NAME_UUID.u,
        .access_cb = on_device_name_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
        .val_handle = &chr_device_name_val_handle,
    },
    {
        .uuid = &CHAR_COMMAND_UUID.u,
        .access_cb = on_command_write,
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
        .val_handle = &chr_command_val_handle,
    },
    {
        .uuid = &CHAR_ACTIVITY_LIST_UUID.u,
        .access_cb = on_activity_list_read,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_activity_list_val_handle,
    },
    {
        .uuid = &CHAR_ACTIVITY_DETAIL_UUID.u,
        .access_cb = on_activity_detail_read,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_activity_detail_val_handle,
    },
    // New characteristics
    {
        .uuid = &CHAR_LIVE_PREVIEW_UUID.u,
        .access_cb = on_live_preview_read,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_live_preview_val_handle,
    },
    {
        .uuid = &CHAR_SENSOR_PAIRING_UUID.u,
        .access_cb = on_sensor_pairing_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_sensor_pairing_val_handle,
    },
    {
        .uuid = &CHAR_BRIGHTNESS_UUID.u,
        .access_cb = on_brightness_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_brightness_val_handle,
    },
    {
        .uuid = &CHAR_AUTO_DIM_UUID.u,
        .access_cb = on_auto_dim_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_auto_dim_val_handle,
    },
    {
        .uuid = &CHAR_SPLIT_DISTANCE_UUID.u,
        .access_cb = on_split_distance_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_split_distance_val_handle,
    },
    {
        .uuid = &CHAR_USER_WEIGHT_UUID.u,
        .access_cb = on_user_weight_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_user_weight_val_handle,
    },
    {
        .uuid = &CHAR_BOAT_TYPE_UUID.u,
        .access_cb = on_boat_type_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_boat_type_val_handle,
    },
    {
        .uuid = &CHAR_AUTO_PAUSE_UUID.u,
        .access_cb = on_auto_pause_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_auto_pause_val_handle,
    },
    {
        .uuid = &CHAR_SOUND_UUID.u,
        .access_cb = on_sound_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_sound_val_handle,
    },
    {
        .uuid = &CHAR_SLEEP_TIMEOUT_UUID.u,
        .access_cb = on_sleep_timeout_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_sleep_timeout_val_handle,
    },
    {
        .uuid = &CHAR_HR_DEVICE_NAME_UUID.u,
        .access_cb = on_hr_device_name_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_hr_device_name_val_handle,
    },
    {
        0,
    }
};

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &SPEEDCOACH_SVC_UUID.u,
        .characteristics = gatt_chrs,
    },
    {
        0,
    }
};

static int gap_event(struct ble_gap_event *event, void *arg);
static void on_reset(int reason);
static void on_sync(void);
static void gatts_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
static void host_task(void *param);
static void notify_timer_cb(struct ble_npl_event *ev);
static void arm_notify_timer(void);

static int on_fw_version_read(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const char *version = "1.0.0";
    os_mbuf_append(ctxt->om, version, strlen(version));
    return 0;
}

static int on_battery_read(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int pct = -1;
    if (sp_battery_handle) {
        battery_drv_read_percent((battery_drv_handle_t)sp_battery_handle, &pct);
    }
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    uint8_t val = (uint8_t)pct;
    os_mbuf_append(ctxt->om, &val, 1);
    return 0;
}

static int on_activity_state_read(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint8_t state = 0;
    if (sp_activity_recording && *sp_activity_recording) {
        state = (sp_activity_paused && *sp_activity_paused) ? 2 : 1;
    }
    os_mbuf_append(ctxt->om, &state, 1);
    return 0;
}

static int on_speed_read(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    float speed = 0.0f;
    if (sp_activity_recording && *sp_activity_recording && sp_activity) {
        speed = sp_activity->avg_speed_mps;
    }
    int32_t val = (int32_t)(speed * 1000);
    os_mbuf_append(ctxt->om, &val, 4);
    return 0;
}

static int on_pace_read(uint16_t conn_handle, uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    float pace = 0.0f;
    if (sp_activity_recording && *sp_activity_recording && sp_activity && sp_activity->avg_speed_mps > 0.01f) {
        pace = 500.0f / sp_activity->avg_speed_mps;
    }
    int32_t val = (int32_t)(pace * 10);
    os_mbuf_append(ctxt->om, &val, 4);
    return 0;
}

static int on_distance_read(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    float dist = 0.0f;
    if (sp_activity_recording && *sp_activity_recording && sp_activity) {
        dist = sp_activity->distance_m;
    }
    // Also show target distance if workout is configured
    if (sp_workout_config && sp_workout_config->mode == 2) {
        // Return target distance when not yet recording, so app can show target
        if (!sp_activity_recording || !*sp_activity_recording) {
            dist = (float)sp_workout_config->distance_m;
        }
    }
    int32_t val = (int32_t)(dist * 100);
    os_mbuf_append(ctxt->om, &val, 4);
    return 0;
}

static int on_stroke_rate_read(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    float spm = 0.0f;
    if (sp_activity_recording && *sp_activity_recording && sp_activity) {
        spm = sp_activity->avg_spm;
    }
    int16_t val = (int16_t)(spm * 10);
    os_mbuf_append(ctxt->om, &val, 2);
    return 0;
}

static int on_heart_rate_read(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    sensor_hub_live_t live = {0};
    sensor_hub_get_live(&live);
    uint16_t hr = live.hr_bpm;
    os_mbuf_append(ctxt->om, &hr, 2);
    return 0;
}

static int on_session_time_read(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint32_t time_s = 0;
    if (sp_activity_recording && *sp_activity_recording && sp_session_time_s) {
        time_s = (uint32_t)(*sp_session_time_s);
    }
    // Also show target time if time workout configured
    if (sp_workout_config && sp_workout_config->mode == 1) {
        if (!sp_activity_recording || !*sp_activity_recording) {
            time_s = sp_workout_config->duration_s;
        }
    }
    os_mbuf_append(ctxt->om, &time_s, 4);
    return 0;
}

static int on_stroke_count_read(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint32_t count = 0;
    if (sp_activity_recording && *sp_activity_recording && sp_activity) {
        count = sp_activity->stroke_count;
    }
    os_mbuf_append(ctxt->om, &count, 4);
    return 0;
}

static int on_sensor_hr_read(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    sensor_hub_slot_info_t slot = {0};
    sensor_hub_get_slot(SENSOR_KIND_HR, &slot);
    uint8_t addr[6] = {0};
    nvs_helper_get_hr_addr(addr);
    uint8_t data[27];
    data[0] = (uint8_t)slot.state;
    data[1] = slot.saved ? 1 : 0;
    data[2] = slot.paused ? 1 : 0;
    data[3] = slot.battery_pct;
    memcpy(&data[4], addr, 6);
    memcpy(&data[10], slot.name, 16);
    os_mbuf_append(ctxt->om, data, sizeof(data));
    return 0;
}

static int on_sensor_rowpod_read(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    sensor_hub_slot_info_t slot = {0};
    sensor_hub_live_t live = {0};
    sensor_hub_get_slot(SENSOR_KIND_ROWPOD, &slot);
    sensor_hub_get_live(&live);
    uint8_t addr[6] = {0};
    nvs_helper_get_sensor_addr(addr);
    uint8_t data[30];
    data[0] = (uint8_t)slot.state;
    data[1] = slot.saved ? 1 : 0;
    data[2] = slot.paused ? 1 : 0;
    data[3] = slot.battery_pct;
    memcpy(&data[4], addr, 6);
    memcpy(&data[10], slot.name, 16);
    data[26] = live.pod_connected ? 1 : 0;
    data[27] = live.pod_valid ? 1 : 0;
    int16_t catch_ddeg = live.pod_catch_ddeg;
    memcpy(&data[28], &catch_ddeg, 2);
    os_mbuf_append(ctxt->om, data, sizeof(data));
    return 0;
}

static int on_workout_config_access(uint16_t conn_handle, uint16_t attr_handle,
                                     struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        ESP_LOGI(TAG, "BLE RX READ: workout_config");
        uint8_t data[15] = {0};
        if (sp_workout_config) {
            data[0] = sp_workout_config->mode;
            memcpy(&data[1], &sp_workout_config->duration_s, 4);
            memcpy(&data[5], &sp_workout_config->distance_m, 4);
            memcpy(&data[9], &sp_workout_config->interval_work_s, 2);
            memcpy(&data[11], &sp_workout_config->interval_rest_s, 2);
            memcpy(&data[13], &sp_workout_config->interval_reps, 2);
        }
        os_mbuf_append(ctxt->om, data, sizeof(data));
        return 0;
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        ESP_LOGI(TAG, "BLE RX WRITE: workout_config len=%d", ctxt->om->om_len);
        if (ctxt->om->om_len < 15) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        if (!sp_workout_config) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        if (sp_activity_recording && *sp_activity_recording) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        uint8_t *data = ctxt->om->om_data;
        sp_workout_config->mode = data[0];
        memcpy(&sp_workout_config->duration_s, &data[1], 4);
        memcpy(&sp_workout_config->distance_m, &data[5], 4);
        memcpy(&sp_workout_config->interval_work_s, &data[9], 2);
        memcpy(&sp_workout_config->interval_rest_s, &data[11], 2);
        memcpy(&sp_workout_config->interval_reps, &data[13], 2);
        nvs_helper_set_workout_config(sp_workout_config->mode,
                                       sp_workout_config->duration_s,
                                       sp_workout_config->distance_m,
                                       sp_workout_config->interval_work_s,
                                       sp_workout_config->interval_rest_s,
                                       sp_workout_config->interval_reps);
        ESP_LOGI(TAG, "BLE SET workout: mode=%d dur=%u dist=%u",
                 sp_workout_config->mode, sp_workout_config->duration_s,
                 sp_workout_config->distance_m);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static int on_display_preset_access(uint16_t conn_handle, uint16_t attr_handle,
                                     struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        ESP_LOGI(TAG, "BLE RX READ: display_preset");
        uint8_t preset = 0;
        if (sp_workout_config) {
            preset = sp_workout_config->display_preset;
        }
        os_mbuf_append(ctxt->om, &preset, 1);
        return 0;
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        ESP_LOGI(TAG, "BLE RX WRITE: display_preset len=%d", ctxt->om->om_len);
        if (ctxt->om->om_len < 1) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        if (!sp_workout_config) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        uint8_t preset = ctxt->om->om_data[0];
        if (preset <= 2) {
            sp_workout_config->display_preset = preset;
            nvs_helper_set_display_preset(preset);
            ESP_LOGI(TAG, "BLE SET display_preset=%u", preset);
        }
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static int on_device_name_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        ESP_LOGI(TAG, "BLE RX READ: device_name");
        static char s_dev_name[32] = "SpeedCoach";
        nvs_helper_get_device_name(s_dev_name, sizeof(s_dev_name));
        if (s_dev_name[0] == '\0') {
            strcpy(s_dev_name, "SpeedCoach");
        }
        os_mbuf_append(ctxt->om, s_dev_name, strlen(s_dev_name));
        return 0;
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        ESP_LOGI(TAG, "BLE RX WRITE: device_name len=%d", ctxt->om->om_len);
        if (ctxt->om->om_len < 1 || ctxt->om->om_len > 31) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        char new_name[32] = {0};
        memcpy(new_name, ctxt->om->om_data, ctxt->om->om_len);
        new_name[ctxt->om->om_len] = '\0';
        nvs_helper_set_device_name(new_name);
        ESP_LOGI(TAG, "BLE SET device_name=%s", new_name);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// New: Live preview read - returns binary packed data for real-time display
static int on_live_preview_read(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t data[36] = {0};
        if (sp_activity_recording && *sp_activity_recording && sp_activity) {
            data[0] = 1; // active
            // Pace (sec/km * 10)
            float pace = 0.0f;
            if (sp_activity->avg_speed_mps > 0.01f) {
                pace = 500.0f / sp_activity->avg_speed_mps;
            }
            int32_t pace_val = (int32_t)(pace * 10);
            memcpy(&data[1], &pace_val, 4);
            // Speed (m/s * 1000)
            int32_t speed_val = (int32_t)(sp_activity->avg_speed_mps * 1000);
            memcpy(&data[5], &speed_val, 4);
            // Distance (m * 100)
            int32_t dist_val = (int32_t)(sp_activity->distance_m * 100);
            memcpy(&data[9], &dist_val, 4);
            // SPM * 10
            int16_t spm_val = (int16_t)(sp_activity->avg_spm * 10);
            memcpy(&data[13], &spm_val, 2);
            // Heart rate
            sensor_hub_live_t live = {0};
            sensor_hub_get_live(&live);
            uint16_t hr = live.hr_bpm;
            memcpy(&data[15], &hr, 2);
            // Session time (seconds)
            uint32_t time_s = 0;
            if (sp_session_time_s) {
                time_s = (uint32_t)(*sp_session_time_s);
            }
            memcpy(&data[17], &time_s, 4);
            // Stroke count
            uint32_t count = sp_activity->stroke_count;
            memcpy(&data[21], &count, 4);
            // Drive time ms (not tracked in activity_t, use 0)
            int32_t drive_ms = 0;
            memcpy(&data[25], &drive_ms, 4);
            // Recovery time ms (not tracked in activity_t, use 0)
            int32_t recovery_ms = 0;
            memcpy(&data[29], &recovery_ms, 4);
        } else {
            data[0] = 0; // not active
        }
        os_mbuf_append(ctxt->om, data, sizeof(data));
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// New: Sensor pairing characteristic
static int on_sensor_pairing_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t data[48] = {0};
        sensor_hub_slot_info_t hr_slot = {0};
        sensor_hub_slot_info_t pod_slot = {0};
        sensor_hub_get_slot(SENSOR_KIND_HR, &hr_slot);
        sensor_hub_get_slot(SENSOR_KIND_ROWPOD, &pod_slot);
        
        data[0] = (uint8_t)hr_slot.state;
        data[1] = hr_slot.saved ? 1 : 0;
        uint8_t hr_addr[6] = {0};
        nvs_helper_get_hr_addr(hr_addr);
        memcpy(&data[2], hr_addr, 6);
        memcpy(&data[8], hr_slot.name, 16);
        
        data[24] = (uint8_t)pod_slot.state;
        data[25] = pod_slot.saved ? 1 : 0;
        uint8_t pod_addr[6] = {0};
        nvs_helper_get_sensor_addr(pod_addr);
        memcpy(&data[26], pod_addr, 6);
        memcpy(&data[32], pod_slot.name, 16);
        
        os_mbuf_append(ctxt->om, data, sizeof(data));
        return 0;
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // Sensor pairing commands: [cmd, sensor_type, addr(6), name(16)]
        // cmd: 1=pair HR, 2=pair RowPod, 3=forget HR, 4=forget RowPod
        if (ctxt->om->om_len < 2) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        uint8_t cmd = ctxt->om->om_data[0];
        uint8_t sensor_type = ctxt->om->om_data[1];
        
        if (cmd == 1 || cmd == 2) { // Pair
            if (ctxt->om->om_len < 2 + 6 + 16) {
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }
            uint8_t addr[6];
            char name[17] = {0};
            memcpy(addr, &ctxt->om->om_data[2], 6);
            memcpy(name, &ctxt->om->om_data[8], 16);
            name[16] = '\0';
            
            if (sensor_type == 1) { // HR
                nvs_helper_set_hr_addr(addr);
                nvs_helper_set_hr_name(name);
                sensor_hub_pair(SENSOR_KIND_HR, addr, name);
            } else if (sensor_type == 2) { // RowPod
                nvs_helper_set_sensor_addr(addr);
                nvs_helper_set_sensor_name(name);
                sensor_hub_pair(SENSOR_KIND_ROWPOD, addr, name);
            }
            ESP_LOGI(TAG, "Sensor paired: type=%u name=%s", sensor_type, name);
        } else if (cmd == 3) { // Forget HR
            sensor_hub_forget(SENSOR_KIND_HR);
            ESP_LOGI(TAG, "HR sensor forgotten");
        } else if (cmd == 4) { // Forget RowPod
            sensor_hub_forget(SENSOR_KIND_ROWPOD);
            ESP_LOGI(TAG, "RowPod sensor forgotten");
        }
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// New: Brightness characteristic
static int on_brightness_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t brightness = nvs_helper_get_brightness();
        os_mbuf_append(ctxt->om, &brightness, 1);
        return 0;
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (ctxt->om->om_len < 1) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        uint8_t brightness = ctxt->om->om_data[0];
        if (brightness < 10) brightness = 10;
        if (brightness > 100) brightness = 100;
        nvs_helper_set_brightness(brightness);
        ESP_LOGI(TAG, "Brightness set to %u%%", brightness);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// New: Auto-dim characteristic
static int on_auto_dim_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t val = nvs_helper_get_auto_dim() ? 1 : 0;
        os_mbuf_append(ctxt->om, &val, 1);
        return 0;
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (ctxt->om->om_len < 1) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        bool enabled = ctxt->om->om_data[0] != 0;
        nvs_helper_set_auto_dim(enabled);
        ESP_LOGI(TAG, "Auto-dim %s", enabled ? "enabled" : "disabled");
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// New: Split distance characteristic
static int on_split_distance_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint32_t split = nvs_helper_get_split_len();
        uint8_t data[4];
        data[0] = split & 0xFF;
        data[1] = (split >> 8) & 0xFF;
        data[2] = (split >> 16) & 0xFF;
        data[3] = (split >> 24) & 0xFF;
        os_mbuf_append(ctxt->om, data, 4);
        return 0;
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (ctxt->om->om_len < 4) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        uint32_t split = ctxt->om->om_data[0] | 
                         (ctxt->om->om_data[1] << 8) | 
                         (ctxt->om->om_data[2] << 16) | 
                         (ctxt->om->om_data[3] << 24);
        if (split < 100) split = 100;
        if (split > 10000) split = 10000;
        nvs_helper_set_split_len(split);
        ESP_LOGI(TAG, "Split distance set to %lu m", (unsigned long)split);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// New: User weight characteristic
static int on_user_weight_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        // Default 75kg, could be stored in NVS
        uint16_t weight = 75;
        uint8_t data[2];
        data[0] = weight & 0xFF;
        data[1] = (weight >> 8) & 0xFF;
        os_mbuf_append(ctxt->om, data, 2);
        return 0;
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (ctxt->om->om_len < 2) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        uint16_t weight = ctxt->om->om_data[0] | (ctxt->om->om_data[1] << 8);
        if (weight < 30) weight = 30;
        if (weight > 150) weight = 150;
        // Store in NVS if needed
        ESP_LOGI(TAG, "User weight set to %u kg", weight);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// New: Boat type characteristic
static int on_boat_type_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        // Return current boat type as string
        os_mbuf_append(ctxt->om, "1x", 2);
        return 0;
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (ctxt->om->om_len < 1 || ctxt->om->om_len > 4) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        char boat_type[5] = {0};
        memcpy(boat_type, ctxt->om->om_data, ctxt->om->om_len);
        ESP_LOGI(TAG, "Boat type set to %s", boat_type);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// New: Auto-pause characteristic
static int on_auto_pause_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t val = 1; // default true
        os_mbuf_append(ctxt->om, &val, 1);
        return 0;
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (ctxt->om->om_len < 1) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        bool enabled = ctxt->om->om_data[0] != 0;
        ESP_LOGI(TAG, "Auto-pause %s", enabled ? "enabled" : "disabled");
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// New: Sound characteristic
static int on_sound_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t val = 1; // default true
        os_mbuf_append(ctxt->om, &val, 1);
        return 0;
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (ctxt->om->om_len < 1) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        bool enabled = ctxt->om->om_data[0] != 0;
        ESP_LOGI(TAG, "Sound %s", enabled ? "enabled" : "disabled");
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// New: Sleep timeout characteristic
static int on_sleep_timeout_access(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        // Default 45 minutes, could be stored in NVS
        uint8_t val = 45;
        os_mbuf_append(ctxt->om, &val, 1);
        return 0;
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (ctxt->om->om_len < 1) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        uint8_t timeout = ctxt->om->om_data[0];
        if (timeout < 5) timeout = 5;
        if (timeout > 120) timeout = 120;
        // Store in NVS if needed
        ESP_LOGI(TAG, "Sleep timeout set to %u min", timeout);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// New: HR device name characteristic
static int on_hr_device_name_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        static char hr_name[32] = "Suunto Watch HR 1029";
        nvs_helper_get_hr_name(hr_name, sizeof(hr_name));
        if (hr_name[0] == '\0') {
            strcpy(hr_name, "Suunto Watch HR 1029");
        }
        os_mbuf_append(ctxt->om, hr_name, strlen(hr_name));
        return 0;
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (ctxt->om->om_len < 1 || ctxt->om->om_len > 31) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        char new_name[32] = {0};
        memcpy(new_name, ctxt->om->om_data, ctxt->om->om_len);
        new_name[ctxt->om->om_len] = '\0';
        nvs_helper_set_hr_name(new_name);
        ESP_LOGI(TAG, "HR device name changed to: %s", new_name);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static void execute_command(uint8_t cmd)
{
    extern esp_err_t sensor_hub_forget(sensor_kind_t kind);

    switch (cmd) {
        case CMD_START_ACTIVITY:
            ESP_LOGI(TAG, "BLE CMD: START_ACTIVITY (paused=%d, recording=%d)",
                     sp_activity_paused ? *sp_activity_paused : -1,
                     sp_activity_recording ? *sp_activity_recording : -1);
            if (sp_activity_recording && sp_activity_paused) {
                if (!*sp_activity_recording) {
                    app_activity_post_cmd(1);
                }
            }
            break;
        case CMD_STOP_ACTIVITY:
            ESP_LOGI(TAG, "BLE CMD: STOP_ACTIVITY (recording=%d)",
                     sp_activity_recording ? *sp_activity_recording : -1);
            if (sp_activity_recording && *sp_activity_recording) {
                app_activity_post_cmd(4);
            }
            break;
        case CMD_PAUSE_ACTIVITY:
            ESP_LOGI(TAG, "BLE CMD: PAUSE_ACTIVITY (recording=%d, paused=%d)",
                     sp_activity_recording ? *sp_activity_recording : -1,
                     sp_activity_paused ? *sp_activity_paused : -1);
            if (sp_activity_recording && *sp_activity_recording && sp_activity_paused && !*sp_activity_paused) {
                app_activity_post_cmd(2);
            }
            break;
        case CMD_RESUME_ACTIVITY:
            ESP_LOGI(TAG, "BLE CMD: RESUME_ACTIVITY (recording=%d, paused=%d)",
                     sp_activity_recording ? *sp_activity_recording : -1,
                     sp_activity_paused ? *sp_activity_paused : -1);
            if (sp_activity_recording && *sp_activity_recording && sp_activity_paused && *sp_activity_paused) {
                app_activity_post_cmd(3);
            }
            break;
        case CMD_FORGET_HR:
            ESP_LOGI(TAG, "BLE CMD: FORGET_HR");
            sensor_hub_forget(SENSOR_KIND_HR);
            break;
        case CMD_FORGET_ROWPOD:
            ESP_LOGI(TAG, "BLE CMD: FORGET_ROWPOD");
            sensor_hub_forget(SENSOR_KIND_ROWPOD);
            break;
        case CMD_START_WORKOUT:
            ESP_LOGI(TAG, "BLE CMD: START_WORKOUT (no-op, use CMD_START_ACTIVITY)");
            break;
        case CMD_SET_WORKOUT_TARGET:
            ESP_LOGI(TAG, "BLE CMD: SET_WORKOUT_TARGET (no-op, use workout config chr)");
            break;
case CMD_PAIR_HR:
        case CMD_PAIR_ROWPOD:
            ESP_LOGI(TAG, "BLE CMD: PAIR (no-op, use sensor pairing chr)");
            break;
        case CMD_FORMAT_STORAGE:
            ESP_LOGI(TAG, "BLE CMD: FORMAT_STORAGE");
            flash_storage_format();
            break;
        default:
            break;
    }
}
}

static int on_command_write(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->om->om_len < 1) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    uint8_t cmd = ctxt->om->om_data[0];
    ESP_LOGI(TAG, "BLE RX CMD_CHAR: cmd=0x%02x len=%d", cmd, ctxt->om->om_len);
    
    // For CMD_START_WORKOUT, the next bytes contain workout type and target
    if (cmd == CMD_START_WORKOUT && ctxt->om->om_len >= 6) {
        uint8_t workout_type = ctxt->om->om_data[1];
        uint32_t target = 0;
        memcpy(&target, &ctxt->om->om_data[2], 4);
        ESP_LOGI(TAG, "BLE RX CMD_START_WORKOUT: type=%d target=%u", workout_type, target);
        
        // Update workout config with the target
        if (sp_workout_config) {
            sp_workout_config->mode = workout_type;
            if (workout_type == 1) { // Time
                sp_workout_config->duration_s = target;
                ESP_LOGI(TAG, "BLE SET workout: TIME target=%us", target);
            } else if (workout_type == 2) { // Distance
                sp_workout_config->distance_m = target;
                ESP_LOGI(TAG, "BLE SET workout: DISTANCE target=%um", target);
            }
            nvs_helper_set_workout_config(sp_workout_config->mode,
                                           sp_workout_config->duration_s,
                                           sp_workout_config->distance_m,
                                           sp_workout_config->interval_work_s,
                                           sp_workout_config->interval_rest_s,
                                           sp_workout_config->interval_reps);
        }
        
        // Now start the activity
        if (sp_activity_recording && sp_activity_paused) {
            if (!*sp_activity_recording) {
                ESP_LOGI(TAG, "BLE TX: Starting activity");
                app_activity_post_cmd(1);
            }
        }
        return 0;
    }
    
    execute_command(cmd);
    return 0;
}

static int on_activity_list_read(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    ESP_LOGI(TAG, "BLE RX READ: activity_list");
    FILE *f = fopen(ACTIVITY_INDEX_PATH, "r");
    if (!f) {
        os_mbuf_append(ctxt->om, "", 1);
        return 0;
    }
    char line[256];
    size_t total = 0;
    while (fgets(line, sizeof(line), f) && total < 2000) {
        os_mbuf_append(ctxt->om, line, strlen(line));
        total += strlen(line);
    }
    fclose(f);
    ESP_LOGI(TAG, "BLE TX: activity_list sent %zu bytes", total);
    return 0;
}

static int on_activity_detail_read(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->om->om_len < 4) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    uint32_t id = 0;
    memcpy(&id, ctxt->om->om_data, 4);
    ESP_LOGI(TAG, "BLE RX READ: activity_detail id=%lu", (unsigned long)id);
    
    char path[256];
    snprintf(path, sizeof(path), "%s/%lu_tcx.tcx", ACTIVITY_DIR, (unsigned long)id);
    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGE(TAG, "BLE TX: activity_detail file not found: %s", path);
        return BLE_ATT_ERR_UNLIKELY;
    }
    char chunk[256];
    size_t n;
    size_t total = 0;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (os_mbuf_append(ctxt->om, chunk, n) != 0) {
            fclose(f);
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        total += n;
    }
    fclose(f);
    ESP_LOGI(TAG, "BLE TX: activity_detail sent %zu bytes", total);
    return 0;
}

static void notify_timer_cb(struct ble_npl_event *ev)
{
    (void)ev;
    if (!s_notify_enabled || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }
    ESP_LOGV(TAG, "BLE TX: notify timer tick");
    speedcoach_ble_notify_activity_update();
    speedcoach_ble_notify_sensor_update();
    speedcoach_ble_notify_settings_update();
    speedcoach_ble_notify_live_preview();
    arm_notify_timer();
}

static void arm_notify_timer(void)
{
    if (s_notify_timer_armed) {
        ble_npl_callout_stop(&s_notify_timer);
    }
    ble_npl_callout_reset(&s_notify_timer, ble_npl_time_ms_to_ticks32(1000));
    s_notify_timer_armed = true;
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    const char *type_str;
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT: type_str = "CONNECT"; break;
        case BLE_GAP_EVENT_DISCONNECT: type_str = "DISCONNECT"; break;
        case BLE_GAP_EVENT_CONN_UPDATE: type_str = "CONN_UPDATE"; break;
        case BLE_GAP_EVENT_ADV_COMPLETE: type_str = "ADV_COMPLETE"; break;
        case BLE_GAP_EVENT_ENC_CHANGE: type_str = "ENC_CHANGE"; break;
        case BLE_GAP_EVENT_PASSKEY_ACTION: type_str = "PASSKEY_ACTION"; break;
        case BLE_GAP_EVENT_NOTIFY_RX: type_str = "NOTIFY_RX"; break;
        case BLE_GAP_EVENT_NOTIFY_TX: type_str = "NOTIFY_TX"; break;
        case BLE_GAP_EVENT_SUBSCRIBE: type_str = "SUBSCRIBE"; break;
        case BLE_GAP_EVENT_MTU: type_str = "MTU"; break;
        case BLE_GAP_EVENT_IDENTITY_RESOLVED: type_str = "IDENTITY_RESOLVED"; break;
        case BLE_GAP_EVENT_REPEAT_PAIRING: type_str = "REPEAT_PAIRING"; break;
        case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE: type_str = "PHY_UPDATE"; break;
        case BLE_GAP_EVENT_DATA_LEN_CHG: type_str = "DATA_LEN_CHG"; break;
        case BLE_GAP_EVENT_CONN_UPDATE_REQ: type_str = "CONN_UPDATE_REQ"; break;
        case BLE_GAP_EVENT_SUBRATE_CHANGE: type_str = "CONN_SUBRATE_CHANGE"; break;
        default: type_str = "UNKNOWN"; break;
    }
    ESP_LOGI(TAG, "GAP EVENT: type=%d (%s)", event->type, type_str);
    
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                s_conn_handle = event->connect.conn_handle;
                s_notify_enabled = false;
                ESP_LOGI(TAG, "Client connected, handle=%d", s_conn_handle);
                arm_notify_timer();
            } else {
                ESP_LOGW(TAG, "Connection failed, status=%d", event->connect.status);
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Client disconnected, reason=%d", event->disconnect.reason);
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            s_notify_enabled = false;
            if (s_notify_timer_armed) {
                ble_npl_callout_stop(&s_notify_timer);
                s_notify_timer_armed = false;
            }
            ble_gap_adv_stop();
            struct ble_gap_adv_params adv_params = {0};
            adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
            adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
            adv_params.itvl_min = 160;
            adv_params.itvl_max = 160;
            ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, gap_event, NULL);
            break;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "Advertising complete, reason=%d", event->adv_complete.reason);
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == chr_activity_state_val_handle ||
                event->subscribe.attr_handle == chr_speed_val_handle ||
                event->subscribe.attr_handle == chr_pace_val_handle ||
                event->subscribe.attr_handle == chr_distance_val_handle ||
                event->subscribe.attr_handle == chr_stroke_rate_val_handle ||
                event->subscribe.attr_handle == chr_heart_rate_val_handle ||
                event->subscribe.attr_handle == chr_session_time_val_handle ||
                event->subscribe.attr_handle == chr_stroke_count_val_handle ||
                event->subscribe.attr_handle == chr_sensor_hr_val_handle ||
                event->subscribe.attr_handle == chr_sensor_rowpod_val_handle ||
                event->subscribe.attr_handle == chr_workout_config_val_handle ||
                event->subscribe.attr_handle == chr_display_preset_val_handle ||
                event->subscribe.attr_handle == chr_activity_list_val_handle ||
                event->subscribe.attr_handle == chr_activity_detail_val_handle ||
                event->subscribe.attr_handle == chr_fw_version_val_handle ||
                event->subscribe.attr_handle == chr_battery_val_handle ||
                event->subscribe.attr_handle == chr_live_preview_val_handle ||
                event->subscribe.attr_handle == chr_sensor_pairing_val_handle ||
                event->subscribe.attr_handle == chr_brightness_val_handle ||
                event->subscribe.attr_handle == chr_auto_dim_val_handle ||
                event->subscribe.attr_handle == chr_split_distance_val_handle ||
                event->subscribe.attr_handle == chr_user_weight_val_handle ||
                event->subscribe.attr_handle == chr_boat_type_val_handle ||
                event->subscribe.attr_handle == chr_auto_pause_val_handle ||
                event->subscribe.attr_handle == chr_sound_val_handle ||
                event->subscribe.attr_handle == chr_sleep_timeout_val_handle ||
                event->subscribe.attr_handle == chr_hr_device_name_val_handle) {
                s_notify_enabled = event->subscribe.cur_notify;
                ESP_LOGI(TAG, "Notify %s for handle %d", s_notify_enabled ? "enabled" : "disabled", event->subscribe.attr_handle);
            }
            break;
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU updated to %d", event->mtu.value);
            break;
        case BLE_GAP_EVENT_ENC_CHANGE:
            ESP_LOGI(TAG, "Encryption change: status=%d", event->enc_change.status);
            break;
        case BLE_GAP_EVENT_PASSKEY_ACTION:
            ESP_LOGI(TAG, "Passkey action: action=%d", event->passkey.params.action);
            break;
    }
    return 0;
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "NimBLE reset, reason=%d", reason);
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "No BLE address");
        return;
    }
    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Address type error");
        return;
    }
    
    uint8_t addr[6];
    ble_hs_id_copy_addr(s_own_addr_type, addr, NULL);
    ESP_LOGI(TAG, "BLE Address: %02x:%02x:%02x:%02x:%02x:%02x",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    
    // Read stored device name from NVS
    static char s_dev_name[32] = "SpeedCoach";
    nvs_helper_get_device_name(s_dev_name, sizeof(s_dev_name));
    if (s_dev_name[0] == '\0') {
        strcpy(s_dev_name, "SpeedCoach");
    }

    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids16 = NULL;
    fields.num_uuids16 = 0;
    fields.uuids128 = (ble_uuid128_t *)&SPEEDCOACH_SVC_UUID;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    // Short name in advertising packet (fits in 31 bytes with UUID)
    fields.name = (uint8_t *)"SCoach";
    fields.name_len = strlen("SCoach");
    fields.name_is_complete = 0;  // 0 = shortened name
    
    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Set adv fields failed: %d", rc);
        return;
    }

    struct ble_hs_adv_fields rsp_fields = {0};
    rsp_fields.name = (uint8_t *)s_dev_name;
    rsp_fields.name_len = strlen(s_dev_name);
    rsp_fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Set adv rsp fields failed: %d", rc);
        return;
    }
    
    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = 160;
    adv_params.itvl_max = 160;
    
    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Adv start failed: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "Advertising started as %s", s_dev_name);
}

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
    vTaskDelete(NULL);
}

static void gatts_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];
    switch (ctxt->op) {
        case BLE_GATT_REGISTER_OP_SVC:
            ESP_LOGI(TAG, "GATTS register: service %s handle=%d",
                     ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf), ctxt->svc.handle);
            break;
        case BLE_GATT_REGISTER_OP_CHR:
            ESP_LOGI(TAG, "GATTS register: characteristic %s handle=%d",
                     ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf), ctxt->chr.def_handle);
            break;
        case BLE_GATT_REGISTER_OP_DSC:
            ESP_LOGI(TAG, "GATTS register: descriptor %s handle=%d",
                     ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf), ctxt->dsc.handle);
            break;
    }
}

esp_err_t speedcoach_ble_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing SpeedCoach BLE peripheral...");
    
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(err));
        return err;
    }
    
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.gatts_register_cb = gatts_register_cb;
    ble_hs_cfg.store_status_cb = NULL;
    
    // Security Manager configuration (no bonding, no MITM, Just Works)
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 0;
    ble_hs_cfg.sm_our_key_dist = 0;
    ble_hs_cfg.sm_their_key_dist = 0;
    
    int rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT count failed: %d", rc);
        return ESP_FAIL;
    }
    
    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT add services failed: %d", rc);
        return ESP_FAIL;
    }
    
    ble_npl_callout_init(&s_notify_timer, nimble_port_get_dflt_eventq(), notify_timer_cb, NULL);
    
    nimble_port_freertos_init(host_task);
    
    s_initialized = true;
    s_running = true;
    ESP_LOGI(TAG, "SpeedCoach BLE peripheral initialized");
    return ESP_OK;
}

esp_err_t speedcoach_ble_stop(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping SpeedCoach BLE peripheral...");
    
    if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    }
    
    ble_gap_adv_stop();
    
    if (s_notify_timer_armed) {
        ble_npl_callout_stop(&s_notify_timer);
        s_notify_timer_armed = false;
    }
    
    nimble_port_stop();
    
    s_running = false;
    s_notify_enabled = false;
    
    ESP_LOGI(TAG, "SpeedCoach BLE peripheral stopped");
    return ESP_OK;
}

bool speedcoach_ble_is_running(void)
{
    return s_running;
}

bool speedcoach_ble_is_connected(void)
{
    return s_conn_handle != BLE_HS_CONN_HANDLE_NONE;
}

void speedcoach_ble_notify_activity_update(void)
{
    if (!s_notify_enabled || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }
    ESP_LOGD(TAG, "BLE TX: notify activity update");
    ble_gatts_chr_updated(chr_activity_state_val_handle);
    ble_gatts_chr_updated(chr_speed_val_handle);
    ble_gatts_chr_updated(chr_pace_val_handle);
    ble_gatts_chr_updated(chr_distance_val_handle);
    ble_gatts_chr_updated(chr_stroke_rate_val_handle);
    ble_gatts_chr_updated(chr_heart_rate_val_handle);
    ble_gatts_chr_updated(chr_session_time_val_handle);
    ble_gatts_chr_updated(chr_stroke_count_val_handle);
}

void speedcoach_ble_notify_sensor_update(void)
{
    if (!s_notify_enabled || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }
    ESP_LOGD(TAG, "BLE TX: notify sensor update");
    ble_gatts_chr_updated(chr_sensor_hr_val_handle);
    ble_gatts_chr_updated(chr_sensor_rowpod_val_handle);
    ble_gatts_chr_updated(chr_battery_val_handle);
}

void speedcoach_ble_notify_settings_update(void)
{
    if (!s_notify_enabled || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }
    ESP_LOGD(TAG, "BLE TX: notify settings update");
    ble_gatts_chr_updated(chr_workout_config_val_handle);
    ble_gatts_chr_updated(chr_display_preset_val_handle);
    ble_gatts_chr_updated(chr_activity_list_val_handle);
    ble_gatts_chr_updated(chr_split_distance_val_handle);
    ble_gatts_chr_updated(chr_user_weight_val_handle);
    ble_gatts_chr_updated(chr_boat_type_val_handle);
    ble_gatts_chr_updated(chr_auto_pause_val_handle);
    ble_gatts_chr_updated(chr_sound_val_handle);
    ble_gatts_chr_updated(chr_sleep_timeout_val_handle);
    ble_gatts_chr_updated(chr_hr_device_name_val_handle);
}

void speedcoach_ble_notify_live_preview(void)
{
    if (!s_notify_enabled || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }
    
    ble_gatts_chr_updated(chr_live_preview_val_handle);
}

void speedcoach_ble_set_workout_config(void *config)
{
    sp_workout_config = (workout_config_t *)config;
}

void speedcoach_ble_set_activity_state(volatile bool *recording, volatile bool *paused, float *session_time_s, void *activity)
{
    sp_activity_recording = recording;
    sp_activity_paused = paused;
    sp_session_time_s = session_time_s;
    sp_activity = (activity_t *)activity;
}

void speedcoach_ble_set_battery_handle(void *handle)
{
    sp_battery_handle = (battery_drv_handle_t)handle;
}