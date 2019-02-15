/*
    Copyright (C) 2013-2015 Andrea Zoppi

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "ch.h"
#include "hal.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_dcmi.h"
#include "string.h"
#include <stdarg.h>
#include "chprintf.h"
#include "shell.h"
#include "chprintf.h"
#include "rt_test_root.h"
#include "dcmi_dma.h"
#include "hal_fsmc_sdram.h"
#include "membench.h"
#include "ff.h"
#include "pi2c.h"
#include "ov5640.h"
#include "is42s16400j.h"
#include "collector.h"
#include "debug.h"


#define IN_DRAM __attribute__((section(".ram7")))
uint8_t  OV5640_ram_buffer[OV5640_NUM_PIXELS*2] IN_DRAM;
volatile uint32_t OV5640_ram_buffer_length = 0;

int image_index = 0;

#if (HAL_USE_SERIAL_USB == TRUE)
#include "usbcfg.h"
#endif

static dataPoint_t sensor_data;

volatile uint32_t buffNum = 0;

static thread_t *shelltp = NULL;

// Serial interfaces

// Console port (discovery internal serial port)
static const SerialConfig serial_cfg1 = {
                                         38400,
                                         0,
                                         0,
                                         0};


// GPS
static const SerialConfig serial_cfg2 = {
                                         9600,
                                         0,
                                         0,
                                         0};

// Satcom
static const SerialConfig serial_cfg3 = {
                                         115200,
                                         0,
                                         0,
                                         0};
// Lasercom interface (IRDA)
static const SerialConfig serial_cfg7 = {
                                      115200,
                                      0,
                                      0,
                                      USART_CR3_IREN};



/*
 * Memory benchmark definitions
 */

static uint8_t int_buf[64*1024];

/*
 *
 */
static membench_t membench_ext = {
    SDRAM_START,
    SDRAM_SIZE,
};

static membench_t membench_int = {
    int_buf,
    sizeof(int_buf),
};

static membench_result_t membench_result_ext2int;
static membench_result_t membench_result_int2ext;


/*
 * FatFS object
 */

static FATFS SDC_FS;

/* FS mounted and ready.*/
static bool fs_ready = FALSE;

/* Generic large buffer.*/
static uint8_t fbuff[1024];


/*
 ******************************************************************************
 ******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************
 ******************************************************************************
 */

/*===========================================================================*/
/* Camera related.                                                           */
/*===========================================================================*/

/*
 * Copy RGB565 data from OV5640_ram_buffer to file on SD card in .PPM P3 format
 */
static void write_rgb(void) {
  FIL fil;        /* File object */
  FRESULT fr;     /* FatFs return code */
  //  BYTE buffer[4096];   /* File copy buffer */
  UINT br, bw;         /* File read/write count */
  palSetPad(GPIOD, GPIOD_LED_BLUE);  //turn on red led on discovery
  /* Open a file */
  chprintf((BaseSequentialStream *)&SD_console, "Open file...");
  fr = f_open(&fil, "image1.ppm", FA_WRITE | FA_CREATE_ALWAYS);
  chprintf((BaseSequentialStream *)&SD_console, "done, return code = %d\r\n", fr);

  //header
  static char header [] = "P3\n0640 0480\n255\n";
  br = strlen(header);
  header[3] = OV5640_MAXX/1000%10 + '0';
  header[4] = OV5640_MAXX/100%10 + '0';
  header[5] = OV5640_MAXX/10%10 + '0';
  header[6] = OV5640_MAXX%10 + '0';

  header[8] = OV5640_MAXY/1000%10 + '0';
  header[9] = OV5640_MAXY/100%10 + '0';
  header[10] = OV5640_MAXY/10%10 + '0';
  header[11] = OV5640_MAXY%10 + '0';

  chprintf((BaseSequentialStream *)&SD_console, "Write header...");
  fr = f_write(&fil, header, br, &bw);
  if (fr || bw < br){ /* error or disk full */
      chprintf((BaseSequentialStream *)&SD_console, "write error!\r\n");
      return;
  } else{
  chprintf((BaseSequentialStream *)&SD_console, "done, %d bytes written\r\n", bw);
  }

  //pixels

  unsigned y,x;
  uint16_t color;
  unsigned r,g,b;
  for(y = 0; y < OV5640_MAXY; y += 1)
  {
    for(x = 0; x < OV5640_MAXX; x += 1)
    {
      color = (OV5640_ram_buffer[2*(y*OV5640_MAXX + x)+1]>>8) + OV5640_ram_buffer[2*(y*OV5640_MAXX + x)];
      b = ((color&0x001F)<<3);    // 5bit blue
      g = ((color&0x07E0)>>3);    // 6bit green
      r = ((color&0xF800)>>8);    // 5bit red

      fbuff[0] = r/100%10 + '0';
      fbuff[1] = r/10%10 + '0';
      fbuff[2] = r%10 + '0';
      fbuff[3] = ' ';

      fbuff[4] = g/100%10 + '0';
      fbuff[5] = g/10%10 + '0';
      fbuff[6] = g%10 + '0';
      fbuff[7] = ' ';

      fbuff[8] = b/100%10 + '0';;
      fbuff[9] = b/10%10 + '0';
      fbuff[10] = b%10 + '0';
      fbuff[11] = ' ';

      fbuff[12] = '\n';
      br = 13;
      fr = f_write(&fil, fbuff, br, &bw);
              if (fr || bw < br){ /* error or disk full */
                  chprintf((BaseSequentialStream *)&SD_console, "write error!\r\n");
                  break;
              }
    }
    chprintf((BaseSequentialStream *)&SD_console, ".");
  }

  /* Close file */
  chprintf((BaseSequentialStream *)&SD_console, "Close file...");
  fr = f_close(&fil);
  chprintf((BaseSequentialStream *)&SD_console, "done, return code = %d\r\n", fr);
  palClearPad(GPIOD, GPIOD_LED_BLUE); //turn off red led on discovery

}

