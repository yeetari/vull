#!/bin/bash

# TODO: Can't run on gltf_parser.cc due to FetchContent dependencies.
find engine/sources engine/tests sandbox tools \
    -name '*.cc' \
    -and -not -name 'context_table.cc' \
    -and -not -name 'gltf_parser.cc' \
    -and -not -name 'png_stream.cc' \
    -print0 |
parallel -0 \
    include-what-you-use \
        -std=c++20 \
        -isystem/usr/include/freetype2 \
        -isystem/usr/include/harfbuzz \
        -isystemtools/vpak/enc \
        -Iengine/include -Iengine/sources \
        -fno-rtti \
        -w \
        {} 2>&1 |
    grep -v correct | sed -e :a -e '/^\n*$/{$d;N;};/\n$/ba' | cat -s
