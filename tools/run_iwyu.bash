#!/bin/bash
set -u

"$(dirname "$(realpath "$0")")"/iwyu_tool.py \
    -j $(nproc) \
    -p $1 \
    engine/ sandbox/ tools/ \
    -e 'engine/sources/tasklet/x86_64_sysv.S' \
    -e 'engine/sources/vulkan/context_table.cc' \
    -e 'engine/tests/container/vector.cc' \
    -e 'engine/tests/support/enum.cc' \
    -e 'engine/tests/support/variant.cc' |
    grep -v correct | sed -e :a -e '/^\n*$/{$d;N;};/\n$/ba' | cat -s
