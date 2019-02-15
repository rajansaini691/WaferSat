/*
 * ltr329.h
 * Library for Lite-On LTR-329ALS-01 ambient light sensor
 *
 *  Created on: Nov 8, 2018
 *      Author: Peter Krogen
 */

#ifndef LTR329_H_
#define LTR329_H_

#include "ch.h"
#include "hal.h"

#define LTR329_ADDR            0x29
#define LTR329_CONTR_REG       0x80
#define LTR329_MEAS_RATE_REG   0x85
#define LTR329_MANUFAC_ID      0x87

#define LTR329_DATA_CH1_LOW    0x88
#define LTR329_DATA_CH1_HIGH   0x89
#define LTR329_DATA_CH0_LOW    0x8A
#define LTR329_DATA_CH0_HIGH   0x8B

#define LTR329_ACTIVE          0x01
#define LTR329_STANDBY         0x00

#define LTR329_GAIN_1X         0x00<<2
#define LTR329_GAIN_2X         0x01<<2
#define LTR329_GAIN_4X         0x02<<2
#define LTR329_GAIN_8X         0x03<<2
#define LTR329_GAIN_48X        0x06<<2
#define LTR329_GAIN_96X        0x07<<2

#define LTR329_INT_50          0x01<<3
#define LTR329_INT_100         0x00<<3
#define LTR329_INT_150         0x04<<3
#define LTR329_INT_200         0x02<<3
#define LTR329_INT_250         0x05<<3
#define LTR329_INT_300         0x06<<3
#define LTR329_INT_350         0x07<<3
#define LTR329_INT_400         0x03<<3

#define LTR329_RATE_50         0x00
#define LTR329_RATE_100        0x01
#define LTR329_RATE_200        0x02
#define LTR329_RATE_500        0x03
#define LTR329_RATE_1000       0x04
#define LTR329_RATE_2000       0x05


int16_t ltr329_get_intensity_ch0(void);
int16_t ltr329_get_intensity_ch1(void);
bool ltr329_isAvailable(void);
void ltr329_init(void);
uint8_t ltr329_hasError(void);

#endif /* LTR329_H_ */
