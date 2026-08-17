#ifndef MIC_H
#define MIC_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

#define MIC_GPIO            36
#define MIC_ADC_UNIT        ADC_UNIT_1
#define MIC_ADC_CHANNEL     ADC_CHANNEL_0
#define MIC_ADC_ATTEN       ADC_ATTEN_DB_12
#define MIC_CALIB_SAMPLES   256
#define MIC_LEVEL_SAMPLES   128
// Wzmocnienie startowe. Dalej dobiera je AGC ponizej - zmierzone szczyty przy
// mowie siegaly ~4000 (delta po odjeciu skladowej stalej), czyli 32767/4000 ~= 8.
#define MIC_GAIN            8

// --- AGC: wzmocnienie dobierane w locie tak, zeby szczyty ladowaly pod sufitem ---
// Wzmocnienie trzymamy w stalym przecinku, bo krok calkowity (8 -> 7) to skok o 12%,
// slyszalny jako szarpniecie glosnosci.
#define MIC_GAIN_FRAC_BITS    ( 8 )
#define MIC_GAIN_ONE          ( 1 << MIC_GAIN_FRAC_BITS )

// Celujemy ponizej 32767, zeby transjent miedzy jednym a drugim blokiem mial gdzie wejsc
#define MIC_AGC_TARGET_PEAK   ( 28'000 )
#define MIC_AGC_GAIN_MIN      ( 1 * MIC_GAIN_ONE )

// Mowa daje szczyty 2000-4000, wiec do celu 28000 wystarcza wzmocnienia 7-14.
// Wyzszy sufit tylko podbijalby szum - wzmocnienie nie poprawia stosunku S/N.
#define MIC_AGC_GAIN_MAX      ( 16 * MIC_GAIN_ONE )

// Zmierzona podloga szumow to szczyty 110-140 na sekunde, mowa startuje od ~2000.
// Prog stawiamy posrodku, z zapasem nad szumem.
#define MIC_AGC_NOISE_FLOOR   ( 250 )

// Ponizej progu wysylamy cisze zamiast wzmocnionego szumu. Wylaczamy z opoznieniem,
// zeby przerwy miedzy sylabami nie tnly wypowiedzi na kawalki.
#define MIC_SQUELCH_HANG_MS   ( 300 )

// Blok ADC to MIC_CONT_FRAME_SIZE/2/MIC_CONT_DECIMATION probek, czyli ~5 ms.
// Ponizsze przesuniecia to ulamek dystansu nadrabiany na kazdy taki blok.

// Za glosno -> schodzimy natychmiast (polowa dystansu, ~18 ms). To jedyna ochrona
// przed obcinaniem, wiec zostaje szybka niezaleznie od reszty ustawien.
#define MIC_AGC_ATTACK_SHIFT  ( 1 )

// Za cicho -> podbijamy 1/16 na blok (~85 ms). Szybko, ale nie na tyle, zeby
// rozkrecac wzmocnienie w przerwach miedzy sylabami.
#define MIC_AGC_RISE_SHIFT    ( 4 )

// Cisza -> przez ten czas nie ruszamy wzmocnienia, zeby pauza w zdaniu nie
// przestawiala poziomu ustalonego na glosie. Kazdy blok z mowa resetuje odliczanie.
#define MIC_AGC_HOLD_MS       ( 5'000 )

// Po wygasnieciu hold wracamy leniwie (~1.4 s) do MIC_GAIN, ale tylko w dol -
// podnoszenie wzmocnienia na ciszy zafundowaloby przester na pierwszym slowie.
#define MIC_AGC_DECAY_SHIFT   ( 8 )

#define MIC_CONT_FRAME_SIZE   ( 512 )
#define MIC_CONT_STORE_SIZE   ( 2048 )

// ESP32 nie obsluguje ADC continuous ponizej SOC_ADC_SAMPLE_FREQ_THRES_LOW (20 kHz),
// wiec probkujemy z nadprobkowaniem i decymujemy do SAMPLE_RATE.
#define MIC_CONT_DECIMATION   ( 4 )

// Krotki timeout, zeby petla nagrywania zauwazyla przejscie z TRANSMIT na RECEIVE
#define MIC_CONT_READ_TIMEOUT_MS  ( 20 )

esp_err_t mic_init(void);
esp_err_t mic_read_raw(int *raw);
esp_err_t mic_record(int16_t *buffer, size_t buffer_bytes, uint32_t duration_ms);
esp_err_t mic_read_samples(int16_t *buffer, size_t num_samples);
int mic_measure_level(void);
int mic_get_offset(void);

esp_err_t init_mic_cont(void);
void TASK_cont_mic_stream(void *arg);

#endif
