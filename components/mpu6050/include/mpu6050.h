#pragma once

#include "esp_err.h"
#include "i2c_helper.h"

#define MPU6050_I2C_ADDR 0x68

typedef struct {
    i2c_master_dev_handle_t dev;
    float accel_scale;
    float gyro_scale;
} mpu6050_handle_t;

esp_err_t mpu6050_init(mpu6050_handle_t *imu, i2c_helper_t *bus, uint8_t addr_7bit);
esp_err_t mpu6050_read_accel(mpu6050_handle_t *imu,
                             float *ax_mps2, float *ay_mps2, float *az_mps2);
esp_err_t mpu6050_read_gyro(mpu6050_handle_t *imu,
                            float *gx_rads, float *gy_rads, float *gz_rads);
esp_err_t mpu6050_read_accel_gyro(mpu6050_handle_t *imu,
                                  float *ax_mps2, float *ay_mps2, float *az_mps2,
                                  float *gx_rads, float *gy_rads, float *gz_rads);
