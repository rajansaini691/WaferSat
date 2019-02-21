/*
 * mpu9250.h
 * Library for MPU9250 accelerometers
 *  Created on: Nov 8, 2018
 *      Author: Peter Krogen
 */

#ifndef MPU9250_H_
#define MPU9250_H_

#include "ch.h"
#include "hal.h"

#define MPU9250_ADDR            0x68 //ad0 low
//#define MPU9250_ADDR            0x69 //ad0 high
#define MPU9250_WHO_AM_I        0x75

#define MPU9250_ACCEL_XOUT_H    0x3b
#define MPU9250_ACCEL_XOUT_L    0x3c

#define MPU9250_ACCEL_YOUT_H    0x3d
#define MPU9250_ACCEL_YOUT_L    0x3e

#define MPU9250_ACCEL_ZOUT_H    0x3f
#define MPU9250_ACCEL_ZOUT_L    0x40

int16_t mpu9250_get_x(void);
int16_t mpu9250_get_y(void);
int16_t mpu9250_get_z(void);
bool mpu9250_isAvailable(void);
void mpu9250_init(void);
uint8_t mpu9250_hasError(void);

#endif /* MPU9250_H_ */
