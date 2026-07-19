#ifndef MIC_STREAM_H
#define MIC_STREAM_H

#include <stdint.h>
#include <stdlib.h>
#include "esp_err.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"

#define AUDIO_STREAM_SIZE      ( 4096 )
#define AUDIO_STREAM_TRIGGER   ( 32 )

static StreamBufferHandle_t audio_stream;

static esp_err_t mic_stream_init(void)
{
    audio_stream = xStreamBufferCreate(AUDIO_STREAM_SIZE, AUDIO_STREAM_TRIGGER);
    return (audio_stream != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}


#endif