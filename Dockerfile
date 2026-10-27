# Everything ReactX needs in one image: GCC 16, CMake, Ninja, Emscripten and
# the reactx CLI. Published as ghcr.io/sargisis/reactx:<version> on every release.
#
#   docker build -t reactx .
#   docker run --rm -it -u "$(id -u):$(id -g)" -v "$PWD:/app" -w /app -p 8090:8090 reactx
#   reactx new my-app && cd my-app && reactx dev --host 0.0.0.0
#
# The same image is the dev container (.devcontainer/devcontainer.json).

ARG GCC_VERSION=16

FROM gcc:${GCC_VERSION} AS base
RUN apt-get update \
 && apt-get install -y --no-install-recommends cmake ninja-build \
 && rm -rf /var/lib/apt/lists/*

# The CLI, linked statically (the same build as the binaries in GitHub Releases).
FROM base AS cli
WORKDIR /src
COPY . .
RUN cmake -S . -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DREACTX_BUILD_EXAMPLES=OFF -DREACTX_BUILD_TESTS=OFF -DCMAKE_EXE_LINKER_FLAGS=-static \
 && cmake --build /build --target reactx_cli reactx_serve \
 && install -D -s -t /opt/reactx/bin /build/reactx /build/reactx_serve

FROM base
ARG EMSDK_VERSION=6.0.13
ENV EMSDK=/opt/emsdk
RUN git clone --depth 1 https://github.com/emscripten-core/emsdk.git "$EMSDK" \
 && "$EMSDK/emsdk" install "$EMSDK_VERSION" \
 && "$EMSDK/emsdk" activate "$EMSDK_VERSION" \
 && rm -rf "$EMSDK/downloads" "$EMSDK/.git" \
 && ln -s "$(ls -d "$EMSDK"/node/*/bin)/node" /usr/local/bin/node
ENV EM_CONFIG=/opt/emsdk/.emscripten \
    PATH=/opt/emsdk:/opt/emsdk/upstream/emscripten:$PATH

# Fill Emscripten's cache (libc, libc++ with wasm exceptions) once here, and let
# any user write to it: `docker run -u` and the dev container don't run as root.
RUN printf '#include <string>\n#include <vector>\nint main() { std::vector<std::string> v{"x"}; return int(v.size()); }\n' > /tmp/warm.cpp \
 && em++ -std=c++26 -O2 -fwasm-exceptions -sALLOW_MEMORY_GROWTH=1 /tmp/warm.cpp -o /tmp/warm.js \
 && rm /tmp/warm.* \
 && chmod -R a+rwX "$EMSDK/upstream/emscripten/cache"

# A non-root user for the dev container (its UID follows yours there).
RUN useradd --create-home --uid 1000 --shell /bin/bash dev

COPY --from=cli /opt/reactx/bin/ /usr/local/bin/
COPY LICENSE-MIT LICENSE-APACHE /usr/local/share/doc/reactx/

WORKDIR /app
EXPOSE 8090
CMD ["bash"]
