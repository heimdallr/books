#!/bin/bash
set -euo pipefail

mkdir -p build
cd build

GENERATOR=${1:-TXZ}

cmake .. -DCMAKE_BUILD_TYPE=Release -DQt6_DIR=/opt/Qt/6.12.0/gcc14_Release/lib/cmake/Qt6 -DCPACK_GENERATOR=$GENERATOR -G Ninja
cmake --build . --config Release --parallel
ctest . --output-on-failure -C Release
LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" cpack -C Release
