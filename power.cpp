#include "power.h"

// MPU6050 I2C address (default: 0x68 when AD0 is grounded)
#define MPU6050_ADDR 0x68

#define PWR_MGMT_1     0x6B
#define ACCEL_YOUT_H   0x3D
#define ACCEL_YOUT_L   0x3E

void mpu6050_write(uint8_t regAddr, uint8_t data) {
    char cmd[2] = { regAddr, data };
    i2c.write(MPU6050_ADDR << 1, cmd, 2);
}

uint8_t mpu6050_read(uint8_t regAddr) {
    char cmd = regAddr;
    char data;
    i2c.write(MPU6050_ADDR << 1, &cmd, 1);
    i2c.read(MPU6050_ADDR << 1, &data, 1);
    return data;
}

void mpu6050_init() {
    mpu6050_write(PWR_MGMT_1, 0x00);
}

int16_t read_accel_y() {
    uint8_t high = mpu6050_read(ACCEL_YOUT_H);
    uint8_t low = mpu6050_read(ACCEL_YOUT_L);
    return (int16_t)((high << 8) | low);
}

int16_t map_to_power(int16_t ay) {
    if (ay < -16700) ay = -16700;
    if (ay > 16400) ay = 16400;

    int32_t range = 33100;
    int32_t shifted = ay + 16700;
    return (shifted * 100) / range;
}
