#ifndef nRF_H
#define nRF_H

#include <stdint.h>
#include "esp_err.h"
#include "mirf.h"
#include "streams.h"
#include "event_group.h"

#define nRF_PAYLOAD_BYTE_SIZE ( 32 )
// #define nRF_PAYLOAD_BYTE_ALIGNMENT ( (nRF_PAYLOAD_BYTE_SIZE + 7) / 8 )
#define nRF_PAYLOAD_BYTE_ALIGNMENT ( 16 )
#define CHANNEL ( 90 )

#define STREAM_MODE

extern NRF24_t dev;

esp_err_t init_nRF(void);
void nRF_send_data(uint8_t* data, uint32_t byte_length);



void TASK_nRF_send(void *arg)
{
    (void)arg;
    radio_packet_t packet_to_send;

    while(1)
    {
        blockWaitForTransmitMode();

        while(isTransmitModeStillActive())
        {
            size_t got = xStreamBufferReceive(
                en_crypto_to_nRF_transmit_stream,
                packet_to_send,
                nRF_PAYLOAD_BYTE_SIZE,
                common_timeout
            );

            if (got == nRF_PAYLOAD_BYTE_SIZE)
            {
                nRF_send_data(packet_to_send, nRF_PAYLOAD_BYTE_SIZE);
            }
        }
    }
}

void TASK_nRF_receive(void *arg)
{
    (void)arg;
    radio_packet_t packet_received;

    while(1)
    {
        blockWaitForReceiveMode();

        while(isReceiveModeStillActive())
        {
            if (! Nrf24_dataReady(&dev))
            {
                continue;
            }

            Nrf24_getData(&dev, (uint8_t*)&packet_received);

            xStreamBufferSend(
                nRF_receive_to_de_crypto_stream,
                (uint8_t*)&packet_received,
                sizeof(radio_packet_t),
                common_timeout
            );
        }
    }
}



#endif
