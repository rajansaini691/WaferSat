/*
 * tmp100.h
 * Library for TI TMP100 temperature sensor
 *
 *  Created on: Nov 8, 2018
 *      Author: Peter Krogen
 */

#ifndef TMP100_H_
#define TMP100_H_

#include "ch.h"
#include "hal.h"

#define TMP100_ADDR                     0x48
#define TMP100_TEMP_REG                 0x00
#define TMP100_CONTR_REG                0x01
#define TMP100_TEMP_THRESH_HIGH_REG     0x02
#define TMP100_TEMP_THRESH_LOW_REG      0x03

#define TMP100_ENABLE_SHUTDOWN          0x01
#define TMP100_DISABLE_SHUTDOWN         0x00

//Thermostat mode
#define TMP100_COMPARATOR_MODE          0x00
#define TMP100_INTERRUPT_MODE           0x02

//Polarity
#define TMP100_POLARITY_LOW             0x00        //Active Low
#define TMP100_POLARITY_HIGH            0x04        //Active High

//Fault queue
#define TMP100_CONSECUTIVE_FAULTS_1     0x00
#define TMP100_CONSECUTIVE_FAULTS_2     0x08
#define TMP100_CONSECUTIVE_FAULTS_4     0x10
#define TMP100_CONSECUTIVE_FAULTS_6     0x18

//Converter Resolution
#define TMP100_RESOLUTION_9_BIT         0x00        //0.5 degC
#define TMP100_RESOLUTION_10_BIT        0x20        //0.25 degC
#define TMP100_RESOLUTION_11_BIT        0x40        //0.125 degC
#define TMP100_RESOLUTION_12_BIT        0x60        //0.0625 degC

int16_t tmp100_get_temperature(uint8_t id);
bool tmp100_isAvailable(uint8_t id);
void tmp100_init(uint8_t id);

#endif /* TMP100_H_ */
