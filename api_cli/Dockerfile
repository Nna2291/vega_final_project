FROM ubuntu:24.04

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        curl \
        libcurl4-openssl-dev \
        ca-certificates && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . /app

RUN cmake -S . -B build -DBUILD_TESTING=ON && \
    cmake --build build --config Release && \
    cd build && ctest --output-on-failure

# Default command: run pricing_service; tickers are loaded via C++ DB stub.
CMD ["./build/pricing_service"]


