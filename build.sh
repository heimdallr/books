#!/bin/bash
mkdir build
cd build
GENERATOR=$1
if [ -z "$GENERATOR" ]; then
    GENERATOR=TXZ
fi

cmake .. -DCMAKE_BUILD_TYPE=Release -DQt6_DIR=/opt/Qt/6.11.0/gcc15.2_Release/lib/cmake/Qt6 -DCPACK_GENERATOR=$GENERATOR -G Ninja
cmake --build . --config Release --parallel
LD_LIBRARY_PATH=$PWD/lib:$LD_LIBRARY_PATH cpack -C Release
