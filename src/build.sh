#!/bin/sh

if [ ! -f "nob" ]; then
    echo "Building nob..."
    gcc -std=c11 -o nob nob.c

    if [ $? -ne 0 ]; then
        echo "Failed to build nob."
        exit 1
    fi
fi

./nob -target inspector -platform gnu_linux_x11 -mode debug