/*
 * Copy JPEG data from OV5640_ram_buffer to file on SD card
 */
static void write_jpeg(void) {
  FIL fil;        /* File object */
   FRESULT fr;     /* FatFs return code */
   //  BYTE buffer[4096];   /* File copy buffer */
   UINT br, bw;         /* File read/write count */
   uint32_t file_position;
   time_measurement_t mem_tmu;
   char filename[20];


   palSetPad(GPIOD, GPIOD_LED_BLUE);  //turn on red led on discovery

   chprintf((BaseSequentialStream *)&SD_console, "SDC  > File preview: ");
   if (OV5640_ram_buffer_length > 10) //if file size is greater than 10 bytes, print abbreviated version
   {
     for(file_position=0; file_position<5; file_position++){
       chprintf((BaseSequentialStream *)&SD_console, "%02X, ", OV5640_ram_buffer[file_position]);
       //chprintf((BaseSequentialStream *)&SD_console, "%02X, %02X, ", (unsigned char)(OV5640_ram_buffer[file_position] & 0x00FF), (unsigned char)((OV5640_ram_buffer[file_position] & 0xFF00)>>8));
     }
     chprintf((BaseSequentialStream *)&SD_console, "%02X ... ", OV5640_ram_buffer[file_position]);
     for(file_position=OV5640_ram_buffer_length-5; file_position<OV5640_ram_buffer_length-1; file_position++){
       chprintf((BaseSequentialStream *)&SD_console, "%02X, ", OV5640_ram_buffer[file_position]);
     }
     chprintf((BaseSequentialStream *)&SD_console, "%02X\r\n", OV5640_ram_buffer[file_position]);
   }
   else //otherwise print all bytes
   {
     for(file_position=0; file_position<OV5640_ram_buffer_length; file_position++){
       chprintf((BaseSequentialStream *)&SD_console, "%02X, ", OV5640_ram_buffer[file_position]);
     }
     chprintf((BaseSequentialStream *)&SD_console, "%02X\r\n", OV5640_ram_buffer[file_position]);
   }
   /* Open a file */
   //if file exists, increment filename and try again

   chsnprintf(filename, sizeof(filename), "image%06d.jpg",image_index);
   chprintf((BaseSequentialStream *)&SD_console, "SDC  > Open %s ...", filename);
   fr = f_open(&fil, filename, FA_WRITE | FA_CREATE_NEW);
   if (fr == FR_EXIST){
     chprintf((BaseSequentialStream *)&SD_console, "SDC  > file exists, search for first unused index...\r\n", filename);
     while(fr == FR_EXIST && image_index < 1000000){
     image_index++;
     chsnprintf(filename, sizeof(filename), "image%06d.jpg",image_index);
     fr = f_open(&fil, filename, FA_WRITE | FA_CREATE_NEW);
     }
     chprintf((BaseSequentialStream *)&SD_console, "CAM  > Open %s ...", filename);
   }
   chprintf((BaseSequentialStream *)&SD_console, " done, return code = %d\r\n", fr);


   chprintf((BaseSequentialStream *)&SD_console, "SDC  > Write data...");
   chTMObjectInit(&mem_tmu);
   chTMStartMeasurementX(&mem_tmu);
   br = OV5640_ram_buffer_length;
   fr = f_write(&fil, OV5640_ram_buffer, br , &bw);
   chTMStopMeasurementX(&mem_tmu);
   if (fr || bw < br){ /* error or disk full */
       chprintf((BaseSequentialStream *)&SD_console, "write error!\r\n");
       return;
   } else{
   chprintf((BaseSequentialStream *)&SD_console, "done, %.3f MB written, ", bw/1e6);
   chprintf((BaseSequentialStream *)&SD_console, "took, %d us, ", ((&mem_tmu)->last)/(STM32_SYSCLK/1000000));
   chprintf((BaseSequentialStream *)&SD_console, "%.2f MB/s\r\n", bw*1.0/ (((&mem_tmu)->last)/(STM32_SYSCLK/1000000)) );
   image_index++;
   }


   /* Close file */
   chprintf((BaseSequentialStream *)&SD_console, "SDC  > Close file...");
   fr = f_close(&fil);
   chprintf((BaseSequentialStream *)&SD_console, "done, return code = %d\r\n", fr);
   palClearPad(GPIOD, GPIOD_LED_BLUE); //turn off red led on discovery
}

