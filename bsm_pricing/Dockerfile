FROM ubuntu:24.04

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        libpq-dev \
        ca-certificates && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Копируем исходники в контейнер; каталог build игнорируется через .dockerignore
COPY . /app

RUN rm -rf build && \
    cmake -S . -B build -DBUILD_TESTING=OFF && \
    cmake --build build --config Release

# Default command: run bsm_pricing. Connection parameters and pipe path are passed via CLI args.
CMD ["./build/bsm_pricing"]


