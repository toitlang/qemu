#!/usr/bin/env bash

set -euo pipefail

TARGET=${TARGET:-xtensa-softmmu}
VERSION=${VERSION:-dev}

sed -i '' "s/project('qemu', \['c'\],/project('qemu', \['c', 'objc'\],/" meson.build

# workaround for some headers that macOS couldn't find for some unknown reason
sed -i '' "s/common_user_inc = \[\]/common_user_inc = \['include', 'build'\]/" meson.build

echo DBG
./configure --help
export MACOSX_DEPLOYMENT_TARGET=13
./configure \
    --bindir=xtensa-softmmu \
    --datadir=share/qemu \
    --enable-fdt=internal \
    --enable-gcrypt \
    --enable-cocoa \
    --disable-gtk \
    --enable-pixman \
    --disable-sdl \
    --enable-strip \
    --enable-slirp \
    --enable-stack-protector \
    --prefix=$PWD/install/qemu \
    --python=python3 \
    --target-list=${TARGET} \
    --with-pkgversion="${VERSION}" \
    --with-suffix="" \
    --enable-fdt=disabled \
    --without-default-features \
|| { cat meson-logs/meson-log.txt && false; }
