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
PATCHELF=${PATCHELF:-patchelf}
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

declare -a QUEUE=(
  "${PACKAGE_ROOT}/bin/qemu-system-xtensa"
  "${PACKAGE_ROOT}/bin/qemu-system-riscv32"
)
declare -A SEEN=()

for ((index = 0; index < ${#QUEUE[@]}; index++)); do
  binary=${QUEUE[index]}
  while read -r dependency; do
    [[ -n "${dependency}" && -f "${dependency}" ]] || continue
    name=$(basename "${dependency}")
    case "${name}" in
      ld-linux-*|libc.so.*|libdl.so.*|libgcc_s.so.*|libm.so.*|libpthread.so.*|librt.so.*)
        continue
        ;;
    esac
    [[ -z "${SEEN[${name}]:-}" ]] || continue
    SEEN[${name}]=1
    cp -L "${dependency}" "${PACKAGE_ROOT}/lib/${name}"
    QUEUE+=("${PACKAGE_ROOT}/lib/${name}")
  done < <(ldd "${binary}" | sed -n \
    -e 's/.*=> \(\/[^ ]*\).*/\1/p' \
    -e 's/^\s*\(\/[^ ]*\).*/\1/p')
done

"${PATCHELF}" --set-rpath '$ORIGIN/../lib' \
  "${PACKAGE_ROOT}/bin/qemu-system-xtensa" \
  "${PACKAGE_ROOT}/bin/qemu-system-riscv32"
for library in "${PACKAGE_ROOT}"/lib/*; do
  "${PATCHELF}" --set-rpath '$ORIGIN' "${library}"
done

printf '%s\n' "${VERSION}" > "${PACKAGE_ROOT}/VERSION"
"${PACKAGE_ROOT}/bin/qemu-system-xtensa" --version
"${PACKAGE_ROOT}/bin/qemu-system-riscv32" --version

tar -C "${DIST_ROOT}" -czf "${DIST_ROOT}/${PACKAGE_NAME}.tar.gz" \
  "${PACKAGE_NAME}"
