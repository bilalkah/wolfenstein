#!/bin/bash
# Installs the native toolchain on Ubuntu 26.04: Clang with libc++ (the
# standard library of every target: Emscripten and macOS use it too, and
# libstdc++ lacks <mdspan> even in GCC 15), sanitizer runtimes and
# formatting/lint tools. Used as is by docker/dev.Dockerfile and CI.
#
# SDL 3 itself is built with the game (cmake/Dependencies.cmake); what it
# needs from the system are the headers of the windowing and sound systems it
# talks to (X11, Wayland, ALSA, PulseAudio, PipeWire), loaded when the game
# runs. Without them it builds all the same, headless only.
set -euo pipefail

SUDO=$([ "$(id -u)" -eq 0 ] && echo "" || echo sudo)
$SUDO apt-get update
DEBIAN_FRONTEND=noninteractive $SUDO apt-get install -y --no-install-recommends \
	ca-certificates git git-lfs make cmake pkg-config python3 \
	clang libc++-dev libc++abi-dev libclang-rt-21-dev llvm-21 \
	clang-format clang-tidy \
	libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev \
	libxi-dev libxss-dev libxtst-dev libxkbcommon-dev \
	libwayland-dev libdecor-0-dev libegl-dev libgl-dev libgles-dev \
	libdrm-dev libgbm-dev libdbus-1-dev libudev-dev libibus-1.0-dev \
	libasound2-dev libpulse-dev libpipewire-0.3-dev
