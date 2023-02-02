#!/bin/env sh

make clean
make dmenu
seq 200 | timeout 10s ./dmenu -g 16 -d 3
