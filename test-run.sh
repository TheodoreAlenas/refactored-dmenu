#!/bin/env sh

make clean
make dmenu
seq 63 | timeout 10s ./dmenu -c red -g 16 -d 3
