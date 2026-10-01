#!/usr/bin/env bash

# Check if clang-format is installed
if ! command -v clang-format &> /dev/null; then
    echo "Error: clang-format is not installed or not in PATH." >&2
    exit 1
fi

# Prune excluded directories, then format remaining .c and .h files
find . \( \
    -path "./lib" -o \
    -path "./build" -o \
    -path "./dbg" -o \
    -path "./profiling" \
\) -prune -o \( -name "*.c" -o -name "*.h" \) -exec clang-format -i {} +

echo "Formatting complete."