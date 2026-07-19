#!/usr/bin/env bash

# Copyright (C) 2026 Toit contributors.
# Use of this source code is governed by an MIT-style license that can be
# found in the LICENSE file.

set -euo pipefail

QEMU_BUILD_DIR=${1:-${PWD}/build}
TOIT_VERSION=$(toit version)
ENVELOPE_BASE=https://github.com/toitlang/envelopes/releases/download/${TOIT_VERSION}
ENVELOPE_DIR=$(mktemp -d)

cleanup() {
  rm -rf "${ENVELOPE_DIR}"
}
trap cleanup EXIT

for name in \
  firmware-esp32 \
  firmware-esp32s3-spiram-octo \
  firmware-esp32c3; do
  curl --fail --location --retry 3 \
    --output "${ENVELOPE_DIR}/${name}.envelope.gz" \
    "${ENVELOPE_BASE}/${name}.envelope.gz"
  gzip -d "${ENVELOPE_DIR}/${name}.envelope.gz"
done

export QEMU_SYSTEM_XTENSA=${QEMU_BUILD_DIR}/qemu-system-xtensa
export QEMU_SYSTEM_RISCV32=${QEMU_BUILD_DIR}/qemu-system-riscv32

TOIT_WIFI_ENVELOPE=${ENVELOPE_DIR}/firmware-esp32.envelope \
  tests/toit/run-wifi-test.sh esp32
TOIT_WIFI_ENVELOPE=${ENVELOPE_DIR}/firmware-esp32s3-spiram-octo.envelope \
  tests/toit/run-wifi-test.sh esp32s3
TOIT_C3_ENVELOPE=${ENVELOPE_DIR}/firmware-esp32c3.envelope \
  tests/toit/run-c3-boot-test.sh
