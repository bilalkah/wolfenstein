# Native toolchain used by scripts/dev.sh: Ubuntu 26.04 with Clang, libc++
# (the same standard library as the Emscripten and macOS builds) and the
# system headers SDL 3 builds against.
# Build context is the repository root; see docker/dev.Dockerfile.dockerignore.
FROM ubuntu:26.04

COPY scripts/install_deps.sh /tmp/install_deps.sh
RUN /tmp/install_deps.sh && rm -rf /var/lib/apt/lists/* /tmp/install_deps.sh

# llvm-symbolizer, so sanitizer reports show file and line
ENV PATH="/usr/lib/llvm-21/bin:${PATH}"
