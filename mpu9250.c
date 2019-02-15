/*
 * MPU9250.c
 * Library for MPU9250 accelerometer
 *
 *  Created on: Nov 8, 2018
 *      Author: Peter Krogen
 */

#include "ch.h"
#include "hal.h"
#include "mpu9250.h"
#include "pi2c.h"
//#include "ei2c.h"  //software i2c
#include <math.h>

static uint8_t error;


int16_t mpu9250_get_x(void){
  uint16_t val;
  int32_t val2;

  if(!I2C_read16(MPU9250_ADDR, MPU9250_ACCEL_XOUT_H, &val)) {
        error |= 0x1;
        return 0;
    }
  val2 = (int16_t)val*1000/16384;
  return val2;}

int16_t mpu9250_get_y(void){
  uint16_t val;
  int32_t val2;

  if(!I2C_read16(MPU9250_ADDR, MPU9250_ACCEL_YOUT_H, &val)) {
        error |= 0x1;
        return 0;
    }
  val2 = (int16_t)val*1000/16384;
  return val2;
}

int16_t mpu9250_get_z(void){
  uint16_t val;
  int32_t val2;

  if(!I2C_read16(MPU9250_ADDR, MPU9250_ACCEL_ZOUT_H, &val)) {
        error |= 0x1;
        return 0;
    }
  val2 = (int16_t)val*1000/16384;
  return val2;
}


bool mpu9250_isAvailable(void){
  uint8_t val;
    if(I2C_read8(MPU9250_ADDR, MPU9250_WHO_AM_I, &val)) {
        error |= val != 0x71;
        return val == 0x71;
    } else {
        error |= 0x1;
        return false; // mpu9250 not found
    }
}


void mpu9250_init(void){

}

uint8_t mpu9250_hasError(void){
    uint8_t ret = error;
    error = 0x0;
    return ret;
}

