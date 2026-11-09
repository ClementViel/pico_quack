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

// audio buffer for DMA transfer
struct audio_buffer_pool *audio_pool;

wav_header_t wav_head;

#define SAMPLES_PER_BUFFER 2048
#define SINE_FREQ_HZ 440.0f
#define SINE_AMPLITUDE 10000

const uint32_t chunk_size = 2048;
const int32_t DAC_ZERO = 1;
const uint16_t FMT_PCM = 1;
const uint16_t FMT_FLOAT = 3;

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
  printf("FS infos: \n");
  printf("type %d\n", infos.type);
  printf("size %d\n", infos.size);
  printf("name %s\n", infos.name);

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
    // play_file();
  }
  return 0;
}

void play_file() {
  static uint8_t file_buffer[2048];
  audio_buffer_t *buffer;
  printf("play\n");
  if ((buffer = take_audio_buffer(audio_pool, false)) == NULL) {
    return;
  }

  int bytes_per_frame = (wav_head.BitsPerSample / 8) * wav_head.NumChannels;
  int aligned_read_size =
      (sizeof(file_buffer) / bytes_per_frame) * bytes_per_frame;
  int read_bytes = lfs_file_read(&lfs, &file, file_buffer, aligned_read_size);
  uint32_t frames_read = read_bytes > 0 ? read_bytes / bytes_per_frame : 0;
  buffer->sample_count = MIN(frames_read, buffer->max_sample_count);
  int16_t *samples = (int16_t *)buffer->buffer->bytes;
  for (uint32_t i = 0; i < buffer->sample_count; i++) {
    int base = i * bytes_per_frame;
    int32_t buf_s32;
    switch ((wav_head.AudioFormat << 8) | wav_head.BitsPerSample) {
    case ((FMT_PCM << 8) | 16):
      buf_s32 = (int32_t)((file_buffer[base + 1] << 24) |
                          (file_buffer[base + 0] << 16));
      break;
    case ((FMT_PCM << 8) | 24):
      buf_s32 = (int32_t)((file_buffer[base + 2] << 24) |
                          (file_buffer[base + 1] << 16) |
                          (file_buffer[base + 0] << 8));
      break;
    case ((FMT_PCM << 8) | 32):
      buf_s32 = (int32_t)((file_buffer[base + 3] << 24) |
                          (file_buffer[base + 2] << 16) |
                          (file_buffer[base + 1] << 8) |
                          (file_buffer[base + 0] << 0));
      break;
    case ((FMT_FLOAT << 8) | 32):
      buf_s32 = 0;
      break;
    default:
      buf_s32 = 0;
      break;
    }
    samples[i] = (int16_t)(((int64_t)buf_s32 / 65536) + DAC_ZERO);
  }
  give_audio_buffer(audio_pool, buffer);
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
