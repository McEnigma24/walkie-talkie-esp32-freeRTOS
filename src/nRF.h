#ifndef nRF_H
#define nRF_H

#include "mirf.h"
#include <stdint.h>

static NRF24_t dev;

#define TRANSMISSION_PAYLOAD_LENGTH ( 32 )
// #define TRANSMISSION_PAYLOAD_BYTE_ALIGNMENT ( (TRANSMISSION_PAYLOAD_LENGTH + 7) / 8 )
#define TRANSMISSION_PAYLOAD_BYTE_ALIGNMENT ( 16 )
#define CHANNEL ( 90 )

#define nRF_CHECK_ERR(call) { \
    esp_err_t ret = (call); \
    if (ret != ESP_OK) { \
        printf("Fatal error %s\n", #call); \
        assert(false); \
    } \
}

static esp_err_t nRF_init(void)
{
    Nrf24_init(&dev);                                  // zwraca void
    // Nrf24_enableNoAckFeature(&dev);
    Nrf24_config(&dev, CHANNEL, TRANSMISSION_PAYLOAD_LENGTH);    // CONFIG_RADIO_CHANNEL
    nRF_CHECK_ERR(Nrf24_setTADDR(&dev, (uint8_t*)"WALK1"));
    nRF_CHECK_ERR(Nrf24_setRADDR(&dev, (uint8_t*)"WALK1"));

    return ESP_OK;
}

static void nRF_send_data(uint8_t* data, uint32_t byte_length)
{
    if(byte_length % TRANSMISSION_PAYLOAD_BYTE_ALIGNMENT != 0)
    {
        printf("Buffer missaligned\n");
        return;
    }

    for(int i = 0; i < byte_length; i += TRANSMISSION_PAYLOAD_LENGTH)
    {
        Nrf24_send(&dev, &data[i]);
        // Nrf24_sendNoAck(&dev, &data[i]);

        bool status = Nrf24_isSend(&dev, 1000);
        printf("Sending data - %d \n", status);
    }
}


#endif