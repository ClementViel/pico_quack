#include "wave.h"
#include "audio_hal.h"
#include "lfs_hal.h"
#include "pico/audio_i2s.h"
#include <stdint.h>

/**
 * Id formatting
 */

#define __WAVE_ID(s)                                                           \
  (uint32_t)(s[0] | (s[1] << 8) | (s[2] << 16) | (s[3] << 24))

/**
 * File format statement
 * | type_id     WAVE_FILE_TYPE_ID
 * | size        File size - 8 bytes
 * | type_id     WAVE_FILE_FMT_ID
 */

#define WAVE_FILE_TYPE_ID __WAVE_ID("RIFF")
#define WAVE_FILE_FMT_ID __WAVE_ID("WAVE")

struct wave_file {
  uint32_t type_id;
  uint32_t size;
  uint32_t fmt_id;
};

/**
 * Audio format statement
 * | id          WAVE_FORMAT_ID
 * | size        Size of the block - 8 bytes (= 16 bytes)
 * | format      WAVE_FORMAT_PCM or WAVE_FORMAT_EXT
 * | channels    Number of channels
 * | samplerate  Sampling rate
 * | byterate    Bytes per secondes = `samplerate * framesize`
 * | framesize   Bytes per sampling time = `channels * bitdepth / 8`
 * | bitdepth    Number of bits per sample
 */

#define WAVE_FORMAT_ID __WAVE_ID("fmt ")
#define WAVE_FORMAT_PCM 0x0001
#define WAVE_FORMAT_EXT 0xfffe

struct wave_format {
  uint32_t id;
  uint32_t size;
  uint16_t fmt;
  uint16_t channels;
  uint32_t samplerate;
  uint32_t byterate;
  uint16_t framesize;
  uint16_t bitdepth;
};

/**
 * Audio data statement
 * | id          WAV_DATA_ID
 * | size        Size of the data following
 */

#define WAVE_DATA_ID __WAVE_ID("data")

struct wave_data {
  uint32_t id;
  uint32_t size;
};

/**
 * Read WAVE file header
 */
int wave_read_header(FILE *fp, int *bitdepth, int *samplesize, int *samplerate,
                     int *nchannels, int *nframes) {
  struct wave_file file;
  struct wave_format format;
  struct wave_data data;

  if (fread(&file, sizeof(file), 1, fp) != 1 ||
      file.type_id != WAVE_FILE_TYPE_ID || file.fmt_id != WAVE_FILE_FMT_ID)
    return -1;

  if (fread(&format, sizeof(format), 1, fp) != 1 ||
      format.id != WAVE_FORMAT_ID ||
      (format.fmt != WAVE_FORMAT_PCM && format.fmt != WAVE_FORMAT_EXT) ||
      format.channels <= 0 || format.samplerate <= 0 || format.framesize <= 0 ||
      format.byterate != format.samplerate * format.framesize)
    return -1;

  fseek(fp, sizeof(format) - (8 + format.size), SEEK_CUR);

  for (; fread(&data, sizeof(data), 1, fp) == 1 && data.id != WAVE_DATA_ID;
       fseek(fp, data.size, SEEK_CUR))
    ;

  if (feof(fp))
    return -1;

  *bitdepth = format.bitdepth;
  *samplesize = format.framesize / format.channels;
  *samplerate = format.samplerate;
  *nchannels = format.channels;
  *nframes = data.size / format.framesize;

  return 0;
}

/**
 * Read PCM samples from wave file
 */
int wave_read_pcm(FILE *fp, int samplesize, int nch, int count, void *buffer) {
  return fread(buffer, nch * samplesize, count, fp);
}

/**
 * Write WAVE file header
 */
void wave_write_header(FILE *fp, int bitdepth, int samplesize, int samplerate,
                       int nchannels, int nframes) {
  struct {
    struct wave_file file;
    struct wave_format format;
    struct wave_data data;
  } header;

  long data_size = nchannels * nframes * samplesize;
  long file_size = sizeof(header) + data_size;

  header.file = (struct wave_file){WAVE_FILE_TYPE_ID, file_size - 8,
                                   .fmt_id = WAVE_FILE_FMT_ID};

  header.format = (struct wave_format){
      WAVE_FORMAT_ID,
      sizeof(header.format) - 8,
      .fmt = WAVE_FORMAT_PCM,
      .channels = nchannels,
      .samplerate = samplerate,
      .byterate = samplerate * nchannels * samplesize,
      .framesize = nchannels * samplesize,
      .bitdepth = bitdepth,
  };

  header.data = (struct wave_data){WAVE_DATA_ID, data_size};

  fwrite(&header, sizeof(header), 1, fp);
}

/**
 * Write PCM samples to wave file
 */
void wave_write_pcm(FILE *fp, int samplesize, const void *_pcm, int nch,
                    int off, int count) {
  const int8_t *pcm = _pcm;
  fwrite(pcm + nch * off * samplesize, nch * samplesize, count, fp);
}

void play_wave_file(lfs_t *fs, lfs_file_t *file,
                    struct audio_buffer_pool *buffer_pool,
                    wav_header_t header) {
  static uint8_t file_buffer[2048];
  audio_buffer_t *buffer;
  printf("play\n");
  if ((buffer = take_audio_buffer(buffer_pool, false)) == NULL) {
    return;
  }

  int bytes_per_frame = (header.BitsPerSample / 8) * header.NumChannels;
  int aligned_read_size =
      (sizeof(file_buffer) / bytes_per_frame) * bytes_per_frame;
  int read_bytes = lfs_file_read(fs, file, file_buffer, aligned_read_size);
  uint32_t frames_read = read_bytes > 0 ? read_bytes / bytes_per_frame : 0;
  buffer->sample_count = MIN(frames_read, buffer->max_sample_count);
  int16_t *samples = (int16_t *)buffer->buffer->bytes;
  for (uint32_t i = 0; i < buffer->sample_count; i++) {
    int base = i * bytes_per_frame;
    int32_t buf_s32;
    switch ((header.AudioFormat << 8) | header.BitsPerSample) {
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
  give_audio_buffer(buffer_pool, buffer);
}
