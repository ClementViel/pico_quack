#include "audio_hal.h"
#include "pico/audio_i2s.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"

struct audio_buffer_pool *init_audio() {

  static audio_format_t audio_format = {
      .format = AUDIO_BUFFER_FORMAT_PCM_S16,
      .sample_freq = 44100,
      .channel_count = 1,
  };

  static struct audio_buffer_format producer_format = {.format = &audio_format,
                                                       .sample_stride = 2};

  struct audio_buffer_pool *producer_pool = audio_new_producer_pool(
      &producer_format, 3, SAMPLES_PER_BUFFER); // todo correct size
  bool ok;
  const struct audio_format *output_format;
  struct audio_i2s_config config = {
      .data_pin = 2,
      .clock_pin_base = 3,
      .dma_channel = 0,
      .pio_sm = 0,
  };

  output_format = audio_i2s_setup(&audio_format, &config);
  printf("connecting audio\n");
  if (!output_format) {
    panic("PicoAudio: Unable to open audio device.\n");
  }
  ok = audio_i2s_connect(producer_pool);

  printf("connected audio\n");
  assert(ok);
  {
    audio_buffer_t *ab = take_audio_buffer(producer_pool, true);
    int16_t *samples = (int16_t *)ab->buffer->bytes;
    for (uint i = 0; i < ab->max_sample_count; i++) {
      samples[i] = DAC_ZERO;
    }
    ab->sample_count = ab->max_sample_count;
    give_audio_buffer(producer_pool, ab);
  }
  printf("set audio enabled\n");
  audio_i2s_set_enabled(true);
  printf("audio inited");
  return producer_pool;
}
