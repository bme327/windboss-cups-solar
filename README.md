# Windboss L051 Turbine

Firmware for an STM32L051K8-based wind turbine monitoring station. It reads the
connected sensors, calculates wind measurements, and reports telemetry over LoRa.

## Development

### Requirements

- STM32CubeIDE with its GNU Arm toolchain and OpenOCD plugins
- GNU Make
- An ST-Link programmer/debugger for target access

The default toolchain path in the `Makefile` targets STM32CubeIDE 2.2.0 on
Windows. Override `CUBEIDE_PLUGINS` when CubeIDE is installed elsewhere.

```powershell
make -j8          # debug build
make DEBUG=0 -j8  # size-optimized build
make clean
make flash        # build, flash, verify, and reset
make flash-only   # flash the existing build
make reset
```

Build artifacts are written to `build/`. Equivalent build, flash, reset, and
debug actions are available as VS Code tasks and launch configurations.

Source code is under `Core/`; STM32 HAL and CMSIS dependencies are under
`Drivers/`. See [`docs/debugging-stlink.md`](docs/debugging-stlink.md) for ST-Link
connection and debugging guidance.

## Contributing

1. Create a branch from `main` and keep changes focused.
2. Build the firmware and resolve compiler warnings introduced by the change.
3. Test hardware-dependent behavior on the target when possible, and state what
   was or was not verified in the pull request.
4. Open a pull request for review. Changes from other contributors require the
   code owner's approval before merging.

Do not commit files from `build/`. Keep debug-only switches disabled in field
builds, and document notable behavior changes under `docs/`.

## Documentation Status

Firmware build and debugging notes are available under `docs/`. Hardware design,
wiring, assembly, enclosure, and 3D design documentation are work in progress.