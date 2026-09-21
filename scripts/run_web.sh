#!/bin/bash
# Serves the web build at http://localhost:8000 (browsers refuse to load
# WebAssembly from file:// URLs)
set -euo pipefail

cd "$(dirname "$0")/.."
PORT="${PORT:-8000}"

if [ ! -f build-web/bin/index.html ]; then
	./scripts/build_web.sh
fi

echo "Open http://localhost:$PORT"
exec python3 -m http.server "$PORT" --directory build-web/bin
