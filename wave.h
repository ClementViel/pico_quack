/******************************************************************************
 *
 *  Copyright 2022 Google LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#ifndef __WAVE_H
#define __WAVE_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
  // The "RIFF" chunk descriptor
  uint8_t ChunkID[4];
  int32_t ChunkSize;
  uint8_t Format[4];
  // The "fmt" sub-chunk
  uint8_t Subchunk1ID[4];
  int32_t Subchunk1Size;
  int16_t AudioFormat;
  int16_t NumChannels;
  int32_t SampleRate;
  int32_t ByteRate;
  int16_t BlockAlign;
  int16_t BitsPerSample;
  // The "data" sub-chunk
  uint8_t Subchunk2ID[4];
  int32_t Subchunk2Size;
} wav_header_t;

/**
 * Read WAVE file header
 * fp              Opened file, moved after header on return
 * bitdepth        Return bitdepth
 * samplesize      Return size of samples, in bytes
 * samplerate      Return samplerate, in Hz
 * nchannels       Return number of channels
 * nframes         Return count of frames
 * return          0: Ok  -1: Bad or unsupported WAVE File
 */
int wave_read_header(FILE *fp, int *bitdepth, int *samplesize, int *samplerate,
                     int *nchannels, int *nframes);

/**
 * Read PCM samples from wave file
 * fp              Opened file
 * samplesize      Size of samples, in bytes
 * nch, count      Number of channels and count of frames to read
 * buffer          Output buffer of `nchannels * count` interleaved samples
 * return          Number of frames read
 */
int wave_read_pcm(FILE *fp, int samplesize, int nch, int count, void *_buffer);

/**
 * Write WAVE file header
 * fp              Opened file, moved after header on return
 * bitdepth        Bitdepth
 * samplesize      Size of samples
 * samplerate      Samplerate, in Hz
 * nchannels       Number of channels
 * nframes         Count of frames
 */
void wave_write_header(FILE *fp, int bitdepth, int samplesize, int samplerate,
                       int nchannels, int nframes);

/**
 * Write PCM samples to wave file
 * fp              Opened file
 * samplesize      Size of samples, in bytes
 * pcm, nch        PCM frames, as 'nch' interleaved samples
 * off, count      Offset and count of frames
 */
void wave_write_pcm(FILE *fp, int samplesize, const void *pcm, int nch, int off,
                    int count);

#endif /* __WAVE_H */
