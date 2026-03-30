#!/bin/bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DQt6_DIR=/opt/Qt-6.11.0/lib/cmake/Qt6 -DCPACK_GENERATOR=DEB -G Ninja
cmake --build .
cpack

