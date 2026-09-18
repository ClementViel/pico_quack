#include "audio_i2s.h"
#include "lfs_hal.h"
#include "littlefs/lfs.h"
#include "pico/audio_i2s.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "wave.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

// variables used by the filesystem
lfs_t lfs;
lfs_file_t file;

const uint32_t chunk_size = 4096;

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

int main() {
  struct lfs_info infos;
  struct lfs_file file;
  int ret;
  uint8_t *buffer = malloc(chunk_size);
  wav_header_t wav_head;
  uint8_t vol = 127;

  struct audio_buffer_pool *audio_pool = init_audio();
  struct audio_buffer *audio_buf = take_audio_buffer(audio_pool, true);

  uint32_t step = 0x200000;
  uint32_t pos = 0;
  uint32_t pos_max = 0;

  stdio_init_all();
  int err = lfs_mount(&lfs, &cfg);
  if (err) {
    printf("error mounting lfs\n");
    lfs_format(&lfs, &cfg);
    lfs_mount(&lfs, &cfg);
  }

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

  int len = lfs_file_read(&lfs, &file, (void *)buffer, sizeof(wav_header_t));
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

  pos_max = 0x1000 * len;

  int16_t *samples = (int16_t *)audio_buf->buffer->bytes;
  for (uint i = 0; i < audio_buf->max_sample_count; i++) {
    samples[i] = (vol * buffer[pos >> 16u]) >> 8u;
    pos += step;
    if (pos >= pos_max)
      pos -= pos_max;
  }
  audio_buf->sample_count = audio_buf->max_sample_count;
  give_audio_buffer(audio_pool, audio_buf);
  while (true) {
    // mount the filesystem
    sleep_ms(2000);
  }

  return 0;
}
