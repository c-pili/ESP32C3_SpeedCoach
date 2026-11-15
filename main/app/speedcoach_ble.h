#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the SpeedCoach BLE peripheral (GATT server).
 * Starts advertising as "SpeedCoach".
 */
esp_err_t speedcoach_ble_init(void);

/**
 * Stop the SpeedCoach BLE peripheral.
 * Stops advertising and disconnects any connected clients.
 */
esp_err_t speedcoach_ble_stop(void);

/**
 * Check if BLE peripheral is currently running.
 */
bool speedcoach_ble_is_running(void);

/**
 * Check if a client is currently connected.
 */
bool speedcoach_ble_is_connected(void);

/**
 * Notify connected client of activity data update.
 * Call this when activity metrics change.
 */
void speedcoach_ble_notify_activity_update(void);

/**
 * Notify connected client of sensor data update (HR, RowPod).
 */
void speedcoach_ble_notify_sensor_update(void);

/**
 * Notify connected client of settings change.
 */
void speedcoach_ble_notify_settings_update(void);

/**
 * Set the workout config pointer for speedcoach_ble to access.
 * Call this from main after s_workout_config is initialized.
 */
void speedcoach_ble_set_workout_config(void *config);

/**
 * Set the activity state pointers for speedcoach_ble to access.
 * Call this from main after globals are initialized.
 */
void speedcoach_ble_set_activity_state(volatile bool *recording, volatile bool *paused, float *session_time_s, void *activity);

/**
 * Set the battery driver handle for reading battery percentage.
 */
void speedcoach_ble_set_battery_handle(void *handle);

#ifdef __cplusplus
}
#endif