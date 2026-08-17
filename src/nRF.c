#include "nRF.h"

#include <assert.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "crypto.h"
#include "streams.h"
#include "event_group.h"

_Static_assert(sizeof(radio_packet_t) == nRF_PAYLOAD_BYTE_SIZE, "radio_packet_t musi miec dokladnie nRF_PAYLOAD_BYTE_SIZE bajtow");

NRF24_t dev;

// Mirf trzyma CSN recznie, wiec jedna operacja na radiu to kilka osobnych transakcji SPI.
// Bez tego muteksu task nadawczy i odbiorczy przeplataja sie w srodku takiej sekwencji,
// co wywala assert w spi_device_transmit i rozjezdza protokol nRF.
static SemaphoreHandle_t nRF_mutex;

static void nRF_lock(void)
{
    xSemaphoreTake(nRF_mutex, portMAX_DELAY);
}

static void nRF_unlock(void)
{
    xSemaphoreGive(nRF_mutex);
}

#define nRF_CHECK_ERR(call) { \
    esp_err_t ret = (call); \
    if (ret != ESP_OK) { \
        printf("Fatal error %s\n", #call); \
        assert(false); \
    } \
}

esp_err_t init_nRF(void)
{
    nRF_mutex = xSemaphoreCreateMutex();
    if (nRF_mutex == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    Nrf24_init(&dev);                                  // zwraca void
    #ifdef STREAM_MODE
        Nrf24_enableNoAckFeature(&dev);
    #endif
    Nrf24_config(&dev, CHANNEL, nRF_PAYLOAD_BYTE_SIZE);    // CONFIG_RADIO_CHANNEL
    nRF_CHECK_ERR(Nrf24_setTADDR(&dev, (uint8_t*)"WALK1"));
    nRF_CHECK_ERR(Nrf24_setRADDR(&dev, (uint8_t*)"WALK1"));

    return ESP_OK;
}

void nRF_send_data(uint8_t* data, uint32_t byte_length)
{
    // if(byte_length % nRF_PAYLOAD_BYTE_ALIGNMENT != 0)
    // {
    //     printf("Buffer missaligned\n");
    //     return;
    // }

    nRF_lock();

    for(uint32_t i = 0; i < byte_length; i += nRF_PAYLOAD_BYTE_SIZE)
    {
        #ifdef STREAM_MODE
            Nrf24_sendNoAck(&dev, &data[i]);
        #else
            Nrf24_send(&dev, &data[i]);
        #endif

        // bool status = Nrf24_isSend(&dev, 1000);
        // printf("Sending data - %d \n", status);
    }

    nRF_unlock();
}

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
                &packet_to_send,
                sizeof(packet_to_send),
                common_timeout
            );

            if (got == sizeof(packet_to_send))
            {
                nRF_send_data((uint8_t*)&packet_to_send, sizeof(packet_to_send));
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
            bool got_packet = false;

            nRF_lock();
            // Tryb sprawdzamy pod muteksem: inaczej moglibysmy przestawic uklad
            // na nasluch juz po tym, jak task nadawczy wszedl w TX
            if (isReceiveModeStillActive())
            {
                if (dev.PTX) // nadajnik zostawil uklad w trybie TX - wracamy na nasluch
                {
                    Nrf24_powerUpRx(&dev);
                    Nrf24_flushRx(&dev);
                }
                // getData kasuje RX_DR, wiec przy kilku pakietach w FIFO samo
                // dataReady() zatrzymaloby nas na pierwszym - dobieramy reszte po FIFO
                if (Nrf24_dataReady(&dev) || ! Nrf24_rxFifoEmpty(&dev))
                {
                    Nrf24_getData(&dev, (uint8_t*)&packet_received);
                    got_packet = true;
                }
            }
            nRF_unlock();

            if (! got_packet)
            {
                // Bez tego IDLE nie dostaje czasu i odpala sie task watchdog.
                // Sen musi byc krotszy niz 3 pakiety (~5.6 ms), bo tyle miesci FIFO radia
                // - stad CONFIG_FREERTOS_HZ=1000.
                vTaskDelay(1);
                continue;
            }

            xStreamBufferSend(
                nRF_receive_to_de_crypto_stream,
                (uint8_t*)&packet_received,
                sizeof(packet_received),
                common_timeout
            );
        }
    }
}
