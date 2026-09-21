#!/bin/bash
# Builds the web version and runs its benchmark in headless Chromium (Docker).
# Results go to benchmarks/results/ and a summary row to benchmarks/results.md.
#
# Usage: ./scripts/bench_web.sh [frames] [label]
set -euo pipefail

cd "$(dirname "$0")/.."
FRAMES="${1:-2000}"
LABEL="${2:-}"
PLAYWRIGHT_VERSION=$(sed -n 's/.*"playwright-core": "\(.*\)".*/\1/p' benchmarks/package.json)

./scripts/build_web.sh

GIT_COMMIT=$(git rev-parse --short HEAD)
GIT_DIRTY=$([ -n "$(git status --porcelain -- src app CMakeLists.txt web)" ] && echo 1 || echo 0)

# node_modules lives in a named volume so it is installed once and stays out
# of the repository
docker run --rm \
	-v "$PWD":/repo -w /repo/benchmarks \
	-v wolfenstein-bench-node:/repo/benchmarks/node_modules \
	-e GIT_COMMIT="$GIT_COMMIT" -e GIT_DIRTY="$GIT_DIRTY" \
	"mcr.microsoft.com/playwright:v$PLAYWRIGHT_VERSION-noble" \
	bash -c "npm install --no-audit --no-fund --silent \
		&& node run_web_benchmark.mjs /repo/build-web/bin $FRAMES '$LABEL' \
		&& chown -R $(id -u):$(id -g) results results.md"
