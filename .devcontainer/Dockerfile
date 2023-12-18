FROM ubuntu:jammy

RUN apt update && \
    DEBIAN_FRONTEND=noninteractive apt install -y \
        clang \
        clangd \
        cmake \
        git \
        python3 \
        python3-pip \
        python3-venv && \
    apt remove -y \
        gcc \
        g++

CMD ["/bin/bash"]
