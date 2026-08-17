#ifndef MIC_STREAM_H
#define MIC_STREAM_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include <stddef.h>



// size_t got = xStreamBufferReceive(
//     mic_to_en_crypto_stream,
//     packet,
//     nRF_PAYLOAD_BYTE_SIZE,
//     common_timeout
// );

// xStreamBufferSend(
//     mic_to_en_crypto_stream,
//     pcm,
//     n * sizeof(int16_t),
//     common_timeout
// );



extern const TickType_t common_timeout = pdMS_TO_TICKS( 1 );

// TRANSMIT - streams //             MIC -> EN_CRYPTO -> nRF Transmit


/////////////////////////////////////////////////////////////////

#define MIC_to_EN_CRYPTO_STREAM_SIZE      ( 4'096 )
#define MIC_to_EN_CRYPTO_STREAM_TRIGGER   ( 30 )

extern StreamBufferHandle_t mic_to_en_crypto_stream;
esp_err_t init_mic_to_en_crypto_stream(void)
{
    mic_to_en_crypto_stream = xStreamBufferCreate(MIC_to_EN_CRYPTO_STREAM_SIZE, MIC_to_EN_CRYPTO_STREAM_TRIGGER);
    return (mic_to_en_crypto_stream != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

/////////////////////////////////////////////////////////////////

#define EN_CRYPTO_to_nRF_TRANSMIT_STREAM_SIZE      ( 4'096 )
#define EN_CRYPTO_to_nRF_TRANSMIT_STREAM_TRIGGER   ( 32 )

extern StreamBufferHandle_t en_crypto_to_nRF_transmit_stream;
esp_err_t init_en_crypto_to_nRF_transmit_stream(void)
{
    en_crypto_to_nRF_transmit_stream = xStreamBufferCreate(EN_CRYPTO_to_nRF_TRANSMIT_STREAM_SIZE, EN_CRYPTO_to_nRF_TRANSMIT_STREAM_TRIGGER);
    return (en_crypto_to_nRF_transmit_stream != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

/////////////////////////////////////////////////////////////////





// RECEIVE - streams //              nRF Receive -> DE_CRYPTO -> SPEAKER


/////////////////////////////////////////////////////////////////

#define nRF_RECEIVE_to_DE_CRYPTO_STREAM_SIZE      ( 4'096 )
#define nRF_RECEIVE_to_DE_CRYPTO_STREAM_TRIGGER   ( 32 )

extern StreamBufferHandle_t nRF_receive_to_de_crypto_stream;
esp_err_t init_nRF_receive_to_de_crypto_stream(void)
{
    nRF_receive_to_de_crypto_stream = xStreamBufferCreate(nRF_RECEIVE_to_DE_CRYPTO_STREAM_SIZE, nRF_RECEIVE_to_DE_CRYPTO_STREAM_TRIGGER);
    return (nRF_receive_to_de_crypto_stream != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

/////////////////////////////////////////////////////////////////

#define DE_CRYPTO_to_SPEAKER_STREAM_SIZE      ( 4'096 )
#define DE_CRYPTO_to_SPEAKER_STREAM_TRIGGER   ( 30 )

extern StreamBufferHandle_t de_crypto_to_speaker_stream;
esp_err_t init_de_crypto_to_speaker_stream(void)
{
    de_crypto_to_speaker_stream = xStreamBufferCreate(DE_CRYPTO_to_SPEAKER_STREAM_SIZE, DE_CRYPTO_to_SPEAKER_STREAM_TRIGGER);
    return (de_crypto_to_speaker_stream != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

/////////////////////////////////////////////////////////////////





#endif
