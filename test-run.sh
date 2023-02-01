#!/bin/env sh

make clean
make dmenu
timeout 10s ./dmenu < dmenu.c
