FROM ubuntu:jammy

RUN apt update && \
    DEBIAN_FRONTEND=noninteractive apt install -y \
        clang \
        clangd \
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

CMD ["/bin/bash"]
