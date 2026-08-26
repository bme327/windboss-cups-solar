# Windboss L051

Firmware for a solar-powered, battery-backed weather station built around an
STM32L051K8 microcontroller. The station measures wind speed and wind direction
together with ambient temperature, barometric pressure and relative humidity, and
reports the aggregated results over a LoRaWAN uplink.

It is designed for unattended outdoor operation: it spends most of its time
asleep, wakes on a fixed measurement cadence, and is protected by a hardware
watchdog and a staged recovery sequence so that a lost radio link or a stalled
peripheral cannot leave it permanently silent.

## How It Works

### Measurement

- **Wind speed** is derived from the rotating **wind cups**. Each revolution
  produces pulses that are counted by TIM2 in hardware, clocked directly from the
  external pulse input. Because counting happens in the timer peripheral rather
  than in software, pulses are preserved even while the CPU is asleep or blocked
  in a radio transmission. Rotations per second are normalised over the actual
  elapsed period, and both a running average and a peak gust value are tracked.
- **Wind direction** comes from the **wind rose**, whose shaft carries a magnet
  read by a 3D Hall-effect sensor over I2C. The sensor reports a magnetic angle
  that is corrected by a stored offset and accumulated as a vector average, so
  readings around the 0°/360° boundary average correctly.
- **Temperature, pressure and humidity** are read from a BME280 on the same I2C
  bus, sampled once per transmission cycle.
- **Battery and solar voltages** are sampled through the ADC via resistive
  dividers. Readings are scaled using the internal reference voltage and its
  factory calibration value, so results stay accurate as the supply rail sags.

### Reporting

Measurements are taken on a short cycle and accumulated. On the longer
transmission cycle the accumulated values are packed into a **Cayenne Low Power
Protocol** payload — protocol version, average wind speed, gust, average
direction, humidity, temperature, pressure, battery and solar level — then
hex-encoded and handed to the radio as a confirmed uplink.

The radio is a **Seeed LoRa-E5** module driven over UART with AT commands. It
joins using OTAA on the EU868 band with adaptive data rate enabled, and is
returned to low-power mode between transmissions.

### Reliability

An uplink counts as successful only when the network acknowledgement is received;
a `Done` response from the modem alone is not treated as delivery. Failures
escalate in stages:

1. Isolated failures are ignored and retried on the next cycle.
2. A run of consecutive failures triggers a modem reset and rejoin.
3. If no acknowledged uplink is received for an extended period, the firmware
   stops refreshing the independent watchdog and lets it reset the MCU.

The wind rose direction offset is persisted to the MCU's internal EEPROM, so
calibration survives resets and power cycles.

## Technology

| Area | Details |
| --- | --- |
| MCU | STM32L051K8Tx, Arm Cortex-M0+, 64 KB flash, 8 KB RAM |
| Framework | STM32 HAL and CMSIS, configured through STM32CubeMX (`.ioc`) |
| Language | C |
| Radio | Seeed LoRa-E5 LoRaWAN module, UART AT interface, OTAA, EU868 |
| Payload | Cayenne LPP, hex-encoded confirmed uplink |
| Sensors | 3D Hall-effect angle sensor (I2C), BME280 (I2C), pulse input via TIM2 |
| Power | Sleep between cycles, ADC monitoring of battery and solar rails |
| Safety | Independent watchdog (IWDG), staged link recovery |
| Storage | Internal EEPROM for calibration settings |
| Build | GNU Make driving the STM32CubeIDE GNU Arm toolchain |
| Flash/debug | OpenOCD with an ST-Link adapter |

## Repository Layout

| Path | Contents |
| --- | --- |
| `Core/Inc`, `Core/Src` | Application code: measurement, radio, tasks, drivers |
| `Core/Startup` | Startup assembly |
| `Drivers` | STM32 HAL and CMSIS |
| `docs` | Change notes and debugging guides |
| `hw` | Hardware design files |
| `build` | Build output, not tracked |

Tunable behaviour — measurement and transmission cycles, LoRa credentials,
calibration mode and debug switches — lives in `Core/Inc/parameters.h`.

## Development

### Requirements

- STM32CubeIDE, which supplies the GNU Arm toolchain and OpenOCD used by the build
- GNU Make
- An ST-Link programmer/debugger

The `Makefile` defaults to STM32CubeIDE 2.2.0 on Windows and globs the versioned
plugin directories, so a CubeIDE update does not break it. Override
`CUBEIDE_PLUGINS` if CubeIDE is installed elsewhere.

### Common Commands

```powershell
make -j8          # debug build (-Og, DEBUG defined)
make DEBUG=0 -j8  # size-optimized build (-Os)
make clean
make flash        # build, flash, verify and reset
make flash-only   # flash the existing build output
make reset
```

Artifacts (`.elf`, `.hex`, `.bin`, `.map`) are written to `build/`. The same
build, flash, reset and debug actions are available as VS Code tasks and launch
configurations.

### Debug Switches

Debug features are grouped behind `DEBUG_ENABLED` in `Core/Inc/parameters.h`:
serial debug output, LoRa traffic logging, watchdog suspension while single
stepping, and keeping the core clock alive in sleep so halting inside `WFI` is
safe. A separate calibration mode writes the wind rose offset to EEPROM.

**Keep these disabled for field builds.** With the core clock stopped in sleep a
debugger cannot attach reliably; see
[`docs/debugging-stlink.md`](docs/debugging-stlink.md) for connection recovery
steps and known OpenOCD behaviour.

## Contributing

1. Create a branch from `main` and keep each change focused on one concern.
2. Build the firmware and resolve any warnings your change introduces. Watch the
   reported size — the application already occupies a large share of the
   available flash and RAM.
3. Test on the target where the change touches hardware behaviour, and state
   clearly in the pull request what was verified on device and what was not.
4. Confirm that debug switches and test-only cycle timings are not left enabled.
5. Open a pull request for review. Changes from other contributors require the
   code owner's approval before merging.

Additional notes:

- Do not commit anything from `build/`.
- Keep code comments short; record longer reasoning in `docs/`.
- Regenerating from STM32CubeMX overwrites parts of `Core`, so prefer changes
  that do not require it, and call it out explicitly when it is unavoidable.
- Timing-sensitive code runs in interrupt context. The UART receive path has a
  tight per-byte budget and overruns are not reported, so keep added work there
  minimal.

## Documentation Status

Firmware build, debugging and change notes are available under `docs/`.

**Hardware documentation and 3D design documentation are work in progress** —
schematics, wiring, sensor mounting, enclosure and mechanical drawings for the
wind cups and wind rose assemblies are not yet documented.