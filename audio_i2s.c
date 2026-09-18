#include <math.h>
#include <stdio.h>

#include "pico/audio_i2s.h"
#include "pico/binary_info.h"
bi_decl(bi_3pins_with_names(PICO_AUDIO_I2S_DATA_PIN, "I2S DIN",
                            PICO_AUDIO_I2S_CLOCK_PIN_BASE, "I2S BCK",
                            PICO_AUDIO_I2S_CLOCK_PIN_BASE + 1, "I2S LRCK"));

#define SAMPLES_PER_BUFFER 256
struct audio_buffer_pool *init_audio() {

  static audio_format_t audio_format = {
      .format = AUDIO_BUFFER_FORMAT_PCM_S16,
      .sample_freq = 24000,
      .channel_count = 1,
  };

  static struct audio_buffer_format producer_format = {.format = &audio_format,
                                                       .sample_stride = 2};

  struct audio_buffer_pool *producer_pool =
      audio_new_producer_pool(&producer_format, 3,
                              SAMPLES_PER_BUFFER); // todo correct size
  bool __unused ok;
  const struct audio_format *output_format;
  struct audio_i2s_config config = {
      .data_pin = PICO_AUDIO_I2S_DATA_PIN,
      .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
      .dma_channel = 0,
      .pio_sm = 0,
  };

  output_format = audio_i2s_setup(&audio_format, &config);
  if (!output_format) {
    panic("PicoAudio: Unable to open audio device.\n");
  }

  ok = audio_i2s_connect(producer_pool);
  assert(ok);
  audio_i2s_set_enabled(true);
  return producer_pool;
}
