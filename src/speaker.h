#ifndef SPEAKER_H
#define SPEAKER_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define I2S_WS    25   // LRC   → pin "25"
#define I2S_BCLK  26   // BCLK  → pin "26"
#define I2S_DOUT  27   // DIN   → pin "27"

#define SAMPLE_RATE     16'000
#define TONE_HZ         440
#define AMPLITUDE       6000
#define BUFFER_SAMPLES  256
#define RECORD_SEC      2
#define RECORD_SAMPLES  (SAMPLE_RATE * RECORD_SEC)
#define PLAYBACK_VOLUME_PERCENT  30
#define SILENCE_FLUSH_BUFFERS    4

esp_err_t init_speaker(void);
esp_err_t speaker_stream_begin(void);
esp_err_t speaker_stream_end(void);
esp_err_t speaker_stream_write(const int16_t *mono, size_t num_samples);
esp_err_t speaker_play(const int16_t *mono, size_t buffer_bytes);
void play_tone(void);

void TASK_speaker(void *arg);

#endif
