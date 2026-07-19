#!/usr/bin/env bash

set -euo pipefail

TARGET=${TARGET:-xtensa-softmmu,riscv32-softmmu}
VERSION=${VERSION:-dev}

./configure \
    --bindir=bin \
    --datadir=share/qemu-firmware \
    --disable-docs \
    --disable-gtk \
    --disable-opengl \
    --disable-sdl \
    --disable-werror \
    --enable-fdt=internal \
    --enable-gcrypt \
    --enable-iconv \
    --enable-pixman \
    --enable-strip \
    --enable-slirp \
    --enable-stack-protector \
    --prefix=${PWD}/install/qemu \
    --target-list=${TARGET} \
    --with-pkgversion="${VERSION}" \
    --with-suffix="" \
    --without-default-features \
|| { cat meson-logs/meson-log.txt && false; }


# This fixes the issue that for some reason, meson is not able to determine correct
# paths for libiconv and libintl libraries from 'pkg-config --libs --static libgcrypt'.
# MSYS_BASE=$(cygpath -w / | sed 's/\\/\//g')
# sed -i "s|/mingw64/lib/libintl.dll.a|${MSYS_BASE}/mingw64/lib/libintl.dll.a|g; s|/mingw64/lib/libiconv.dll.a|${MSYS_BASE}/mingw64/lib/libiconv.dll.a|g" build/build.ninja
