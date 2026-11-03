# Build environment for portable Linux releases. Ubuntu 22.04 ships glibc 2.35, so plugins
# built here load on that release and anything newer (Debian 12, Fedora 36+, Mint 21+, Arch...).
FROM ubuntu:22.04

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        g++-12 cmake make pkg-config binutils \
        libasound2-dev libfreetype-dev libfontconfig1-dev \
        libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libxcomposite-dev \
    && rm -rf /var/lib/apt/lists/*

ENV CC=gcc-12 CXX=g++-12
