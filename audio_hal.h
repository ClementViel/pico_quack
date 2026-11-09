#ifndef __AUDIO_HAL_H
#define __AUDIO_HAL_H
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#define SAMPLES_PER_BUFFER 2048
#define SINE_FREQ_HZ 440.0f
#define SINE_AMPLITUDE 10000

#define chunk_size 2048
#define DAC_ZERO 1
#define FMT_PCM 1
#define FMT_FLOAT 3

struct audio_buffer_pool *init_audio();
#endif
