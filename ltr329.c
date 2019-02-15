/*
 * ltr329.c
 * Library for Lite-On LTR-329ALS-01 ambient light sensor
 *
 *  Created on: Nov 8, 2018
 *      Author: Peter Krogen
 */

#include "ch.h"
#include "hal.h"
#include "ltr329.h"
#include "pi2c.h"
//#include "ei2c.h"  //software i2c
#include <math.h>

static uint8_t error;


int16_t ltr329_get_intensity_ch0(void){
  uint16_t val;
  val = 0;

  if(!I2C_read16(LTR329_ADDR, LTR329_DATA_CH0_LOW, &val)) {
        error |= 0x1;
        return 0;
    }

  return (val>>8 & 0xFF) | (val & 0xFF)<<8; //swap high and low byte
}

int16_t ltr329_get_intensity_ch1(void){
  uint16_t val;
  val = 0;

  if(!I2C_read16(LTR329_ADDR, LTR329_DATA_CH1_LOW, &val)) {
        error |= 0x1;
        return 0;
    }

  return (val>>8 & 0xFF) | (val & 0xFF)<<8; //swap high and low byte
}

bool ltr329_isAvailable(void){
  uint8_t val;
    if(I2C_read8(LTR329_ADDR, LTR329_MANUFAC_ID, &val)) {
        error |= val != 0x05;
        return val == 0x05;
    } else {
        error |= 0x1;
        return false; // ltr329 not found
    }
}


void ltr329_init(void){

  I2C_write8(LTR329_ADDR, LTR329_CONTR_REG, LTR329_GAIN_1X | LTR329_ACTIVE);
  I2C_write8(LTR329_ADDR, LTR329_MEAS_RATE_REG, LTR329_INT_400  | LTR329_RATE_500);
  chThdSleepMilliseconds(10);
}

uint8_t ltr329_hasError(void){
    uint8_t ret = error;
    error = 0x0;
    return ret;
}

