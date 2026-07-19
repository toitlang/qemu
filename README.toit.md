# QEMU for Toit

This file documents why this QEMU fork exists, where its ESP32 support comes
from, what has been verified with Toit, and which local changes must be
maintained. The upstream QEMU documentation remains in [README.rst](README.rst).

## Goal

The primary goal is a deterministic, headless ESP32 emulator for Toit tests and
CI. In particular, it should be possible to:

- build a Toit container with the public `toit` command;
- install it in an envelope published by
  [`toitlang/envelopes`](https://github.com/toitlang/envelopes/releases);
- boot the resulting flash image without physical hardware;
- test WiFi association, DHCP, and TCP traffic locally; and
- download prebuilt QEMU executables instead of compiling QEMU in every CI job.

Accurate simulation of every ESP32 peripheral is useful, but secondary to a
stable CI environment for Toit.

## Provenance

This repository has gone through three relevant stages.

### Original Toit fork

The repository originally followed
[`espressif/qemu`](https://github.com/espressif/qemu). The old Toit commits were
based on Espressif's QEMU 7.2 development branch and primarily added GitHub
Actions builds, release artifacts, libslirp support, and Windows builds. That
experiment was archived before it became a routinely used Toit test platform.

The surviving old work can be found on the `floitsch/win` branch. It is useful
as historical context, but it is not the base of the current work.

### PICSimLab investigation

The `toitlang` branch follows the `picsimlab-esp32` branch from
[`lcgamboa/qemu`](https://github.com/lcgamboa/qemu). PICSimLab added a simulated
802.11 access point and an `esp32_wifi` network device. Current Toit ESP32 WiFi
was verified on that branch, including scanning, association, DHCP, TCP, and an
HTTP request through QEMU's user-mode network.

PICSimLab's ESP32-C3 machine can boot current Toit, but its WiFi register model
does not match the current ESP-IDF WiFi driver blob. Its ESP32-S3 machine does
not instantiate or connect the WiFi device.

### Current MicroPythonOS base

The maintained `main` branch starts from commit
`121833aa6e71b69b5edf8f8f3e19a14bb8d7cadc` of the `esp-develop-9.2` branch in
[`MicroPythonOS/qemu`](https://github.com/MicroPythonOS/qemu). It identifies as
QEMU 9.2.2.

MicroPythonOS started from Espressif's QEMU 9.2 work and added, among other
things:

- WiFi emulation for ESP32 and ESP32-S3;
- ESP32-S3 WiFi DMA and receive metadata;
- the S3 integration for Espressif's QPI and OPI PSRAM model, with adjusted
  PSRAM identification;
- ULP, light-sleep, deep-sleep, timer wake-up, and reset improvements;
- GPIO, IO-mux, pull-up, RMT, LEDC, MCPWM, touch, and RTC GPIO work;
- SPI flash and QIO fixes;
- ST7789, RGB LED, MPU6050, servo, and TTGO board simulations; and
- Linux, macOS, and Windows build packaging.

The display skins and interactive board simulations are not required for Toit
CI. The WiFi, PSRAM, GPIO, sleep/reset, and SPI changes are the most relevant
parts for this fork.

## Local Toit changes

The current Toit work adds or changes the following on top of the imported
MicroPythonOS commit:

- PSRAM reads and writes reject negative computed addresses. Without this
  bounds check, QEMU can segfault in `ssi_psram.c`, including after an otherwise
  successful ESP32-S3 WiFi test.
- Quad-PSRAM reads use the normal data path only while processing a read
  command, rather than incorrectly nesting that path under the read-ID state.
- C3-specific hardcoded eFuse reads are removed from the generic Espressif
  eFuse device. The ESP32-C3 subclass supplies a default revision 0.3 identity
  and a non-zero MAC address instead.
- ESP32-S3 octal flash/PSRAM pad and block-revision defaults live in the S3
  eFuse subclass instead of leaking into the shared C3/S3 eFuse model.
- ESP32-C3 TWAI sources are included when building the RISC-V target.
- `tests/toit` contains boot and WiFi smoke tests that build their own Toit
  containers and flash images. They use single-threaded TCG to avoid a
  multi-vCPU translation race observed with this fork's S3 machine.

The complete Toit patch stack can be reviewed independently of the imported
base with `git diff 121833aa6e..main`.

## Verified status

Last verified on 2026-07-19 with Toit SDK `v2.0.0-alpha.195`:

| Target | Status | Notes |
| --- | --- | --- |
| ESP32 | WiFi passed on the current base | Scan, association, DHCP, TCP, and HTTP passed. |
| ESP32-S3 | WiFi passed on the current base | Requires an octal-PSRAM envelope. Toit used SPIRAM for the heap; scan, association, DHCP, TCP, and HTTP passed. |
| ESP32-C3 | Boot only | Current Toit boots, but the MicroPythonOS fork does not connect its WiFi device to the C3 machine. |

ESP32-S3 must use an octal-PSRAM envelope, such as
`firmware-esp32s3-spiram-octo.envelope.gz`. A normal
`firmware-esp32s3.envelope` expects quad PSRAM and is not a valid match for this
machine model.

The installed `toit` SDK and the envelope must have identical SDK versions.

## Building

The CI-oriented build needs the Xtensa and RISC-V system emulators, SLIRP, and
libgcrypt:

```sh
mkdir build
cd build
../configure \
  --target-list=xtensa-softmmu,riscv32-softmmu \
  --enable-slirp \
  --enable-gcrypt
ninja qemu-system-xtensa qemu-system-riscv32
```

The resulting programs are:

- `qemu-system-xtensa` for ESP32 and ESP32-S3; and
- `qemu-system-riscv32` for ESP32-C3.

## Toit smoke tests

See [tests/toit/README.md](tests/toit/README.md) for complete instructions.

In summary:

```sh
# ESP32 WiFi.
TOIT_WIFI_ENVELOPE=/path/to/firmware-esp32.envelope \
  tests/toit/run-wifi-test.sh esp32

# ESP32-S3 WiFi with octal PSRAM.
TOIT_WIFI_ENVELOPE=/path/to/firmware-esp32s3-spiram-octo.envelope \
  tests/toit/run-wifi-test.sh esp32s3

# ESP32-C3 boot-only test.
TOIT_C3_ENVELOPE=/path/to/firmware-esp32c3.envelope \
  tests/toit/run-c3-boot-test.sh
```

The WiFi tests use the simulated open SSID `Open Wifi`. QEMU's SLIRP network is
`192.168.4.0/24`, the host is reachable from the guest as `192.168.4.2`, and
the test HTTP server defaults to port 18080. The tests do not require Internet
access.

## Known limitations

- The access points are hardcoded and open; WPA authentication is not
  simulated.
- The WiFi model implements only the behavior needed by the ESP-IDF driver and
  is not a general 802.11 simulator.
- ESP32-C3 WiFi is not supported on the current base.
- ESP32-S3 currently models octal PSRAM and must be paired with a matching Toit
  envelope.
- Several MicroPythonOS board peripherals are experimental or disabled in the
  S3 machine.
- The imported fork contains large graphical board skins and unrelated
  packaging changes. These should not become dependencies of headless CI.
- Interactive display and browser-based simulation are future work; the
  current test interface is intentionally headless and scriptable.

## Releases and setup action

GitHub Actions builds these headless archives on every push and pull request:

- Linux x86-64 and ARM64;
- Windows x86-64; and
- macOS Intel and ARM64.

Each archive contains both system emulators, ESP32 firmware data files, bundled
runtime libraries, a `VERSION` file, and the license. Linux x86-64 runs all
three Toit smoke tests against the packaged binaries. A `v*` tag publishes the
archives and a `SHA256SUMS` file as a GitHub release; `workflow_dispatch`
builds the same release bundle without publishing it.

Archives have a common top-level layout:

```text
qemu-toit-<version>-<platform>/
  bin/qemu-system-xtensa[.exe]
  bin/qemu-system-riscv32[.exe]
  lib/                         # Unix runtime libraries.
  share/qemu-firmware/esp32*.bin
  VERSION
  LICENSE
```

QEMU should have its own release version. It should not reuse Toit SDK version
numbers; compatibility is established by tests and release notes.

The existing Toit action is
[`toitlang/action-setup`](https://github.com/toitlang/action-setup). QEMU should
preferably use a separate action, for example `toitlang/action-qemu-setup`,
because the SDK and emulator have independent versions and use cases. A
workflow can then compose them:

```yaml
- uses: toitlang/action-setup@v1
  with:
    toit-version: latest

- uses: toitlang/action-qemu-setup@v1
  with:
    qemu-version: latest
```

The next deliverable is a separate `toitlang/action-qemu-setup` repository. It
should resolve an exact version or `latest`, select the archive for the runner,
verify `SHA256SUMS`, cache or extract it, add its `bin` directory to `PATH`,
and expose the installed version plus both executable paths as outputs.

## Maintenance guidance

Prefer small, reviewable commits and tests over importing another large board
simulation change. In particular:

1. Keep the Toit smoke tests passing for every supported target.
2. Preserve headless builds with SLIRP networking.
3. Separate fixes needed by Toit from optional GUI and board models.
4. Record the upstream or fork commit whenever the base is updated.
5. Test release archives exactly as downstream GitHub Actions users will run
   them.
