#!/bin/bash
# Runs a command in the native toolchain container (docker/dev.Dockerfile),
# building the image on first use. Examples:
#   ./scripts/dev.sh cmake --preset native-debug
#   ./scripts/dev.sh cmake --build --preset native-debug
#   ./scripts/dev.sh ctest --preset native-debug
set -euo pipefail

cd "$(dirname "$0")/.."
IMAGE=wolfenstein-dev:26.04

if ! docker info >/dev/null 2>&1; then
	echo "Docker is not running; start Docker Desktop and try again." >&2
	exit 1
fi

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
	docker build -t "$IMAGE" -f docker/dev.Dockerfile .
fi

# Headless SDL so the game and tests run without a display or sound card; the
# software renderer avoids Mesa's GL stack
exec docker run --rm \
	-v "$PWD":/src -w /src \
	-e SDL_VIDEODRIVER=offscreen -e SDL_AUDIODRIVER=dummy \
	-e SDL_RENDER_DRIVER=software \
	-u "$(id -u):$(id -g)" -e HOME=/tmp \
	"$IMAGE" "$@"
