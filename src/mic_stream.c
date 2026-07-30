#include "mic_stream.h"

#include <stddef.h>

StreamBufferHandle_t audio_stream;

esp_err_t mic_stream_init(void)
{
    audio_stream = xStreamBufferCreate(AUDIO_STREAM_SIZE, AUDIO_STREAM_TRIGGER);
    return (audio_stream != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}
