#!/usr/bin/env bash

mkdir -p cmake-build-host
cd cmake-build-host || exit
if test "$1" = "--clean"; then
    rm -rf *;
fi

cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -Dveridie_build_tests=ON \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  #-DCMAKE_CXX_COMPILER=clang++-12
cmake --build . && ctest -VV

## GDB debugging on target (android):
# adb gdbserver :5039 shell \"export LD_LIBRARY_PATH=/data/local/tmp/armeabi-v7a\; /data/local/tmp/armeabi-v7a/tests\"
# export LD_LIBRARY_PATH=/data/local/tmp/armeabi-v7a; /data/local/tmp/armeabi-v7a/tests
