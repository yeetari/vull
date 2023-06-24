#!/bin/bash

# TODO: Can't run on GltfParser.cc due to FetchContent dependencies.
find engine/sources engine/tests sandbox tools \
    -name '*.cc' \
    -and -not -name 'ContextTable.cc' \
    -and -not -name 'GltfParser.cc' \
    -and -not -name 'PngStream.cc' \
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
