#!/bin/bash
# Installs the native toolchain and libraries on Ubuntu 26.04: Clang with
# libc++ (the standard library of every target: Emscripten and macOS use it
# too, and libstdc++ lacks <mdspan> even in GCC 15), sanitizer runtimes,
# formatting/lint tools and SDL2. Used as is by
# docker/dev.Dockerfile and CI.
set -euo pipefail

SUDO=$([ "$(id -u)" -eq 0 ] && echo "" || echo sudo)
$SUDO apt-get update
DEBIAN_FRONTEND=noninteractive $SUDO apt-get install -y --no-install-recommends \
	ca-certificates git git-lfs make cmake python3 \
	clang libc++-dev libc++abi-dev libclang-rt-21-dev llvm-21 \
	clang-format clang-tidy \
	libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev
