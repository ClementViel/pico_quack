#include <string.h>

#include "lfs_hal.h"

#include <hardware/flash.h>
#include <pico/flash.h>

// Defines one region of flash to use for a filesystem. The size is a multiple
// of the 4096 byte erase size. We calculate it's location working back from the
// end of the flash device, so that code flashed at the start of the device will
// not collide. Pico's have a 2Mb flash device, so we're looking to be less than
// 2Mb.

// 128 blocks will reserve a 512K filsystem - 1/4 of the 2Mb device on a Pico

#define FLASHFS_BLOCK_COUNT 128
#define FLASHFS_SIZE_BYTES (PICO_ERASE_PAGE_SIZE * FLASHFS_BLOCK_COUNT)

// Flash can be addressed at several different aliased addresses. We use the
// base address to document the location, and for addresses in the UF2 file
// format. On the actual pico we will calculate a different base address which
// will use an alias that gives us the caching performance we want. A start
// location counted back from the end of the device.
#define FLASHFS_BASE_ADDR                                                      \
  (XIP_MAIN_BASE + PICO_FLASH_SIZE_BYTES - FLASHFS_SIZE_BYTES)

// littlefs hal for Pico. Assumes only one filesystem will be present in the
// device, unlike host-uf2 alongside, which can potentially write multiple
// filesystems to different areas of the flash device.

// We need an offset in bytes for erase and program operations.
#define FLASHFS_FLASH_OFFSET ((const size_t)(FLASHFS_BASE_ADDR - XIP_MAIN_BASE))

uint32_t flash_device_offset(uint32_t block, uint32_t offset) {
  return FLASHFS_FLASH_OFFSET + block * PICO_ERASE_PAGE_SIZE + offset;
}

uint32_t get_flash_offset(void) { return FLASHFS_FLASH_OFFSET; }

uint32_t get_flash_base(void) { return FLASHFS_BASE_ADDR; }
uint32_t get_flash_size(void) { return FLASHFS_SIZE_BYTES; }

/*
 * Read from the flash device. Pico's flash is memory mapped, so memcpy will
 * work well.
 *
 * Pico's flash device appears at XIP_MAIN_BASE, and reads are identical to
 * reading from memory. The flash device has a cache, and the device is mapped
 * to 4 different locations in the address map. Which address range you use,
 * determines the cache behaviour. For littlefs, which has it's own cache in
 * RAM, we use the alias (XIP_NOCACHE_NOALLOC_BASE) that skips the cache
 * entirely This also means we don't have to do cache maintenance when we write
 * to the flash device.
 */
int pico_read_flash_block(const struct lfs_config *c, lfs_block_t block,
                          lfs_off_t off, void *buffer, lfs_size_t size) {

  uint32_t offset = flash_device_offset(block, off);
  uint32_t fsAddress = XIP_NOCACHE_NOALLOC_BASE + offset;

  memcpy(buffer, (const uint8_t *)fsAddress, size);
  return LFS_ERR_OK;
}

/*
 * The Pico SDK provides flash_safe_execute which can be used to wrap write
 * (erase/program) operations to the flash device. The implementation varies,
 * depending on your target configuration. It will disable interupts, and pause
 * code execution on core 1 if needed.
 *
 * Using it needs the underlying functions to be passed parameters through a
 * single data pointer, hence the need for a couple of structures to pass them.
 */

struct prog_param {
  uint32_t flash_offs;
  const uint8_t *data;
  size_t count;
};

static void call_flash_range_program(void *param) {
  struct prog_param *p = (struct prog_param *)param;
  flash_range_program(p->flash_offs, p->data, p->count);

  // if using cached memory addresses for read, we would need to
  // remove cache references to this range here.
}

/*
 * Write (program) a block of data to the flash device.
 */
int pico_prog_flash_block(const struct lfs_config *c, lfs_block_t block,
                          lfs_off_t off, const void *buffer, lfs_size_t size) {

  struct prog_param p = {.data = buffer, .count = size};
  p.flash_offs = flash_device_offset(block, off);

  int rc = flash_safe_execute(call_flash_range_program, &p, UINT32_MAX);
  if (rc == PICO_OK) {
    return LFS_ERR_OK;
  } else {
    return LFS_ERR_IO;
  }
}

// we only need to pass one parameter, so cast it into the pointer.
static void call_flash_range_erase(void *param) {
  uint32_t offset = (uint32_t)param;
  flash_range_erase(offset, PICO_ERASE_PAGE_SIZE);

  // if using cached memory addresses for read, we would need to
  // remove cache references to this range here.
}

/*
 * Erase a block of flash, in preparation for future writes
 */
int pico_erase_flash_block(const struct lfs_config *c, lfs_block_t block) {

  uint32_t offset = flash_device_offset(block, 0);

  int rc =
      flash_safe_execute(call_flash_range_erase, (void *)offset, UINT32_MAX);
  if (rc == PICO_OK) {
    return LFS_ERR_OK;
  } else {
    return LFS_ERR_IO;
  }
}

/*
 * Our writes appear to be atomic without the need for a second 'flush' to
 * complete any pending writes.
 */
int pico_sync_flash_block(const struct lfs_config *c) { return LFS_ERR_OK; }
