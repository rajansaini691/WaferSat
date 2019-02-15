
#include <stdint.h>
#include "hal.h"
#include "dcmi_dma.h"
#include "chprintf.h"


#define DCMI_BASE_ADR        ((uint32_t)0x50050000)
#define DCMI_REG_DR_OFFSET   0x28
#define DCMI_REG_DR_ADDRESS  (DCMI_BASE_ADR + DCMI_REG_DR_OFFSET)

uintptr_t dcmi_dma_buffer = (uintptr_t) NULL;
uintptr_t dcmi_dma_image_size = 0;
uintptr_t dcmi_dma_buffer_size = 0;
size_t dcmi_dma_bytes_received;
stm32_dmaisr_t dcmi_dma_done_callback = NULL;
void* dcmi_dma_done_callback_data = NULL;
stm32_dmaisr_t dcmi_dma_error_callback = NULL;
void* dcmi_dma_error_callback_data;

static const stm32_dma_stream_t * const dma_stream_driver = STM32_DMA2_STREAM1;
static int blocks_done;
static int block_size;
static int num_blocks;

#define dmaStreamClearInterrupt_CompleteOnly(dmastp) { \
  *(dmastp)->ifcr = STM32_DMA_ISR_TCIF << (dmastp)->ishift; \
}

static void dcmi_dma_shuffle_isr(void* p, uint32_t flags)
{
  (void) p;

  //Ignore hafl transfers
    flags &= ~ STM32_DMA_ISR_HTIF;
    if (flags ==0) //If half transfer is only interrupt, then don't care
      return;

   if(flags != STM32_DMA_ISR_TCIF)
   {
      if(dcmi_dma_error_callback != NULL)
        dcmi_dma_error_callback(dcmi_dma_error_callback_data, flags);
      dmaStreamDisable(dma_stream_driver);
   }

   //Have a complete flag and no errors, do some buffer shuffling
    dmaStreamClearInterrupt_CompleteOnly(dma_stream_driver);
    blocks_done++;
    //Now block [0, blocks_done) are done, block[blocks_done] is in being
    //transferred to, and we want to update blocks_done+1

    if(blocks_done == num_blocks)
    {
      //We are in fact done
      if(dcmi_dma_done_callback != NULL)
        dcmi_dma_done_callback(dcmi_dma_done_callback_data, flags);
      dmaStreamDisable(dma_stream_driver);
    }
    //else if (blocks_done == (num_blocks-1))
    // Doesn't let us handle this cleanly, so just overallocate
    else //(blocks_done<= (num_blocks-2))
    {
      uintptr_t block_addr = dcmi_dma_buffer +
               (unsigned) block_size * ((unsigned)blocks_done+1);

           if(dmaStreamGetCurrentTarget(dma_stream_driver) ==0) {
             dmaStreamSetMemory1(dma_stream_driver, block_addr);
           } else {
             dmaStreamSetMemory0(dma_stream_driver, block_addr);
           }

    }
}

static void dcmi_dma_shuffle_isr_open_ended(void* p, uint32_t flags)
{
  (void) p;

  //Ignore hafl transfers
    flags &= ~ STM32_DMA_ISR_HTIF;
    if (flags ==0) //If half transfer is only interrupt, then don't care
      return;

   if(flags != STM32_DMA_ISR_TCIF)
   {
      if(dcmi_dma_error_callback != NULL)
        dcmi_dma_error_callback(dcmi_dma_error_callback_data, flags);
      dmaStreamDisable(dma_stream_driver);
   }

   //Have a complete flag and no errors, do some buffer shuffling
    dmaStreamClearInterrupt_CompleteOnly(dma_stream_driver);
    blocks_done++;
    //Now block [0, blocks_done) are done, block[blocks_done] is in being
    //transferred to, and we want to update blocks_done+1

    if(blocks_done == num_blocks)
    {
      //We are in fact out of buffer, overrun error
      if(dcmi_dma_done_callback != NULL)
        dcmi_dma_error_callback(dcmi_dma_error_callback_data, flags);
      dmaStreamDisable(dma_stream_driver);
    }
    //else if (blocks_done == (num_blocks-1))
    // Doesn't let us handle this cleanly, so just overallocate
    else //(blocks_done<= (num_blocks-2))
    {
      uintptr_t block_addr = dcmi_dma_buffer +
               (unsigned) block_size * ((unsigned)blocks_done+1);

           if(dmaStreamGetCurrentTarget(dma_stream_driver) ==0) {
             dmaStreamSetMemory1(dma_stream_driver, block_addr);
           } else {
             dmaStreamSetMemory0(dma_stream_driver, block_addr);
           }
    }
}

static uint32_t stream_control_register;

