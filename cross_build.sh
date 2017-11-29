#!/bin/bash
# Build deb package in Docker image nomaddo/cross-rpi
# `vc4cl-VERSION-Linux.deb` will be geneerated...

cmake . -DCROSS_COMPILE=ON -DBUILD_TESTING=ON -DCMAKE_FIND_DEBUG_MODE=1  -DOpenCL_INCLUDE_DIR=/opt/rasberrypi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/arm-linux-gnueabihf/include -DOpenCL_LIBRARY=/opt/rasberrypi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/arm-linux-gnueabihf/include -DCMAKE_INSTALL_PREFIX=/usr
cpack -G DEB
