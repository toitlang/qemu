#!/usr/bin/env bash

# Copyright (C) 2026 Toit contributors.
# Use of this source code is governed by an MIT-style license that can be
# found in the LICENSE file.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
QEMU_SYSTEM_RISCV32="${QEMU_SYSTEM_RISCV32:-${ROOT_DIR}/build/qemu-system-riscv32}"
TOIT_C3_ENVELOPE="${TOIT_C3_ENVELOPE:-}"
TOIT="${TOIT:-toit}"
QEMU_TIMEOUT_TICKS="${QEMU_TIMEOUT_TICKS:-300}"

if [[ ! -x "${QEMU_SYSTEM_RISCV32}" ]]; then
  echo "QEMU_SYSTEM_RISCV32 is not executable: ${QEMU_SYSTEM_RISCV32}" >&2
  exit 2
fi

if [[ -z "${TOIT_C3_ENVELOPE}" || ! -f "${TOIT_C3_ENVELOPE}" ]]; then
  echo "Set TOIT_C3_ENVELOPE to a current ESP32-C3 envelope." >&2
  exit 2
fi

TEMP_DIR="$(mktemp -d)"
QEMU_PID=""

cleanup() {
  if [[ -n "${QEMU_PID}" ]]; then
    kill "${QEMU_PID}" 2>/dev/null || true
    wait "${QEMU_PID}" 2>/dev/null || true
  fi
  rm -rf "${TEMP_DIR}"
}
trap cleanup EXIT

"${TOIT}" compile -Werror -s \
  -o "${TEMP_DIR}/boot.snapshot" \
  "${ROOT_DIR}/tests/toit/boot.toit"
"${TOIT}" tool snapshot-to-image -m32 --format=binary \
  -o "${TEMP_DIR}/boot.image" \
  "${TEMP_DIR}/boot.snapshot"
"${TOIT}" tool firmware --envelope="${TOIT_C3_ENVELOPE}" container install \
  --output="${TEMP_DIR}/boot.envelope" \
  boot-test "${TEMP_DIR}/boot.image"
"${TOIT}" tool firmware --envelope="${TEMP_DIR}/boot.envelope" extract \
  --format=image \
  --output="${TEMP_DIR}/boot.bin"

"${QEMU_SYSTEM_RISCV32}" \
  -M esp32c3 \
  -accel tcg,thread=single \
  -nographic \
  -no-reboot \
  -drive "file=${TEMP_DIR}/boot.bin,if=mtd,format=raw" \
  -global driver=timer.esp32c3.timg,property=wdt_disable,value=true \
  >"${TEMP_DIR}/qemu.log" 2>&1 &
QEMU_PID="$!"

PASSED=false
for ((tick = 0; tick < QEMU_TIMEOUT_TICKS; tick++)); do
  if grep -q '^TOIT-QEMU-BOOT: PASS' "${TEMP_DIR}/qemu.log"; then
    PASSED=true
    break
  fi
  if ! kill -0 "${QEMU_PID}" 2>/dev/null; then
    break
  fi
  sleep 0.1
done

cat "${TEMP_DIR}/qemu.log"

if [[ "${PASSED}" != true ]]; then
  echo "ESP32-C3 boot smoke test failed." >&2
  exit 1
fi
