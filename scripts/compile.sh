#!/bin/bash
# Native release build (Clang + libc++); see CMakePresets.json for others
set -euo pipefail
cd "$(dirname "$0")/.."
cmake --preset native-release
cmake --build --preset native-release