/*===========================================================================*/
/* FSMC related.                                                             */
/*===========================================================================*/
/*
 * Erases the whole SDRAM bank.
 */
static void sdram_bulk_erase(void) {

  volatile uint8_t *p = (volatile uint8_t *)SDRAM_BANK_ADDR;
  volatile uint8_t *end = p + IS42S16400J_SIZE;
  while (p < end)
    *p++ = 0;
}

/*
 * memory benchmark
 */
static void membench(BaseSequentialStream *chp) {
  chprintf(chp, "Starting External RAM Benchmark....");
  membench_run(&membench_ext, &membench_int, &membench_result_int2ext);
  chprintf(chp, ".");
  membench_run(&membench_int, &membench_ext, &membench_result_ext2int);
  chprintf(chp, ".");

  chprintf(chp, "OK\r\nRAM Performance (MB/s) memset, memcpy, memcpy_dma, memcmp\r\n");
  chprintf(chp, "int2ext:                 %5D %7D %10D %7D \r\n",
             membench_result_int2ext.memset/1000000,
             membench_result_int2ext.memcpy/1000000,
             membench_result_int2ext.memcpy_dma/1000000,
             membench_result_int2ext.memcmp/1000000);

  chprintf(chp, "ext2int:                 %5D %7D %10D %7D \r\n",
             membench_result_ext2int.memset/1000000,
             membench_result_ext2int.memcpy/1000000,
             membench_result_ext2int.memcpy_dma/1000000,
             membench_result_ext2int.memcmp/1000000);
}

/*===========================================================================*/
/* FatFs related.                                                            */
/*===========================================================================*/


static FRESULT scan_files(BaseSequentialStream *chp, char *path) {
  static FILINFO fno;
  FRESULT res;
  DIR dir;
  size_t i;
  char *fn;

  res = f_opendir(&dir, path);
  if (res == FR_OK) {
    i = strlen(path);
    while (((res = f_readdir(&dir, &fno)) == FR_OK) && fno.fname[0]) {
      if (FF_FS_RPATH && fno.fname[0] == '.')
        continue;
      fn = fno.fname;
      if (fno.fattrib & AM_DIR) {
        *(path + i) = '/';
        strcpy(path + i + 1, fn);
        res = scan_files(chp, path);
        *(path + i) = '\0';
        if (res != FR_OK)
          break;
      }
      else {
        chprintf(chp, "%s/%s\r\n", path, fn);
      }
    }
  }
  return res;
}


