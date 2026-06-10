# syntax=docker/dockerfile:1

# ---- builder ----
FROM ubuntu:24.04 AS builder
ARG VCPKG_REF=32305df7d0b9a308e6ac454dd98ebfe550342c09
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        git curl zip unzip tar ca-certificates pkg-config \
        build-essential cmake ninja-build \
        python3 python3-venv python3-pip \
        bison flex \
    && rm -rf /var/lib/apt/lists/*

ENV VCPKG_ROOT=/opt/vcpkg
RUN git clone https://github.com/microsoft/vcpkg "$VCPKG_ROOT" \
    && git -C "$VCPKG_ROOT" checkout "$VCPKG_REF" \
    && "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics

WORKDIR /src
# Manifest first so the dependency layer caches independently of source changes.
COPY vcpkg.json .
COPY CMakeLists.txt CMakePresets.json ./
COPY src ./src
COPY tests ./tests
COPY migrations_clickhouse ./migrations_clickhouse

RUN --mount=type=cache,target=/root/.cache/vcpkg \
    cmake --preset linux -DXBREACH_WORKER_BUILD_TESTS=OFF \
    && cmake --build --preset linux

# ---- runtime ----
FROM debian:bookworm-slim AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates \
    && rm -rf /var/lib/apt/lists/*
COPY --from=builder /src/build/linux/xbreach-worker /usr/local/bin/xbreach-worker
COPY --from=builder /src/migrations_clickhouse /opt/xbreach/migrations_clickhouse
ENV XBREACH_CLICKHOUSE_MIGRATIONS_PATH=/opt/xbreach/migrations_clickhouse
ENTRYPOINT ["/usr/local/bin/xbreach-worker"]
