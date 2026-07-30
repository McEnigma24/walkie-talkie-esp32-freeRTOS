#ifndef nRF_H
#define nRF_H

#include <stdint.h>
#include "esp_err.h"
#include "mirf.h"

#define TRANSMISSION_PAYLOAD_LENGTH ( 32 )
// #define TRANSMISSION_PAYLOAD_BYTE_ALIGNMENT ( (TRANSMISSION_PAYLOAD_LENGTH + 7) / 8 )
#define TRANSMISSION_PAYLOAD_BYTE_ALIGNMENT ( 16 )
#define CHANNEL ( 90 )

#define STREAM_MODE

extern NRF24_t dev;

esp_err_t nRF_init(void);
void nRF_send_data(uint8_t* data, uint32_t byte_length);
void nRF_stream_task(void *arg);

#endif