/*===========================================================================*/
/* SDC related                                                               */
/*===========================================================================*/

/*
 * Card insertion event.
 */
static void InsertHandler(eventid_t id) {
  FRESULT err;

  (void)id;
  /*
   * On insertion SDC initialization and FS mount.
   */
#if HAL_USE_SDC
  if (sdcConnect(&SDCD1))
#else
  if (mmcConnect(&MMCD1))
#endif
    return;

  err = f_mount(&SDC_FS, "/", 1);
  if (err != FR_OK) {
#if HAL_USE_SDC
    sdcDisconnect(&SDCD1);
#else
    mmcDisconnect(&MMCD1);
#endif
    return;
  }
  fs_ready = TRUE;
}



/*
 * Card removal event.
 */
static void RemoveHandler(eventid_t id) {

  (void)id;
#if HAL_USE_SDC
    sdcDisconnect(&SDCD1);
#else
    mmcDisconnect(&MMCD1);
#endif
  fs_ready = FALSE;
}

/*
 * Shell exit event.
 */
static void ShellHandler(eventid_t id) {

  (void)id;
  if (chThdTerminatedX(shelltp)) {
    chThdRelease(shelltp);
    shelltp = NULL;
  }
}



/*===========================================================================*/
/* Card insertion monitor.                                                   */
/*===========================================================================*/

#define POLLING_INTERVAL                10
#define POLLING_DELAY                   10

/**
 * @brief   Card monitor timer.
 */
static virtual_timer_t tmr;

/**
 * @brief   Debounce counter.
 */
static unsigned cnt;

/**
 * @brief   Card event sources.
 */
static event_source_t inserted_event, removed_event;

/**
 * @brief   Insertion monitor timer callback function.
 *
 * @param[in] p         pointer to the @p BaseBlockDevice object
 *
 * @notapi
 */
static void tmrfunc(void *p) {
  BaseBlockDevice *bbdp = p;

  chSysLockFromISR();
  if (cnt > 0) {
    if (blkIsInserted(bbdp)) {
      if (--cnt == 0) {
        chEvtBroadcastI(&inserted_event);
      }
    }
    else
      cnt = POLLING_INTERVAL;
  }
  else {
    if (!blkIsInserted(bbdp)) {
      cnt = POLLING_INTERVAL;
      chEvtBroadcastI(&removed_event);
    }
  }
  chVTSetI(&tmr, TIME_MS2I(POLLING_DELAY), tmrfunc, bbdp);
  chSysUnlockFromISR();
}

/**
 * @brief   Polling monitor start.
 *
 * @param[in] p         pointer to an object implementing @p BaseBlockDevice
 *
 * @notapi
 */
static void tmr_init(void *p) {

  chEvtObjectInit(&inserted_event);
  chEvtObjectInit(&removed_event);
  chSysLock();
  cnt = POLLING_INTERVAL;
  chVTSetI(&tmr, TIME_MS2I(POLLING_DELAY), tmrfunc, p);
  chSysUnlock();
}


/*===========================================================================*/
/* Misc threads                                                              */
/*===========================================================================*/



/*
 * Red LED blinker thread, times are in milliseconds.
 */
static THD_WORKING_AREA(waThread1, 128);
static THD_FUNCTION(Thread1, arg) {

  (void)arg;
  chRegSetThreadName("blinker1");
  while (true) {
    palClearPad(GPIOD, GPIOD_LED_RED);
    chThdSleepMilliseconds(100);
    //chThdSleepMilliseconds(fs_ready ? 250 : 500);

    palSetPad(GPIOD, GPIOD_LED_RED);
    chThdSleepMilliseconds(1);
    //chThdSleepMilliseconds(fs_ready ? 250 : 500);
  }
}


/*
 * Green LED blinker thread, times are in milliseconds.
 */
static THD_WORKING_AREA(waThread2, 128);
static THD_FUNCTION(Thread2, arg) {

  (void)arg;
  chRegSetThreadName("blinker2");
  while (true) {
    palClearPad(GPIOD, GPIOD_LED_GREEN);
    chThdSleepMilliseconds(fs_ready ? 250 : 500);
    palSetPad(GPIOD, GPIOD_LED_GREEN);
    chThdSleepMilliseconds(1);
  }
}


/*
 * Blue LED blinker thread, times are in milliseconds.
 */
