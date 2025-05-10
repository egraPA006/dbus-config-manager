#!/bin/bash
set -e 

mkdir -p build && cd build
cmake -DCMAKE_INSTALL_PREFIX=../bin ..
cmake --build . --target install --parallel $(nproc)