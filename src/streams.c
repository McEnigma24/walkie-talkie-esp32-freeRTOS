#include "streams.h"

#include <stddef.h>

StreamBufferHandle_t mic_to_en_crypto_stream;

esp_err_t mic_to_en_crypto_stream_init(void)
{
    mic_to_en_crypto_stream = xStreamBufferCreate(MIC_to_EN_CRYPTO_STREAM_SIZE, MIC_to_EN_CRYPTO_STREAM_TRIGGER);
    return (mic_to_en_crypto_stream != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}
