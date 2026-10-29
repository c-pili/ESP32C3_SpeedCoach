#include "mpu6050.h"

#include "esp_check.h"
#include "esp_log.h"

#include <math.h>
#include <string.h>

static const char *TAG = "mpu6050";

#define REG_SMPLRT_DIV   0x19
#define REG_CONFIG       0x1A
#define REG_GYRO_CONFIG  0x1B
#define REG_ACCEL_CONFIG 0x1C
#define REG_ACCEL_XOUT_H 0x3B
#define REG_PWR_MGMT_1   0x6B
#define REG_WHO_AM_I     0x75

#define MPU6050_WHO_AM_I 0x68

static esp_err_t write8(mpu6050_handle_t *imu, uint8_t reg, uint8_t value)
{
    return i2c_helper_write_reg(imu->dev, reg, &value, 1);
}

esp_err_t mpu6050_init(mpu6050_handle_t *imu, i2c_helper_t *bus, uint8_t addr_7bit)
{
    if (!imu || !bus || (addr_7bit != 0x68 && addr_7bit != 0x69)) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(imu, 0, sizeof(*imu));
    ESP_RETURN_ON_ERROR(i2c_helper_add_device(bus, addr_7bit, &imu->dev), TAG, "add device");

    uint8_t who = 0;
    ESP_RETURN_ON_ERROR(i2c_helper_read_reg(imu->dev, REG_WHO_AM_I, &who, 1), TAG, "WHO_AM_I");
    if (who != MPU6050_WHO_AM_I) {
        ESP_LOGE(TAG, "unexpected WHO_AM_I 0x%02x (expected 0x%02x)", who, MPU6050_WHO_AM_I);
        return ESP_ERR_NOT_FOUND;
    }

    /* Wake the sensor, use the gyro PLL, and sample at 100 Hz with a 44 Hz DLPF. */
    ESP_RETURN_ON_ERROR(write8(imu, REG_PWR_MGMT_1, 0x01), TAG, "wake");
    ESP_RETURN_ON_ERROR(write8(imu, REG_CONFIG, 0x03), TAG, "DLPF");
    ESP_RETURN_ON_ERROR(write8(imu, REG_SMPLRT_DIV, 9), TAG, "sample rate");
    ESP_RETURN_ON_ERROR(write8(imu, REG_GYRO_CONFIG, 0x08), TAG, "gyro range"); /* ±500 dps */
    ESP_RETURN_ON_ERROR(write8(imu, REG_ACCEL_CONFIG, 0x10), TAG, "accel range"); /* ±8 g */

    imu->accel_scale = (8.0f * 9.80665f) / 32768.0f;
    imu->gyro_scale = ((500.0f / 32768.0f) * (float)M_PI) / 180.0f;
    ESP_LOGI(TAG, "MPU6050 ready at 0x%02x, accel ±8g, gyro ±500dps, 100Hz", addr_7bit);
    return ESP_OK;
}

esp_err_t mpu6050_read_accel_gyro(mpu6050_handle_t *imu,
                                  float *ax_mps2, float *ay_mps2, float *az_mps2,
                                  float *gx_rads, float *gy_rads, float *gz_rads)
{
    if (!imu) return ESP_ERR_INVALID_ARG;
    uint8_t buf[14];
    ESP_RETURN_ON_ERROR(i2c_helper_read_reg(imu->dev, REG_ACCEL_XOUT_H, buf, sizeof(buf)),
                        TAG, "read sample");

    int16_t raw[6] = {
        (int16_t)((buf[0] << 8) | buf[1]),
        (int16_t)((buf[2] << 8) | buf[3]),
        (int16_t)((buf[4] << 8) | buf[5]),
        (int16_t)((buf[8] << 8) | buf[9]),
        (int16_t)((buf[10] << 8) | buf[11]),
        (int16_t)((buf[12] << 8) | buf[13]),
    };
    if (ax_mps2) *ax_mps2 = raw[0] * imu->accel_scale;
    if (ay_mps2) *ay_mps2 = raw[1] * imu->accel_scale;
    if (az_mps2) *az_mps2 = raw[2] * imu->accel_scale;
    if (gx_rads) *gx_rads = raw[3] * imu->gyro_scale;
    if (gy_rads) *gy_rads = raw[4] * imu->gyro_scale;
    if (gz_rads) *gz_rads = raw[5] * imu->gyro_scale;
    return ESP_OK;
}

esp_err_t mpu6050_read_accel(mpu6050_handle_t *imu,
                             float *ax_mps2, float *ay_mps2, float *az_mps2)
{
    return mpu6050_read_accel_gyro(imu, ax_mps2, ay_mps2, az_mps2, NULL, NULL, NULL);
}

esp_err_t mpu6050_read_gyro(mpu6050_handle_t *imu,
                            float *gx_rads, float *gy_rads, float *gz_rads)
{
    return mpu6050_read_accel_gyro(imu, NULL, NULL, NULL, gx_rads, gy_rads, gz_rads);
}
