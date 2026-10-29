FROM ubuntu:24.04 AS build

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        libboost-dev \
        ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /source
COPY . .

RUN cmake -S . -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /build --target easydesk-signal

FROM ubuntu:24.04

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates \
        libgcc-s1 \
        libstdc++6 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --no-create-home easydesk

COPY --from=build /build/easydesk-signal /usr/local/bin/easydesk-signal

USER easydesk
EXPOSE 8080

ENV EASYDESK_BIND_ADDRESS=0.0.0.0 \
    EASYDESK_PORT=8080 \
    EASYDESK_WORKER_THREADS=2

ENTRYPOINT ["/usr/local/bin/easydesk-signal"]
