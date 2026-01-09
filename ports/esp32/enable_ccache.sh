#!/bin/bash
# Enable ccache for ESP32 MicroPython builds
export IDF_CCACHE_ENABLE=1
export CCACHE_DIR=$HOME/.cache/ccache
export CMAKE_C_COMPILER_LAUNCHER=ccache
export CMAKE_CXX_COMPILER_LAUNCHER=ccache

echo "ccache enabled for ESP-IDF builds"
echo "Cache directory: $CCACHE_DIR"
ccache -s
