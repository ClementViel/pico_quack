cmake_minimum_required(VERSION 3.13...3.27)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(coincoin)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project
add_executable(coincoin
    coincoin.c
    lfs_hal.c
    littlefs/lfs.c
    littlefs/lfs_util.c
)


# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(coincoin pico_stdlib hardware_flash)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(coincoin)
