# The game's WebAssembly build, compiled and served in containers: Docker is
# all it takes to build the game and play it in a browser.
#
#   docker build -f docker/web.Dockerfile -t wolfenstein-web .
#   docker run --rm -p 8000:8000 wolfenstein-web      # then open http://localhost:8000

# The Emscripten SDK as CI pins it (.github/workflows/ci.yml and pages.yml)
FROM emscripten/emsdk:6.0.10 AS build
WORKDIR /src
COPY . .
# Assets are stored with Git LFS; a checkout without it has only pointer files
RUN if grep -rlq "^version https://git-lfs" assets; then \
        echo "assets/ holds Git LFS pointers: run git lfs install && git lfs pull" >&2; \
        exit 1; \
    fi
RUN cmake --preset web-release && cmake --build --preset web-release

# Served by the repository's own server: byte ranges, as the page's loader
# expects, and reachable from outside the container
FROM python:3.14-slim
COPY --from=build /src/build/web-release/bin /site
COPY scripts/serve_web.py /serve_web.py
EXPOSE 8000
CMD ["python3", "-u", "/serve_web.py", "/site", "8000"]
