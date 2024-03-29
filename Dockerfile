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
    git submodule update --init --recursive --remote -- third_party/pigweed/ && \
    git submodule update --init --recursive --remote -- third_party/jsoncpp/ && \
    # TODO: Why doesn't the submodule in the repo get the one that has the sdk directory?
    git clone https://android.googlesource.com/platform/external/perfetto -b v39.0 --depth 1 third_party/perfetto/repo && \
    git submodule update --init --recursive --remote -- third_party/nlunit-test/ && \
    git submodule update --init --recursive --remote -- third_party/nlassert/ && \
    git submodule update --init --recursive --remote -- third_party/nlio/ && \
    # Small hack... https://github.com/project-chip/connectedhomeip/issues/31102
    (source scripts/activate.sh || \
    .environment/pigweed-venv/bin/pip install "prompt-toolkit==3.0.43") && \
    source scripts/activate.sh

WORKDIR /wled-matter-bridge

RUN cmake -S third_party/curl/repo -B third_party/curl/repo/build -DBUILD_STATIC_LIBS=on -DENABLE_WEBSOCKETS=on && \
    cmake --build third_party/curl/repo/build --target libcurl_static -j

RUN cd third_party/connectedhomeip && \
    source scripts/activate.sh && \
    cd - && \
    cd src && \
    gn gen out/host && \
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