static THD_WORKING_AREA(waThread3, 128);
static THD_FUNCTION(Thread3, arg) {

  (void)arg;
  chRegSetThreadName("blinker3");
  while (true) {
    palClearPad(GPIOD, GPIOD_LED_BLUE);
    chThdSleepMilliseconds(500);
    palSetPad(GPIOD, GPIOD_LED_BLUE);
    chThdSleepMilliseconds(1);
  }
}




/*===========================================================================*/
/* Command line related.                                                     */
/*===========================================================================*/

#define SHELL_WA_SIZE   THD_WORKING_AREA_SIZE(2048)
#define TEST_WA_SIZE    THD_WORKING_AREA_SIZE(256)

/*===========================================================================*/
/* Camera related                                                            */
/*===========================================================================*/

static void cmd_image_rgb(BaseSequentialStream *chp, int argc, char *argv[]) {
  FRESULT fr;     /* FatFs return code */

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: image\r\n");
    return;
  }

  chprintf((BaseSequentialStream *)&SD_console, "Camera Init...\r\n");
  OV5640_init_RGB();
  chprintf((BaseSequentialStream *)&SD_console, "Starting RGB Capture...\r\n");
  OV5640_Snapshot2RAM_RGB();   // Sample data from DCMI through DMA to RAM
  chprintf((BaseSequentialStream *)&SD_console, "capture done\r\n");

  if (!fs_ready) {
    chprintf(chp, "File System not mounted\r\n");
    return;
  }
  /* Register work area to the default drive */
  chprintf((BaseSequentialStream *)&SD_console, "Mounting file system...");
  fr = f_mount(&SDC_FS, "0:", 0);
  chprintf((BaseSequentialStream *)&SD_console, "done, return code = %d\r\n", fr);

  /* Write image data */
  write_rgb();
}

static void cmd_cam_init(BaseSequentialStream *chp, int argc, char *argv[]) {

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: image\r\n");
    return;
  }

  //chprintf((BaseSequentialStream *)&SD_console, "Camera Init...\r\n");
  OV5640_init();
  //OV5640_SetResolution(RES_XGA);
  //OV5640_SetResolution(RES_UXGA);
  //OV5640_SetResolution(RES_5MP);

}

static void cmd_image(BaseSequentialStream *chp, int argc, char *argv[]) {
  FRESULT fr;     /* FatFs return code */

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: image\r\n");
    return;
  }

  while(1){
  //chprintf((BaseSequentialStream *)&SD_console, "Starting JPEG Capture...\r\n");
  palSetPad(GPIOD, GPIOD_LED_RED);
  OV5640_Snapshot2RAM();   // Sample data from DCMI through DMA to RAM
  palClearPad(GPIOD, GPIOD_LED_RED);

  //chprintf((BaseSequentialStream *)&SD_console, "capture done\r\n");

  if (!fs_ready) {
    chprintf(chp, "File System not mounted\r\n");
    return;
  }
  /* Register work area to the default drive */
  chprintf((BaseSequentialStream *)&SD_console, "SDC  > Mounting file system...");
  fr = f_mount(&SDC_FS, "0:", 0);
  chprintf((BaseSequentialStream *)&SD_console, "done, return code = %d\r\n", fr);

  /* Write image data */
  write_jpeg();
  palSetPad(GPIOD, GPIOD_LED_GREEN);
  chThdSleepMilliseconds(1000);
  palClearPad(GPIOD, GPIOD_LED_GREEN);

//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);
//  chThdSleepMilliseconds(1000);



  }
}

static void cmd_reset(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: reset\r\n");
    return;
  }

  chprintf(chp, "Will reset in 200ms\r\n");
  chThdSleepMilliseconds(200);
  NVIC_SystemReset();
}

static void cmd_tree(BaseSequentialStream *chp, int argc, char *argv[]) {
  FRESULT err;
  uint32_t fre_clust;
  FATFS *fsp;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: tree\r\n");
    return;
  }
  if (!fs_ready) {
    chprintf(chp, "File System not mounted\r\n");
    return;
  }
  err = f_getfree("/", &fre_clust, &fsp);
  if (err != FR_OK) {
    chprintf(chp, "FS: f_getfree() failed\r\n");
    return;
  }
  chprintf(chp,
           "FS: %lu free clusters with %lu sectors (%lu bytes) per cluster\r\n",
           fre_clust, (uint32_t)fsp->csize, (uint32_t)fsp->csize * 512);
  fbuff[0] = 0;
  scan_files(chp, (char *)fbuff);
}

