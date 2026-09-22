#!/bin/bash
# Builds the web version (incrementally) and serves it at http://localhost:8000
# (browsers refuse to load WebAssembly from file:// URLs). PORT overrides the
# port.
set -euo pipefail

cd "$(dirname "$0")/.."
PORT="${PORT:-8000}"

# Always rebuild, so a failed build never leaves an old version being served
./scripts/build_web.sh

if python3 -c "import socket,sys; s=socket.socket(); sys.exit(s.connect_ex(('127.0.0.1', $PORT)) != 0)"; then
	echo "Port $PORT is already in use (another server still running?)." >&2
	echo "Stop it, or pick another port: PORT=8001 ./scripts/run_web.sh" >&2
	exit 1
fi

echo "Open http://localhost:$PORT"
exec python3 -m http.server "$PORT" --directory build/web-release/bin
