#include "littlefs/lfs.h"
#include "littlefs/lfs_hal.h"
#include "pico/stdlib.h"
#include <stdio.h>

// variables used by the filesystem
lfs_t lfs;
lfs_file_t file;

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
    .block_count = 128,
    .cache_size = 16,
    .lookahead_size = 16,
    .block_cycles = 500,
};

int main() {
  stdio_init_all();
  printf("hello world\n");
  // mount the filesystem
  int err = lfs_mount(&lfs, &cfg);
  if (err) {
    printf("error mounting lfs");
    lfs_format(&lfs, &cfg);
    lfs_mount(&lfs, &cfg);
  }

  return 0;
}
