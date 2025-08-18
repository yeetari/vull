#!/bin/bash
set -eu

update_test=false
if [ "$1" = "-u" ]; then
    update_test=true
    shift 1
fi

vslc_prog="$1"
test_name="$2"

spv_output="$(mktemp)"
trap 'rm -f -- "$spv_output"' EXIT
"$vslc_prog" "input/$test_name.vsl" -o "$spv_output"
if [ "$update_test" = true ]; then
    spirv-dis "$spv_output" -o "expected/$test_name.spvasm" --no-indent --no-header
else
    echo -n "RUN ${test_name}... "
    expected=$(spirv-diff "expected/$test_name.spvasm" "expected/$test_name.spvasm" --no-header --no-indent --color)
    actual=$(spirv-diff "expected/$test_name.spvasm" "$spv_output" --no-header --no-indent --color)
    if [ "$expected" = "$actual" ]; then
        echo 'PASS'
    else
        echo 'FAIL'
        echo "$actual"
        exit 1
    fi
fi
