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

#ifdef __cplusplus
}
#endif