#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define CRYPTO_PAYLOAD_BYTE_SIZE ( 30 )

// Struktura dokładnie odwzorowująca to, co leci w eter
typedef struct __attribute__((packed))
{
    uint16_t sequence_number;
    uint8_t  encrypted_audio[CRYPTO_PAYLOAD_BYTE_SIZE];
} radio_packet_t;

esp_err_t init_crypto(void);

bool en_crypto_audio_packet(uint8_t* IN_raw_audio, radio_packet_t* OUT_packet_to_send);
bool decode_radio_packet(radio_packet_t* IN_received_packet, uint8_t* OUT_raw_audio);

// MIC -> EN_CRYPTO -> nRF Transmit //
void TASK_crypto_en_crypto(void *arg);

// nRF Receive -> DE_CRYPTO -> SPEAKER //
void TASK_crypto_de_crypto(void *arg);

#endif
