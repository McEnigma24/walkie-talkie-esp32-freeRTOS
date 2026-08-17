#include "streams.h"

// Musi byc co najmniej jedno tykniecie, inaczej timeout zaokragli sie do zera
// i taski krecilyby sie bez oddawania CPU (patrz CONFIG_FREERTOS_HZ)
const TickType_t common_timeout = pdMS_TO_TICKS( 5 );

StreamBufferHandle_t mic_to_en_crypto_stream = NULL;
StreamBufferHandle_t en_crypto_to_nRF_transmit_stream = NULL;
StreamBufferHandle_t nRF_receive_to_de_crypto_stream = NULL;
StreamBufferHandle_t de_crypto_to_speaker_stream = NULL;

esp_err_t init_mic_to_en_crypto_stream(void)
{
    mic_to_en_crypto_stream = xStreamBufferCreate(MIC_to_EN_CRYPTO_STREAM_SIZE, MIC_to_EN_CRYPTO_STREAM_TRIGGER);
    return (mic_to_en_crypto_stream != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t init_en_crypto_to_nRF_transmit_stream(void)
{
    en_crypto_to_nRF_transmit_stream = xStreamBufferCreate(EN_CRYPTO_to_nRF_TRANSMIT_STREAM_SIZE, EN_CRYPTO_to_nRF_TRANSMIT_STREAM_TRIGGER);
    return (en_crypto_to_nRF_transmit_stream != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t init_nRF_receive_to_de_crypto_stream(void)
{
    nRF_receive_to_de_crypto_stream = xStreamBufferCreate(nRF_RECEIVE_to_DE_CRYPTO_STREAM_SIZE, nRF_RECEIVE_to_DE_CRYPTO_STREAM_TRIGGER);
    return (nRF_receive_to_de_crypto_stream != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t init_de_crypto_to_speaker_stream(void)
{
    de_crypto_to_speaker_stream = xStreamBufferCreate(DE_CRYPTO_to_SPEAKER_STREAM_SIZE, DE_CRYPTO_to_SPEAKER_STREAM_TRIGGER);
    return (de_crypto_to_speaker_stream != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}
