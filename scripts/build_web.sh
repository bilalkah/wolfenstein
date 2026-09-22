#!/bin/bash
# Builds the WebAssembly version into build/web-release/bin. Uses a local Emscripten
# SDK when emcmake is on PATH, otherwise the emscripten/emsdk Docker image.
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD_DIR=build/web-release

# Assets are stored with Git LFS; without it the checkout only has pointer files
if grep -rlq "^version https://git-lfs" assets; then
	echo "assets/ contains Git LFS pointers; run: git lfs install && git lfs pull" >&2
	exit 1
fi

# The preset caps parallel jobs: an unbounded --parallel starts every compile
# at once, which can exhaust the memory of the Docker VM
if command -v emcmake >/dev/null; then
	cmake --preset web-release
	cmake --build --preset web-release
else
	# The named volume keeps Emscripten's compiled SDL ports between builds
	docker run --rm \
		-v "$PWD":/src -w /src \
		-v wolfenstein-emcache:/emsdk/upstream/emscripten/cache \
		emscripten/emsdk:latest \
		bash -c "cmake --preset web-release \
			&& cmake --build --preset web-release \
			&& chown -R $(id -u):$(id -g) $BUILD_DIR"
fi

echo "Built $BUILD_DIR/bin/index.html; run ./scripts/run_web.sh to play"
