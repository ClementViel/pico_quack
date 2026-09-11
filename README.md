# Pico quack

## Prerequisite

This project needs to have pico-sdk installed : https://github.com/raspberrypi/pico-sdk

## Fetch & Build
```git submodule update --recursive```
Build mklittlefs : https://github.com/earlephilhower/mklittlefs

```cmake --build build --target coincoin```
this creates a coincoin.uf2 file in build

```mklittlefs/mklittlefs -c data/ -p 256 -b 4096 -s 1572864  fs.bin```
This generates a 1.5MB LFS

``` picotool uf2 convert fs.bin -t bin data.uf2 -o 0x10180000 --family data```
convert the generated LFS into uf2 image. Note this command is not needed while data/ is not modified

```picotool uf2 combine build/coincoin.uf2 -t uf2  data.uf2 -t uf2  flash.uf2 -t uf2```
This combine uf2 containing the app and uf2 containing the data into the LFS

Flash it !
```picotool load -f flash.uf2```