int dcmi_dma_init(void)
{
    if ((dcmi_dma_buffer == (uintptr_t) NULL) || (dcmi_dma_image_size== 0)){
      chprintf((BaseSequentialStream *)&SD1, "CAM  > dcmi_dma: failure\r\n");
        return -1;
    }
    //Find appropriate fatoring of size into number of blocks

    for(int new_num_blocks=1; TRUE; new_num_blocks++)
    {
       int block_size_words= dcmi_dma_image_size / new_num_blocks/4;
       if(block_size_words < 256)
         //Give up if only block sizes possible are tiny
         return -1;

       if(4U*block_size_words*new_num_blocks != dcmi_dma_image_size)
         continue;
       if(block_size_words > 65535)
         continue;

       //This split looks good, stop searching
       num_blocks = new_num_blocks;
       block_size = 4*block_size_words;
       break;
    }
    chprintf((BaseSequentialStream *)&SD1, "CAM  > dcmi_dma: %d byte image, transferring as %d blocks of %d bytes\r\n",
             dcmi_dma_image_size, num_blocks, block_size);

    dmaStreamAllocate(dma_stream_driver, 10, (stm32_dmaisr_t)dcmi_dma_shuffle_isr, NULL);
    dmaStreamSetPeripheral(dma_stream_driver, ((uint32_t*)DCMI_REG_DR_ADDRESS));
    dmaStreamSetTransactionSize(dma_stream_driver, block_size/4);
    stream_control_register = STM32_DMA_CR_CHSEL(1) | STM32_DMA_CR_DIR_P2M |
        STM32_DMA_CR_MINC | STM32_DMA_CR_PSIZE_WORD |
        STM32_DMA_CR_MSIZE_WORD | STM32_DMA_CR_MBURST_INCR4 |
        STM32_DMA_CR_PBURST_SINGLE | STM32_DMA_CR_TCIE;

    stream_control_register &= ~STM32_DMA_CR_CT;
    dmaStreamSetMode(dma_stream_driver, stream_control_register);
    dmaStreamSetFIFO(dma_stream_driver, STM32_DMA_FCR_FTH_FULL);

    if(num_blocks >1)
    {
      stream_control_register |= STM32_DMA_CR_DBM;
      dmaStreamSetMode(dma_stream_driver, stream_control_register);
    }
    return 0;
}

int dcmi_dma_init_open_ended(void)
{
    if ((dcmi_dma_buffer == (uintptr_t) NULL) || (dcmi_dma_buffer_size== 0)){
      chprintf((BaseSequentialStream *)&SD1, "CAM  > dcmi_dma: failure\r\n");
        return -1;
    }
    //Find appropriate factoring of size into number of blocks
    int block_size_words=  65534;
    block_size = 4*block_size_words;
    num_blocks = (dcmi_dma_buffer_size) / block_size -1;

    chprintf((BaseSequentialStream *)&SD1, "CAM  > dcmi_dma: %d byte image, transferring as %d blocks of %d bytes\r\n",
             dcmi_dma_buffer_size, num_blocks, block_size);

    dmaStreamAllocate(dma_stream_driver, 10, (stm32_dmaisr_t)dcmi_dma_shuffle_isr_open_ended, NULL);
    dmaStreamSetPeripheral(dma_stream_driver, ((uint32_t*)DCMI_REG_DR_ADDRESS));
    dmaStreamSetTransactionSize(dma_stream_driver, block_size/4);
    stream_control_register = STM32_DMA_CR_CHSEL(1) | STM32_DMA_CR_DIR_P2M |
        STM32_DMA_CR_MINC | STM32_DMA_CR_PSIZE_WORD |
        STM32_DMA_CR_MSIZE_WORD | STM32_DMA_CR_MBURST_INCR4 |
        STM32_DMA_CR_PBURST_SINGLE | STM32_DMA_CR_TCIE;

    stream_control_register &= ~STM32_DMA_CR_CT;
    dmaStreamSetMode(dma_stream_driver, stream_control_register);
    dmaStreamSetFIFO(dma_stream_driver, STM32_DMA_FCR_FTH_FULL);

    if(num_blocks >1)
    {
      stream_control_register |= STM32_DMA_CR_DBM;
      dmaStreamSetMode(dma_stream_driver, stream_control_register);
    }
    return 0;
}

#define dmaStreamGetMemory0(dmastp) ((dmastp)->stream->M0AR)
#define dmaStreamGetMemory1(dmastp) ((dmastp)->stream->M1AR)

void dcmi_dma_open_ended_stop(void)
{
    uintptr_t top_addr;
    dmaStreamDisable(dma_stream_driver); //Which also waits for completion

#ifndef DCMI_DMA_BYTES_RECEIVED_EXTRA_PADDING_MODE
    if(dmaStreamGetCurrentTarget(dma_stream_driver) ==0 )
      top_addr = dmaStreamGetMemory0(dma_stream_driver);
    else // Memory address 1
       top_addr = dmaStreamGetMemory1(dma_stream_driver);
#else
    top_addr = dmaStreamGetMemory0(dma_stream_driver);
    uintptr_t top_addr_alt = dmaStreamGetMemory1(dma_stream_driver);
    if(top_addr_alt > top_addr)
      top_addr = top_addr_alt;
#endif

    dcmi_dma_bytes_received = top_addr - dcmi_dma_buffer;
}

int dcmi_dma_arm(void)
{
    blocks_done =0;
    dmaStreamSetMode(dma_stream_driver, stream_control_register);
    dmaStreamSetMemory0(dma_stream_driver, (uint32_t)dcmi_dma_buffer);
    dmaStreamSetMemory1(dma_stream_driver, (uint32_t)dcmi_dma_buffer + block_size);
    dmaStreamEnable(dma_stream_driver);
    return 0;
}
