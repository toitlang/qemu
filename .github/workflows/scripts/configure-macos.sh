#!/usr/bin/env bash

set -euo pipefail

TARGET=${TARGET:-xtensa-softmmu,riscv32-softmmu}
VERSION=${VERSION:-dev}

export MACOSX_DEPLOYMENT_TARGET=13
./configure \
    --bindir=bin \
    --datadir=share/qemu-firmware \
    --disable-docs \
    --disable-gtk \
    --disable-opengl \
    --disable-sdl \
    --enable-fdt=internal \
    --enable-gcrypt \
    --enable-pixman \
    --enable-strip \
    --enable-slirp \
    --enable-stack-protector \
    --prefix=$PWD/install/qemu \
    --python=python3 \
    --target-list=${TARGET} \
    --with-pkgversion="${VERSION}" \
    --with-suffix="" \
    --without-default-features \
|| { cat meson-logs/meson-log.txt && false; }
