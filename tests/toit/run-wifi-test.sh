#!/usr/bin/env bash

# Copyright (C) 2026 Toit contributors.
# Use of this source code is governed by an MIT-style license that can be
# found in the LICENSE file.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TARGET="${1:-}"
QEMU_SYSTEM_XTENSA="${QEMU_SYSTEM_XTENSA:-${ROOT_DIR}/build/qemu-system-xtensa}"
TOIT_WIFI_ENVELOPE="${TOIT_WIFI_ENVELOPE:-}"
TOIT="${TOIT:-toit}"
HOST_HTTP_PORT="${HOST_HTTP_PORT:-18080}"
QEMU_TIMEOUT_TICKS="${QEMU_TIMEOUT_TICKS:-450}"

case "${TARGET}" in
  esp32)
    MACHINE="esp32"
    WDT_DRIVER="timer.esp32.timg"
    ;;
  esp32s3)
    MACHINE="esp32s3"
    WDT_DRIVER="timer.esp32s3.timg"
    ;;
  *)
    echo "Usage: $0 {esp32|esp32s3}" >&2
    exit 2
    ;;
esac

if [[ ! -x "${QEMU_SYSTEM_XTENSA}" ]]; then
  echo "QEMU_SYSTEM_XTENSA is not executable: ${QEMU_SYSTEM_XTENSA}" >&2
  exit 2
fi

if [[ -z "${TOIT_WIFI_ENVELOPE}" || ! -f "${TOIT_WIFI_ENVELOPE}" ]]; then
  echo "Set TOIT_WIFI_ENVELOPE to a current ${TARGET} WiFi envelope." >&2
  exit 2
fi

TEMP_DIR="$(mktemp -d)"
HTTP_PID=""
QEMU_PID=""

cleanup() {
  if [[ -n "${QEMU_PID}" ]]; then
    kill "${QEMU_PID}" 2>/dev/null || true
    wait "${QEMU_PID}" 2>/dev/null || true
  fi
  if [[ -n "${HTTP_PID}" ]]; then
    kill "${HTTP_PID}" 2>/dev/null || true
    wait "${HTTP_PID}" 2>/dev/null || true
  fi
  rm -rf "${TEMP_DIR}"
}
trap cleanup EXIT

"${TOIT}" compile -Werror -s \
  -o "${TEMP_DIR}/wifi.snapshot" \
  "${ROOT_DIR}/tests/toit/wifi.toit"
"${TOIT}" tool snapshot-to-image -m32 --format=binary \
  -o "${TEMP_DIR}/wifi.image" \
  "${TEMP_DIR}/wifi.snapshot"
"${TOIT}" tool firmware --envelope="${TOIT_WIFI_ENVELOPE}" container install \
  --output="${TEMP_DIR}/wifi.envelope" \
  wifi-test "${TEMP_DIR}/wifi.image"
"${TOIT}" tool firmware --envelope="${TEMP_DIR}/wifi.envelope" extract \
  --format=image \
  --output="${TEMP_DIR}/wifi.bin"

mkdir "${TEMP_DIR}/http"
python3 -m http.server "${HOST_HTTP_PORT}" \
  --bind 127.0.0.1 \
  --directory "${TEMP_DIR}/http" \
  >"${TEMP_DIR}/http.log" 2>&1 &
HTTP_PID="$!"

HTTP_READY=false
for _ in {1..50}; do
  if (exec 3<>"/dev/tcp/127.0.0.1/${HOST_HTTP_PORT}") 2>/dev/null; then
    exec 3>&-
    exec 3<&-
    HTTP_READY=true
    break
  fi
  sleep 0.1
done
if [[ "${HTTP_READY}" != true ]]; then
  echo "Local HTTP server did not start." >&2
  cat "${TEMP_DIR}/http.log" >&2
  exit 1
fi

"${QEMU_SYSTEM_XTENSA}" \
  -M "${MACHINE}" \
  -accel tcg,thread=single \
  -nographic \
  -no-reboot \
  -drive "file=${TEMP_DIR}/wifi.bin,if=mtd,format=raw" \
  -global "driver=${WDT_DRIVER},property=wdt_disable,value=true" \
  -nic "user,model=esp32_wifi,net=192.168.4.0/24" \
  >"${TEMP_DIR}/qemu.log" 2>&1 &
QEMU_PID="$!"

PASSED=false
for ((tick = 0; tick < QEMU_TIMEOUT_TICKS; tick++)); do
  if grep -q '^TOIT-QEMU-WIFI: PASS' "${TEMP_DIR}/qemu.log" &&
      grep -q '"GET / HTTP/1.0" 200' "${TEMP_DIR}/http.log"; then
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
  echo "${TARGET} WiFi smoke test failed." >&2
  cat "${TEMP_DIR}/http.log" >&2
  exit 1
fi
