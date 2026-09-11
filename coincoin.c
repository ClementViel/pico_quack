#include "lfs_hal.h"
#include "littlefs/lfs.h"
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
    .block_count = 384,
    .cache_size = 16,
    .lookahead_size = 16,
    .block_cycles = 500,
};

int main() {
  struct lfs_info infos;
  struct lfs_file file;
  char buffer[10];
  int ret;
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
  if (ret >= 0) {

    printf("file opened \n");
    if (lfs_file_read(&lfs, &file, (void *)buffer, 5)) {
      printf("file content %s\n", buffer);
    } else {
      printf("err reading file\n");
    }
  } else {
    printf("err opening file %d\n", ret);
  }

  while (true) {
    // mount the filesystem
    sleep_ms(2000);
  }

  return 0;
}
