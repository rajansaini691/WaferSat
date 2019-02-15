#pragma once

#include <stdint.h>

//Please over-allocate buffer by 256k for reasons. Include this overrun in
//dcmi_dma_buffer size if doing open ended
extern uintptr_t dcmi_dma_buffer;
extern uintptr_t dcmi_dma_image_size; //for non open-ended
extern uintptr_t dcmi_dma_buffer_size; //for open-ended

#define DCMI_DMA_BYTES_RECEIVED_EXTRA_PADDING_MODE
extern size_t dcmi_dma_bytes_received; //for open-ened

extern stm32_dmaisr_t dcmi_dma_done_callback;
extern void* dcmi_dma_done_callback_data;
extern stm32_dmaisr_t dcmi_dma_error_callback;
extern void* dcmi_dma_error_callback_data;


#ifdef __cplusplus
extern "C" {
#endif
int dcmi_dma_init(void);
int dcmi_dma_init_open_ended(void);
int dcmi_dma_arm(void);
void dcmi_dma_open_ended_stop(void);

#ifdef __cplusplus
};
#endif
