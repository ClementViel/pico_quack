#include "audio_hal.h"
#include "hid-passthrough/hid_passthrough.h"
#include "lfs_hal.h"
#include "littlefs/lfs.h"
#include "pico/audio_i2s.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "wave.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <math.h>
// variables used by the filesystem
lfs_t lfs;
lfs_file_t file;

wav_header_t wav_head;

#define SAMPLES_PER_BUFFER 2048
#define SINE_FREQ_HZ 440.0f
#define SINE_AMPLITUDE 10000

static uint16_t format; // 1: PCM, 3: IEEE float

void play_file();
void play_sine();
void play_silence();

struct lfs_config cfg = {
    // block device operations
    .read = pico_read_flash_block,
    .prog = pico_prog_flash_block,
    .erase = pico_erase_flash_block,
    .sync = pico_sync_flash_block,

    // block device configuration
    .read_size = 16,
    .prog_size = 16,
    .block_size = 4096,
    .block_count = 384,
    .cache_size = 16,
    .lookahead_size = 16,
    .block_cycles = 500,
};

struct audio_buffer_pool *audio_pool;

int main() {
  struct lfs_info infos;
  int ret;
  int16_t *pcm_buffer;
  uint8_t vol = 127;

  stdio_init_all();
  sleep_ms(2000);

  uint32_t step = 0x100000;
  uint32_t pos = 0;
  int len = 0;

  int err = lfs_mount(&lfs, &cfg);
  if (err) {
    printf("error mounting lfs\n");
    lfs_format(&lfs, &cfg);
    lfs_mount(&lfs, &cfg);
  }
  // init_hid_passthrough_and_task();
  lfs_stat(&lfs, "/test_quack.wav", &infos);

  ret = lfs_file_open(&lfs, &file, "/test_quack.wav", LFS_O_RDONLY);
  if (ret < 0) {
    printf("error opening file \n");
    return -1;
  }

  len = lfs_file_read(&lfs, &file, (void *)&wav_head, sizeof(wav_header_t));
  printf("file pos = %d\n", lfs_file_tell(&lfs, &file));
  if (!len) {
    printf("cannot read wav header from file");
    lfs_file_close(&lfs, &file);
    return -1;
  }

  if (NULL == strstr((char *)wav_head.Subchunk1ID, "fmt") &&
      NULL == strstr((char *)wav_head.Subchunk2ID, "data")) {
    printf("error format wav header");
    lfs_file_close(&lfs, &file);
    return -1;
  }

  printf("sample rate = %d\n", wav_head.SampleRate);
  printf("bits per sample = %d\n", wav_head.BitsPerSample);
  printf("byterate = %d\n", wav_head.ByteRate);
  printf("chunksize = %d\n", wav_head.ChunkSize);
  printf("samplesize = %d\n", wav_head.ChunkSize / wav_head.SampleRate);

  audio_pool = init_audio();
  printf("audio inited\n");
  sleep_ms(1000);
  while (true) {
    play_wave_file(&lfs, &file, audio_pool, wav_head);
  }
  return 0;
}

// Hardware diagnostic: plays a pure tone instead of the WAV file, to tell
// whether noise is coming from WAV decoding or from the I2S/DMA/amp path.
void play_sine() {
  static float phase = 0.0f;
  const float phase_inc = 2.0f * (float)M_PI * SINE_FREQ_HZ / 44100.0f;

  audio_buffer_t *buffer;
  if ((buffer = take_audio_buffer(audio_pool, false)) == NULL) {
    return;
  }

  int16_t *samples = (int16_t *)buffer->buffer->bytes;
  buffer->sample_count = buffer->max_sample_count;
  for (uint32_t i = 0; i < buffer->sample_count; i++) {
    samples[i] = (int16_t)(SINE_AMPLITUDE * sinf(phase));
    phase += phase_inc;
    if (phase >= 2.0f * (float)M_PI) {
      phase -= 2.0f * (float)M_PI;
    }
  }
  give_audio_buffer(audio_pool, buffer);
}

// Hardware diagnostic: commands constant zero output (no tone, no per-sample
// computation at all). If this is still noisy, the noise is independent of
// any data/software and must be clock/power/ground; if it's clean while the
// sine isn't, the noise is signal-dependent (e.g. supply droop under load).
void play_silence() {
  audio_buffer_t *buffer;
  if ((buffer = take_audio_buffer(audio_pool, false)) == NULL) {
    return;
  }

  int16_t *samples = (int16_t *)buffer->buffer->bytes;
  buffer->sample_count = buffer->max_sample_count;
  for (uint32_t i = 0; i < buffer->sample_count; i++) {
    samples[i] = 0;
  }
  give_audio_buffer(audio_pool, buffer);
}

void i2s_callback_func() { play_file(); }
