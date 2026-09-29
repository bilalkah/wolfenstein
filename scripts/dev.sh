#!/bin/bash
# Runs a command in the native toolchain container (docker/dev.Dockerfile),
# building the image on first use. Examples:
#   ./scripts/dev.sh cmake --preset native-debug
#   ./scripts/dev.sh cmake --build --preset native-debug
#   ./scripts/dev.sh ctest --preset native-debug
set -euo pipefail

cd "$(dirname "$0")/.."
# Tagged by what goes into the image, so a change to it builds a new one
IMAGE=wolfenstein-dev:26.04-$(cat docker/dev.Dockerfile scripts/install_deps.sh | cksum | cut -d ' ' -f 1)

if ! docker info >/dev/null 2>&1; then
	echo "Docker is not running; start Docker Desktop and try again." >&2
	exit 1
fi

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
	docker build -t "$IMAGE" -f docker/dev.Dockerfile .
fi

# Headless SDL so the game and tests run without a display or sound card; the
# software renderer, drawing the window without OpenGL, avoids Mesa's GL stack
exec docker run --rm \
	-v "$PWD":/src -w /src \
	-e SDL_VIDEODRIVER=offscreen -e SDL_AUDIODRIVER=dummy \
	-e SDL_RENDER_DRIVER=software -e SDL_FRAMEBUFFER_ACCELERATION=0 \
	-u "$(id -u):$(id -g)" -e HOME=/tmp \
	"$IMAGE" "$@"
