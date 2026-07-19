#!/usr/bin/env bash

# Copyright (C) 2026 Toit contributors.
# Use of this source code is governed by an MIT-style license that can be
# found in the LICENSE file.

set -euo pipefail

VERSION=${1:?version is required}
PLATFORM=${2:?platform is required}
INSTALL_ROOT=${INSTALL_ROOT:-${PWD}/install/qemu}
DIST_ROOT=${DIST_ROOT:-${PWD}/dist}
FIRMWARE_ROOT=${FIRMWARE_ROOT:-${INSTALL_ROOT}/share/qemu-firmware}
PACKAGE_NAME=qemu-toit-${VERSION}-${PLATFORM}
PACKAGE_ROOT=${DIST_ROOT}/${PACKAGE_NAME}

test -x "${INSTALL_ROOT}/bin/qemu-system-xtensa"
test -x "${INSTALL_ROOT}/bin/qemu-system-riscv32"

mkdir -p "${PACKAGE_ROOT}/bin" "${PACKAGE_ROOT}/lib" \
  "${PACKAGE_ROOT}/share/qemu-firmware"
cp LICENSE "${PACKAGE_ROOT}/"
cp "${INSTALL_ROOT}/bin/qemu-system-xtensa" \
  "${INSTALL_ROOT}/bin/qemu-system-riscv32" \
  "${PACKAGE_ROOT}/bin/"
cp "${FIRMWARE_ROOT}"/esp32*.bin \
  "${PACKAGE_ROOT}/share/qemu-firmware/"

for executable in \
  "${PACKAGE_ROOT}/bin/qemu-system-xtensa" \
  "${PACKAGE_ROOT}/bin/qemu-system-riscv32"; do
  dylibbundler -od -b -x "${executable}" \
    -d "${PACKAGE_ROOT}/lib" \
    -p '@executable_path/../lib'
done

printf '%s\n' "${VERSION}" > "${PACKAGE_ROOT}/VERSION"
"${PACKAGE_ROOT}/bin/qemu-system-xtensa" --version
"${PACKAGE_ROOT}/bin/qemu-system-riscv32" --version

tar -C "${DIST_ROOT}" -czf "${DIST_ROOT}/${PACKAGE_NAME}.tar.gz" \
  "${PACKAGE_NAME}"
