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
#define MIC_GAIN            35

#define MIC_CONT_FRAME_SIZE   ( 512 )
#define MIC_CONT_STORE_SIZE   ( 2048 )

esp_err_t mic_init(void);
esp_err_t mic_read_raw(int *raw);
esp_err_t mic_record(int16_t *buffer, size_t buffer_bytes, uint32_t duration_ms);
esp_err_t mic_read_samples(int16_t *buffer, size_t num_samples);
int mic_measure_level(void);
int mic_get_offset(void);

esp_err_t init_mic_cont(void);
void TASK_cont_mic_stream(void *arg);

#endif
