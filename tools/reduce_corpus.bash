#!/bin/bash
set -eu

echo "Reducing $1 to $2 for $3"
executable=$(realpath "$3")

# First reduction using afl-cmin.
tmp_dir=$(mktemp -d)
afl-cmin -A -i "$1" -o "$tmp_dir" -- "$executable" -

out_dir=$(realpath "$2")
mkdir -p "$out_dir"

# Second reduction using afl-tmin.
pushd "$tmp_dir"
for f in *; do
    afl-tmin -i "$f" -o "$out_dir/$f" -- "$executable" -
done
popd
rm -r "$tmp_dir"
