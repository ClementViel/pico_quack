#!/bin/bash

export PICO_SDK_PATH="$PWD/../pico-sdk"
echo $PICO_SDK_PATH

if [ ! -f pico_sdk_import.cmake ]; then
  echo "pico_sdk import does not exists, copying"
  cp $PICO_SDK_PATH/external/pico_sdk_import.cmake .
  cp $PICO_SDK_PATH/pico_sdk_init.cmake .
fi
