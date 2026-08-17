#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include "crypto.h"
#include "speaker.h"

// Jeden przelacznik na pomiary jakosci toru audio: poziom sygnalu z mikrofonu
// (mic.c) oraz przepustowosc i straty pakietow (nRF.c). Kazdy blok loguje raz
// na sekunde i liczy sie w goracej petli, wiec do normalnej pracy dajemy 0.
#define AUDIO_DIAGNOSTICS 0

// Ile pakietow na sekunde musi przejsc, zeby dzwiek plynal bez dziur
#define AUDIO_PACKETS_PER_SEC ( SAMPLE_RATE * (int)sizeof(int16_t) / CRYPTO_PAYLOAD_BYTE_SIZE )

// Surowa przepustowosc w eterze - z naglowkiem pakietu, nie tylko audio
#define AUDIO_LINK_BYTES_PER_SEC ( AUDIO_PACKETS_PER_SEC * (int)sizeof(radio_packet_t) )

#endif
