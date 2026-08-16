#ifndef nRF_H
#define nRF_H

#include <stdint.h>
#include "esp_err.h"
#include "mirf.h"

#define nRF_PAYLOAD_BYTE_SIZE ( 32 )
// #define nRF_PAYLOAD_BYTE_ALIGNMENT ( (nRF_PAYLOAD_BYTE_SIZE + 7) / 8 )
#define nRF_PAYLOAD_BYTE_ALIGNMENT ( 16 )
#define CHANNEL ( 90 )

#define STREAM_MODE

extern NRF24_t dev;

esp_err_t nRF_init(void);
void nRF_send_data(uint8_t* data, uint32_t byte_length);
void nRF_stream_task(void *arg);

#endif
