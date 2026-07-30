#include "nRF.h"

#include <assert.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "mic_stream.h"

NRF24_t dev;

#define nRF_CHECK_ERR(call) { \
    esp_err_t ret = (call); \
    if (ret != ESP_OK) { \
        printf("Fatal error %s\n", #call); \
        assert(false); \
    } \
}

esp_err_t nRF_init(void)
{
    Nrf24_init(&dev);                                  // zwraca void
    #ifdef STREAM_MODE
        Nrf24_enableNoAckFeature(&dev);
    #endif
    Nrf24_config(&dev, CHANNEL, TRANSMISSION_PAYLOAD_LENGTH);    // CONFIG_RADIO_CHANNEL
    nRF_CHECK_ERR(Nrf24_setTADDR(&dev, (uint8_t*)"WALK1"));
    nRF_CHECK_ERR(Nrf24_setRADDR(&dev, (uint8_t*)"WALK1"));

    return ESP_OK;
}

void nRF_send_data(uint8_t* data, uint32_t byte_length)
{
    // if(byte_length % TRANSMISSION_PAYLOAD_BYTE_ALIGNMENT != 0)
    // {
    //     printf("Buffer missaligned\n");
    //     return;
    // }

    for(int i = 0; i < byte_length; i += TRANSMISSION_PAYLOAD_LENGTH)
    {
        #ifdef STREAM_MODE
            Nrf24_sendNoAck(&dev, &data[i]);
        #else
            Nrf24_send(&dev, &data[i]);
        #endif

        // bool status = Nrf24_isSend(&dev, 1000);
        // printf("Sending data - %d \n", status);
    }
}

void nRF_stream_task(void *arg)
{
    (void)arg;
    uint8_t packet[TRANSMISSION_PAYLOAD_LENGTH];

    while (1)
    {
        // blocked on Stream until full 32 bytes are ready
        size_t got = xStreamBufferReceive(
            audio_stream,
            packet,
            TRANSMISSION_PAYLOAD_LENGTH,
            portMAX_DELAY
        );

        if (got == TRANSMISSION_PAYLOAD_LENGTH)
        {
            nRF_send_data(packet, TRANSMISSION_PAYLOAD_LENGTH);
        }
    }
}
