#include "speedcoach_ble.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_helper.h"
#include "app_context.h"
#include "activity.h"
#include "sensor_hub.h"
#include "battery_drv.h"
#include "storage_paths.h"

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

static const ble_uuid128_t CHAR_COMMAND_UUID =
    BLE_UUID128_INIT(0x40, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_ACTIVITY_LIST_UUID =
    BLE_UUID128_INIT(0x50, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
                     0x71, 0x4f, 0x2f, 0x2b, 0x00, 0x10, 0x7e, 0x9b);

static const ble_uuid128_t CHAR_ACTIVITY_DETAIL_UUID =
    BLE_UUID128_INIT(0x51, 0x4f, 0xd5, 0xe6, 0xb2, 0x4b, 0x86, 0x9b,
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
static uint16_t chr_command_val_handle;
static uint16_t chr_activity_list_val_handle;
static uint16_t chr_activity_detail_val_handle;

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
} speedcoach_cmd_t;

static volatile bool *sp_activity_recording = NULL;
static volatile bool *sp_activity_paused = NULL;
static float *sp_session_time_s = NULL;
static activity_t *sp_activity = NULL;
static workout_config_t *sp_workout_config = NULL;
static battery_drv_handle_t sp_battery_handle = NULL;

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
static int on_workout_config_read(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg);
static int on_workout_config_write(uint16_t conn_handle, uint16_t attr_handle,
                                     struct ble_gatt_access_ctxt *ctxt, void *arg) __attribute__((unused));
static int on_display_preset_write(uint16_t conn_handle, uint16_t attr_handle,
                                     struct ble_gatt_access_ctxt *ctxt, void *arg) __attribute__((unused));
static int on_display_preset_write(uint16_t conn_handle, uint16_t attr_handle,
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
        .access_cb = on_workout_config_read,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_workout_config_val_handle,
    },
    {
        .uuid = &CHAR_DISPLAY_PRESET_UUID.u,
        .access_cb = on_display_preset_read,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &chr_display_preset_val_handle,
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

static int on_workout_config_read(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
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
}

static int on_workout_config_write(uint16_t conn_handle, uint16_t attr_handle,
                                     struct ble_gatt_access_ctxt *ctxt, void *arg) __attribute__((unused))
{
    if (ctxt->om->om_len < 15) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (!sp_workout_config) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    uint8_t *data = ctxt->om->om_data;
    if (sp_activity_recording && *sp_activity_recording) {
        return BLE_ATT_ERR_UNLIKELY;
    }
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
    return 0;
}

static int on_display_preset_read(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint8_t preset = 0;
    if (sp_workout_config) {
        preset = sp_workout_config->display_preset;
    }
    os_mbuf_append(ctxt->om, &preset, 1);
    return 0;
}

static int on_display_preset_write(uint16_t conn_handle, uint16_t attr_handle,
                                     struct ble_gatt_access_ctxt *ctxt, void *arg) __attribute__((unused))
{
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
    }
    return 0;
}

static void execute_command(uint8_t cmd)
{
    extern esp_err_t app_activity_post_cmd(int cmd);
    extern esp_err_t sensor_hub_forget(sensor_kind_t kind);

    switch (cmd) {
        case CMD_START_ACTIVITY:
            if (sp_activity_recording && sp_activity_paused) {
                if (!*sp_activity_recording) {
                    app_activity_post_cmd(1);
                }
            }
            break;
        case CMD_STOP_ACTIVITY:
            if (sp_activity_recording && *sp_activity_recording) {
                app_activity_post_cmd(4);
            }
            break;
        case CMD_PAUSE_ACTIVITY:
            if (sp_activity_recording && *sp_activity_recording && sp_activity_paused && !*sp_activity_paused) {
                app_activity_post_cmd(2);
            }
            break;
        case CMD_RESUME_ACTIVITY:
            if (sp_activity_recording && *sp_activity_recording && sp_activity_paused && *sp_activity_paused) {
                app_activity_post_cmd(3);
            }
            break;
        case CMD_FORGET_HR:
            sensor_hub_forget(SENSOR_KIND_HR);
            break;
        case CMD_FORGET_ROWPOD:
            sensor_hub_forget(SENSOR_KIND_ROWPOD);
            break;
        default:
            break;
    }
}

static int on_command_write(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->om->om_len < 1) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    uint8_t cmd = ctxt->om->om_data[0];
    execute_command(cmd);
    return 0;
}

static int on_activity_list_read(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
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
    
    char path[256];
    snprintf(path, sizeof(path), "%s/%lu_tcx.tcx", ACTIVITY_DIR, (unsigned long)id);
    FILE *f = fopen(path, "r");
    if (!f) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    char chunk[256];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (os_mbuf_append(ctxt->om, chunk, n) != 0) {
            fclose(f);
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }
    }
    fclose(f);
    return 0;
}

static void notify_timer_cb(struct ble_npl_event *ev)
{
    (void)ev;
    if (!s_notify_enabled || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }
    speedcoach_ble_notify_activity_update();
    speedcoach_ble_notify_sensor_update();
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
                event->subscribe.attr_handle == chr_battery_val_handle) {
                s_notify_enabled = event->subscribe.cur_notify;
                ESP_LOGI(TAG, "Notify %s for handle %d", s_notify_enabled ? "enabled" : "disabled", event->subscribe.attr_handle);
            }
            break;
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU updated to %d", event->mtu.value);
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
    
    struct ble_hs_adv_fields fields = {0};
    fields.name = (uint8_t *)BLE_DEVICE_NAME;
    fields.name_len = strlen(BLE_DEVICE_NAME);
    fields.name_is_complete = 1;
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids16 = NULL;
    fields.num_uuids16 = 0;
    fields.uuids128 = (ble_uuid128_t *)&SPEEDCOACH_SVC_UUID;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    
    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Set adv fields failed: %d", rc);
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
    ESP_LOGI(TAG, "Advertising started as %s", BLE_DEVICE_NAME);
}

static void host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
    vTaskDelete(NULL);
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
    ble_hs_cfg.gatts_register_cb = NULL;
    ble_hs_cfg.store_status_cb = NULL;
    
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
    
    ble_gatts_chr_updated(chr_sensor_hr_val_handle);
    ble_gatts_chr_updated(chr_sensor_rowpod_val_handle);
    ble_gatts_chr_updated(chr_battery_val_handle);
}

void speedcoach_ble_notify_settings_update(void)
{
    if (!s_notify_enabled || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return;
    }
    
    ble_gatts_chr_updated(chr_workout_config_val_handle);
    ble_gatts_chr_updated(chr_display_preset_val_handle);
    ble_gatts_chr_updated(chr_activity_list_val_handle);
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