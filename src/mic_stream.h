#ifndef MIC_STREAM_H
#define MIC_STREAM_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"

#define AUDIO_STREAM_SIZE      ( 4096 )
#define AUDIO_STREAM_TRIGGER   ( 32 )

extern StreamBufferHandle_t audio_stream;

esp_err_t mic_stream_init(void);

#endif
