# Toit smoke tests

These tests build containers with the installed `toit` command, install them
in prebuilt firmware envelopes, and boot the resulting flash images in QEMU.
The installed SDK and every envelope must have the same SDK version.

The runners force single-threaded TCG for deterministic CI. This still
emulates both S3 guest cores, while avoiding a host-side multi-threaded TCG
race observed during S3 startup.

Build QEMU with Xtensa, RISC-V, SLIRP, and libgcrypt support:

```sh
mkdir build
cd build
../configure \
  --target-list=xtensa-softmmu,riscv32-softmmu \
  --enable-slirp \
  --enable-gcrypt
ninja qemu-system-xtensa qemu-system-riscv32
```

## ESP32 WiFi

The ESP32 test verifies scanning, association with the simulated open
`Open Wifi` access point, DHCP, and an HTTP request through QEMU's SLIRP
backend. The endpoint is local to the test runner, so Internet access is not
required.

```sh
export TOIT_WIFI_ENVELOPE=/path/to/firmware-esp32.envelope
tests/toit/run-wifi-test.sh esp32
```

## ESP32-S3 WiFi

ESP32-S3 must use an octal-PSRAM envelope. For example, download
`firmware-esp32s3-spiram-octo.envelope.gz` from the Toit envelopes release
whose version matches `toit version`.

```sh
export TOIT_WIFI_ENVELOPE=/path/to/firmware-esp32s3-spiram-octo.envelope
tests/toit/run-wifi-test.sh esp32s3
```

## ESP32-C3 boot

ESP32-C3 can boot current Toit firmware, but this fork does not connect the
WiFi model to the C3 machine. The boot-only smoke test keeps that supported
boundary explicit:

```sh
export TOIT_C3_ENVELOPE=/path/to/firmware-esp32c3.envelope
tests/toit/run-c3-boot-test.sh
```

Set `QEMU_SYSTEM_XTENSA`, `QEMU_SYSTEM_RISCV32`, or `TOIT` to override the
default executables. `HOST_HTTP_PORT` defaults to 18080. Test timeouts are
expressed as 100 ms polling ticks through `QEMU_TIMEOUT_TICKS`.
