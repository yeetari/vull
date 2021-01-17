#!/bin/sh

set -e
TMP_CMAKE_DIR=$(mktemp -d)
cmake $(dirname "$0")/.. -B $TMP_CMAKE_DIR -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
iwyu_tool.py -p $TMP_CMAKE_DIR -- -Xiwyu --mapping_file=$(realpath $(dirname "$0"))/iwyu.imp
