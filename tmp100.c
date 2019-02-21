/*
 * tmp100.c
 * Library for TI TMP100 temperature sensor
 *
 *  Created on: Nov 8, 2018
 *      Author: Peter Krogen
 */

#include "ch.h"
#include "hal.h"
#include "tmp100.h"
#include "pi2c.h"
//#include "ei2c.h"  //software i2c
#include <math.h>

int16_t tmp100_get_temperature(uint8_t id){
  uint16_t val;
  if(!I2C_read16(TMP100_ADDR | (id & 0x07), TMP100_TEMP_REG, &val)) {
        return 0;
    }
  return (val>>4 )*25/4; //convert .0625C/count to 0.01C/count
}

bool tmp100_isAvailable(uint8_t id){
  uint8_t val;
    if(I2C_read8(TMP100_ADDR | (id & 0x07), TMP100_CONTR_REG, &val)) {
        return true;
    } else {
        return false;
    }
}


void tmp100_init(uint8_t id){
  I2C_write8(TMP100_ADDR | (id & 0x07), TMP100_CONTR_REG, TMP100_CONSECUTIVE_FAULTS_6  | TMP100_RESOLUTION_12_BIT );
}

// void tmp_shutdown? (shutdown mode allows the temperature sensor to turn off all circuitry besides the serial interface. May be useful when we need to conserve power?)
