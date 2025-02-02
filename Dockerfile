FROM ubuntu:jammy AS builder

# connectedhomeip/scripts/activate.sh doesn't seem to work in /bin/sh
RUN ln -sf /bin/bash /bin/sh

RUN apt update && \
    DEBIAN_FRONTEND=noninteractive apt install -y \
        cmake \
        git \
        python3 \
        python3-pip \
        python3-venv \
        pkg-config \
        libssl-dev \
        libdbus-1-dev \
        libglib2.0-dev \
        libavahi-client-dev \
        ninja-build \
        python3-venv \
        python3-dev\
        python3-pip \
        unzip \
        libgirepository1.0-dev \
        libcairo2-dev \
        libreadline-dev

RUN git clone https://github.com/zackelia/wled-matter-bridge && \
    cd wled-matter-bridge && \
    git submodule update --init && \
    cd third_party/connectedhomeip && \
    # *Significantly* faster to get the submodules we need rather than let activate.sh get them all
    git submodule update --init --recursive -- \
        third_party/pigweed \
        third_party/jsoncpp \
        third_party/nlassert \
        third_party/nlio && \
    source scripts/activate.sh

WORKDIR /wled-matter-bridge

RUN cd third_party/connectedhomeip && \
    source scripts/activate.sh && \
    cd ../../src && \
    gn gen out/host --args='is_debug=false' && \
    ninja -C out/host && \
    strip out/host/wled-matter-bridge

FROM ubuntu:jammy

RUN apt update && \
    DEBIAN_FRONTEND=noninteractive apt install -y \
        python3 \
        python3-pip \
        python3-venv \
        libglib2.0-0 && \
    mkdir /var/chip && \
    python3 -m venv venv && \
    venv/bin/python -m pip install qrcode

COPY --from=builder /wled-matter-bridge/src/out/host/wled-matter-bridge /wled-matter-bridge
COPY --from=builder /wled-matter-bridge/tools /tools

ENTRYPOINT ["/wled-matter-bridge"]
