#!/bin/sh

emcmake cmake -S . -B build-web -G"Ninja"
ninja -C build-web
