#!/bin/env sh

make clean
make dmenu
timeout 10s ./dmenu -c red < dmenu.c
