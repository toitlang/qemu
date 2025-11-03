#!/usr/bin/env bash

set -euo pipefail

export DEBIAN_FRONTEND="noninteractive"

EXTRA_APT_SOURCES="
deb [arch=arm64] http://ports.ubuntu.com/ noble main restricted
deb [arch=arm64] http://ports.ubuntu.com/ noble-updates main restricted
deb [arch=arm64] http://ports.ubuntu.com/ noble universe
deb [arch=arm64] http://ports.ubuntu.com/ noble-updates universe
deb [arch=arm64] http://ports.ubuntu.com/ noble multiverse
deb [arch=arm64] http://ports.ubuntu.com/ noble-updates multiverse
deb [arch=arm64] http://ports.ubuntu.com/ noble-backports main restricted universe multiverse
"

dpkg --add-architecture arm64

apt-get update -y -q
apt-get install -y -q --no-install-recommends \
    binutils-aarch64-linux-gnu \
    build-essential \
    crossbuild-essential-arm64 \
    gcc-aarch64-linux-gnu \
    git \
    libgcrypt20-dev:arm64 \
    libglib2.0-dev:arm64 \
    libgpg-error-dev:arm64 \
    libpixman-1-dev:arm64 \
    libsdl2-dev:arm64 \
    libslirp-dev:arm64 \
    ninja-build \
    python3-pip \
    zlib1g-dev:arm64 \
&& :

#/usr/bin/pip3 install meson==1.2.3
