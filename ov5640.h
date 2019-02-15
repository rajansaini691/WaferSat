/**
  * This is the OV2640 driver
  */

/**
 * @file    ov5640.h
 * @brief   Driver for OV5640 camera.
 * @notyes  Implements a pseudo DCMI capture capability.
 *
 * @addtogroup drivers
 * @{
 */

#ifndef __OV5640_H__
#define __OV5640_H__

#include "ch.h"
#include "hal.h"
#include "types.h"

/*===========================================================================*/
/* Module constants.                                                         */
/*===========================================================================*/

#define OV5640_I2C_ADR		    0x3C
//#define OV5640_I2C_ADR          0x30



#ifdef __cplusplus
extern "C" {
#endif
uint32_t    OV5640_Snapshot2RAM(void);
uint32_t    OV5640_Snapshot2RAM_RGB(void);
uint32_t    OV5640_Capture(void);
void        OV5640_InitGPIO(void);
void        OV5640_TransmitConfig(void);
void        OV5640_SetResolution(resolution_t res);
void        OV5640_init(void);
void        OV5640_init_RGB(void);
void        OV5640_deinit(void);
bool        OV5640_isAvailable(void);
void        OV5640_InitDCMI(void);
void        OV5640_InitDCMI_RGB(void);
void        OV5640_DeinitDCMI(void);
void        OV5640_DeinitDMA(void);
void        OV5640_InitDMA(void);
void        OV5640_InitDMA_RGB(void);
void        dma_avail(void*p, uint32_t flags);
void        dma_error(void*p, uint32_t flags);
void        OV5640_setLightIntensity(void);
uint32_t    OV5640_getLastLightIntensity(void);
uint8_t     OV5640_hasError(void);
#ifdef __cplusplus
}
#endif

#endif /* __OV5640_H__ */

//#define mode_320x240
//#define mode_640x480
//#define mode_1280x960
//#define mode_1280x960_2x2
#define mode_2048x1536 //broken, pixel clock is 96MHz

#ifdef mode_320x240
    #define OV5640_MAXX         320
    #define OV5640_MAXY         240
#endif
#ifdef mode_640x480
    #define OV5640_MAXX         640
    #define OV5640_MAXY         480
#endif
#ifdef mode_1280x960
    #define OV5640_MAXX         1280
    #define OV5640_MAXY         960
#endif
#ifdef mode_1280x960_2x2
    #define OV5640_MAXX         1280
    #define OV5640_MAXY         960
#endif
#ifdef mode_2048x1536
    #define OV5640_MAXX         2048
    #define OV5640_MAXY         1536
#endif



#define OV5640_NUM_PIXELS  OV5640_MAXX*OV5640_MAXY


extern uint8_t  OV5640_ram_buffer [OV5640_NUM_PIXELS*2];
extern volatile uint32_t OV5640_ram_buffer_length;


/** @} */