static void cmd_mem_external(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: membench\r\n");
    return;
  }

  membench(chp);
}

static void cmd_blink(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: blink\r\n");
    return;
  }

      chThdCreateStatic(waThread1, sizeof(waThread1),
                        NORMALPRIO + 10, Thread1, NULL);
      chThdCreateStatic(waThread2, sizeof(waThread2),
                        NORMALPRIO + 10, Thread2, NULL);
      chThdCreateStatic(waThread3, sizeof(waThread3),
                        NORMALPRIO + 10, Thread3, NULL);

}

static void cmd_sensors(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: sensors\r\n");
    return;
  }
  chprintf(chp, "Reading from sensors...\r\n");
  getSensors(&sensor_data);
  //chThdSleepMilliseconds(10); //let serial buffer clear out before starting shell
  chprintf(chp, "Vin = %04.1fV, ", (&sensor_data)->adc_vbat/1e3);
  chprintf(chp, "\r\n");

  chprintf(chp, "STM32int Temp = %06.2fC, ", (&sensor_data)->stm32_temp/1e2);
  chprintf(chp, "\r\n");
  chprintf(chp, "TMP100 0 Temp = %06.2fC, ", (&sensor_data)->tmp100_0_temp/1e2);
  chprintf(chp, "\r\n");
  chprintf(chp, "TMP100 1 Temp = %06.2fC, ", (&sensor_data)->tmp100_1_temp/1e2);
  chprintf(chp, "\r\n");
  chprintf(chp, "TMP100 2 Temp = %06.2fC, ", (&sensor_data)->tmp100_2_temp/1e2);
  chprintf(chp, "\r\n");
  chprintf(chp, "TMP100 3 Temp = %06.2fC, ", (&sensor_data)->tmp100_3_temp/1e2);
  chprintf(chp, "\r\n");
  chprintf(chp, "TMP100 4 Temp = %06.2fC, ", (&sensor_data)->tmp100_4_temp/1e2);
  chprintf(chp, "\r\n");
  chprintf(chp, "TMP100 5 Temp = %06.2fC, ", (&sensor_data)->tmp100_5_temp/1e2);
  chprintf(chp, "\r\n");
  chprintf(chp, "TMP100 6 Temp = %06.2fC, ", (&sensor_data)->tmp100_6_temp/1e2);
  chprintf(chp, "\r\n");
  chprintf(chp, "TMP100 7 Temp = %06.2fC, ", (&sensor_data)->tmp100_7_temp/1e2);
  chprintf(chp, "\r\n");

  chprintf(chp, "BME280 0x76: press = %01.6fbar, ", (&sensor_data)->sen_e2_press/1e6);
  chprintf(chp, "temp = %06.2fC ", (&sensor_data)->sen_e2_temp/1e2);
  chprintf(chp, "hum = %03.0f%% ", (&sensor_data)->sen_e2_hum/1e0);
  chprintf(chp, "\r\n");

  chprintf(chp, "BME280 0x77: press = %01.6fbar, ", (&sensor_data)->sen_i1_press/1e6);
  chprintf(chp, "temp = %06.2fC ", (&sensor_data)->sen_i1_temp/1e2);
  chprintf(chp, "hum = %03.0f%% ", (&sensor_data)->sen_i1_hum/1e0);
  chprintf(chp, "\r\n");

  chprintf(chp, "LTR329 light intensity ");
  chprintf(chp, "CH0 = %d lux, ", (&sensor_data)->ltr329_intensity_ch0);
  chprintf(chp, "CH1 = %d lux", (&sensor_data)->ltr329_intensity_ch1);
  chprintf(chp, "\r\n");

  chprintf(chp, "OV5640 light intensity = %d ", (&sensor_data)->light_intensity);
  chprintf(chp, "\r\n");

  chprintf(chp, "MPU-9250 x_accel = %04.3f ", (&sensor_data)->mpu9250_x_accel/1e3);
  chprintf(chp, "y_accel = %04.3f ", (&sensor_data)->mpu9250_y_accel/1e3);
  chprintf(chp, "z_accel = %04.3f ", (&sensor_data)->mpu9250_z_accel/1e3);
  chprintf(chp, "\r\n");

}


