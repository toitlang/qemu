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

test -x "${INSTALL_ROOT}/bin/qemu-system-xtensa.exe"
test -x "${INSTALL_ROOT}/bin/qemu-system-riscv32.exe"

mkdir -p "${PACKAGE_ROOT}/bin" "${PACKAGE_ROOT}/share/qemu-firmware"
cp LICENSE "${PACKAGE_ROOT}/"
cp "${INSTALL_ROOT}/bin/qemu-system-xtensa.exe" \
  "${INSTALL_ROOT}/bin/qemu-system-riscv32.exe" \
  "${PACKAGE_ROOT}/bin/"
cp "${FIRMWARE_ROOT}"/esp32*.bin \
  "${PACKAGE_ROOT}/share/qemu-firmware/"

for _ in 1 2 3; do
  while read -r dependency; do
    [[ -n "${dependency}" && -f "${dependency}" ]] || continue
    cp -n "${dependency}" "${PACKAGE_ROOT}/bin/"
  done < <(find "${PACKAGE_ROOT}/bin" -type f \
    \( -name '*.exe' -o -name '*.dll' \) -print0 |
    xargs -0 -n1 ldd 2>/dev/null |
    sed -n 's/.*=> \(\/mingw64\/bin\/[^ ]*\.dll\).*/\1/p' |
    sort -u)
done

printf '%s\n' "${VERSION}" > "${PACKAGE_ROOT}/VERSION"
"${PACKAGE_ROOT}/bin/qemu-system-xtensa.exe" --version
"${PACKAGE_ROOT}/bin/qemu-system-riscv32.exe" --version

(
  cd "${DIST_ROOT}"
  zip -9 -r "${PACKAGE_NAME}.zip" "${PACKAGE_NAME}"
)
