cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(coincoin C CXX ASM)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()
# rest of your project
add_executable(coincoin
    coincoin.c
    lfs_hal.c
    wave.c
    littlefs/lfs.c
    littlefs/lfs_util.c
)


target_compile_definitions(coincoin PRIVATE PICO_AUDIO_I2S_MONO_INPUT)
# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(coincoin pico_stdlib hardware_flash pico_audio_i2s)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(coincoin)

pico_enable_stdio_usb(coincoin 1)