static const ShellCommand commands[] = {
  {"tree", cmd_tree},
  {"cam_init", cmd_cam_init},
  {"image", cmd_image},
  {"i", cmd_image},
  {"image_rgb", cmd_image_rgb},
  {"membench", cmd_mem_external},
  {"blink", cmd_blink},
  {"sensors", cmd_sensors},
  {"reset", cmd_reset},
  {NULL, NULL}
};


static const ShellConfig shell_cfg1 = { (BaseSequentialStream *)&SD_console, \
                                        commands };

/*===========================================================================*/
/* Initialization and main thread.                                           */
/*===========================================================================*/


/*
 * Application entry point.
 */
int main(void) {

  static const evhandler_t evhndl[] = {
    InsertHandler,
    RemoveHandler,
    ShellHandler
  };
  event_listener_t el0, el1, el2;

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
    halInit();
    chSysInit();

    /*
     * Activates the serial driver for communication with host
     * PA9(TX) and PA10(RX) are routed to USART1 (internal discovery serial).
     */
#if (HAL_USE_SERIAL_USB == TRUE)
    /*
     * Initializes a serial-over-USB CDC driver.
     */
    sduObjectInit(&SDU2);
    sduStart(&SDU2, &serusbcfg);
    usbDisconnectBus(serusbcfg.usbp);  //reset USB
    chThdSleepMilliseconds(1000);      //delay in order to avoid the need to
    usbStart(serusbcfg.usbp, &usbcfg); //disconnect cable after reset
    usbConnectBus(serusbcfg.usbp);
#else
    sdStart(&SD_console, &serial_cfg1); //init serial port
#endif /* HAL_USE_SERIAL_USB */
    chprintf((BaseSequentialStream *)&SD_console, "Hello Earth\r\n");



    // Init I2C - taken care of inside pi2c
    //i2cStart(&I2CD1, &i2cfg1);
    //chThdSleepMilliseconds(10);

    //init serial port 2 (gps)
    sdStart(&SD3, &serial_cfg2);

    //init serial port 3 (satcom) for upload image data - no longer used
    sdStart(&SD3, &serial_cfg3);

    //init serial port 7 (lasercom)
    sdStart(&SD3, &serial_cfg7);
    //palSetPadMode(GPIOF, 7, PAL_MODE_OUTPUT_PUSHPULL);
    //palClearPad(GPIOF, 7);

  /*
   * Initialise FSMC for SDRAM.
   */
  fsmcSdramInit();
  fsmcSdramStart(&SDRAMD, &sdram_cfg);
  sdram_bulk_erase();


    /*
     * Activates the  SDC driver 1 using default configuration.
     */
    sdcStart(&SDCD1, NULL);
    tmr_init(&SDCD1); //Activates the card insertion monitor.


  /*
   * Normal main() thread activity, in this demo it just performs
   * a shell respawn upon its termination.
   */

  chEvtRegister(&inserted_event, &el0, 0);
  chEvtRegister(&removed_event, &el1, 1);
  chEvtRegister(&shell_terminated, &el2, 2);

      /*
      * Shell manager initialization.
      */
  shellInit();

  while (true) {
    if (!shelltp) {
#if (HAL_USE_SERIAL_USB == TRUE)
      if (SDU2.config->usbp->state == USB_ACTIVE) {
        /* Spawns a new shell.*/
        shelltp = chThdCreateFromHeap(NULL, SHELL_WA_SIZE, "shell", NORMALPRIO, shellThread, (void *) &shell_cfg1);
      }
#else
        shelltp = chThdCreateFromHeap(NULL, SHELL_WA_SIZE, "shell", NORMALPRIO, shellThread, (void *) &shell_cfg1);
#endif
    }
    else {
      /* If the previous shell exited.*/
      if (chThdTerminatedX(shelltp)) {
        /* Recovers memory of the previous shell.*/
        chThdRelease(shelltp);
        shelltp = NULL;
      }
    }
    chEvtDispatch(evhndl, chEvtWaitOneTimeout(ALL_EVENTS, TIME_MS2I(500)));
   // chThdSleepMilliseconds(500);
  }
}
