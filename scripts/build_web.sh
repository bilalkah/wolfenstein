#!/bin/bash
# Builds the WebAssembly version into build-web/bin. Uses a local Emscripten
# SDK when emcmake is on PATH, otherwise the emscripten/emsdk Docker image.
set -euo pipefail

cd "$(dirname "$0")/.."
BUILD_DIR=build-web

# Assets are stored with Git LFS; without it the checkout only has pointer files
if grep -rlq "^version https://git-lfs" assets; then
	echo "assets/ contains Git LFS pointers; run: git lfs install && git lfs pull" >&2
	exit 1
fi

if command -v emcmake >/dev/null; then
	emcmake cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
	cmake --build "$BUILD_DIR" --parallel
else
	# The named volume keeps Emscripten's compiled SDL ports between builds
	docker run --rm \
		-v "$PWD":/src -w /src \
		-v wolfenstein-emcache:/emsdk/upstream/emscripten/cache \
		emscripten/emsdk:latest \
		bash -c "emcmake cmake -S . -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release \
			&& cmake --build $BUILD_DIR --parallel \
			&& chown -R $(id -u):$(id -g) $BUILD_DIR"
fi

echo "Built $BUILD_DIR/bin/index.html; run ./scripts/run_web.sh to play"
