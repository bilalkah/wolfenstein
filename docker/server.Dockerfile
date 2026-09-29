# The multiplayer server, built and run in containers: Docker is all it
# takes to host a game, on a LAN or on any cloud machine.
#
#   docker build -f docker/server.Dockerfile -t wolfenstein-server .
#   docker run --rm -p 8080:8080 wolfenstein-server
#
# It speaks plain ws:// on port 8080; for players on the internet, put a
# proxy that holds a certificate (Caddy) in front of it for wss://.

FROM ubuntu:26.04 AS build
COPY scripts/install_deps.sh /tmp/install_deps.sh
RUN /tmp/install_deps.sh && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
# Assets are stored with Git LFS; a checkout without it has only pointer files
RUN if grep -rlq "^version https://git-lfs" assets; then \
        echo "assets/ holds Git LFS pointers: run git lfs install && git lfs pull" >&2; \
        exit 1; \
    fi
RUN cmake --preset native-release -DWOLFENSTEIN_TESTS=OFF \
    && cmake --build --preset native-release --target wolfenstein-server

# The server alone, its standard library and the game's content
FROM ubuntu:26.04
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
       libc++1 libc++abi1 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /src/build/native-release/bin/wolfenstein-server /usr/local/bin/
COPY --from=build /src/assets /assets
EXPOSE 8080
USER nobody
CMD ["wolfenstein-server", "--port", "8080", "--assets", "/assets/"]
