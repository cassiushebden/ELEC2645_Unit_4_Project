#ifndef POWER_H
#define POWER_H

#include "mbed.h"

void mpu6050_init();
int16_t read_accel_y();
int16_t map_to_power(int16_t ay);

// Extern declaration of shared I2C object
extern I2C i2c;

#endif // POWER_H
