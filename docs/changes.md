# LoRa reliability and tooling changes

Context: the station would stop transmitting and never recover. Root cause was that
`loraSend()` treated modem silence as success, so a dropped link looked healthy and
no recovery ever ran.

**None of this is verified on hardware yet.** It compiles clean and has not been flashed.

## LoRa driver — `Core/Src/lora.c`

| Change | Why |
|---|---|
| `loraSend()` requires `LORA_STATUS_ACK_RECEIVED` | `CMSGHEX` is a *confirmed* uplink, but the old code accepted `+CMSG: Done`, which the modem prints whether or not the network acknowledged. Silence previously returned `LORA_SENT_OK`. |
| `+JOIN: Done` no longer means "joined" | The modem prints it after a *failed* join too. Success now requires `NetID` / `Network joined`. Added `LORA_STATUS_JOIN_DONE` for the terminator. |
| Parser uses set/clear instead of whole-word assignment | An async `+JOIN` line arriving mid-send used to wipe the pending send status. |
| `HAL_UART_ErrorCallback` clears error flags and re-arms RX | HAL disables reception after a blocking error and nothing restarted it. Also set the flag on `loraStatus`, not `g_LoraStatus` — the two were different variables, so the recovery path in `tasksLoop` was dead code. |
| Bounded RX line assembly | A line ≥200 chars wrapped the index without NUL-terminating, so `strcpy` read past the buffer into a 200-byte ISR stack array. |
| Added missing `\n` to `AT+LOWPOWER` and `AT+ID` | Unterminated commands concatenated into one invalid line, e.g. `AAT+IDAT+CMSGHEX=...`. |
| `snprintf` instead of `strcpy`/`strcat` | Bounded formatting of EUI/KEY/payload commands. |
| `loraSetup()` verifies the join | Previously returned `LORA_OK` unconditionally; nothing ever checked `LORA_STATUS_JOIN_OK`. Auto-join is re-enabled either way. |
| `loraInit()` always clears the critical-error bit | Nothing cleared it, so once set `tasksLoop` would re-enter `loraInit()` forever. |
| `command()` flags failed `HAL_UART_Transmit`, polls at 100 ms | Was 1 s granularity and ignored the TX return code. |

### Parser performance

`parseRecvStatus` runs in the UART ISR. At 9600 baud on a **6 MHz** core
(`RCC_HSI_DIV4` → PLL in 4 MHz, ×6/4) the budget is ~6250 cycles per byte, and
`huart1` has `UART_ADVFEATURE_OVERRUN_DISABLE` — so overrunning that budget loses
bytes *silently*, with no error raised.

- Line classification switched from `strstr` to `strncmp` at offset 0 (O(1) instead of scanning the whole line), with a leading CR/LF/space skip so a stray character can't break it.
- `+CMSG` checks reordered so the common `Done` / `Received` outcomes match first.
- Removed a 200-byte `memset` per line and a per-byte `memset` of `UartRxChar`.

`strchr(buf, '+')` was left as a full scan deliberately: narrowing it to `buf[0]`
risks busy never clearing, which would stall the 15-command init sequence for 75 s.

## Task loop — `Core/Src/tasks.c`

- **Tick-wrap hang fixed.** `if (g_lastMeasureTicks < 0 || tick < g_lastMeasureTicks) return;` — the first half was dead (unsigned), the second latched true for ~49 days at each `HAL_GetTick()` wrap, halting measurement *and* transmission. Unsigned subtraction already handles wrap.
- **Consecutive-failure counter.** A failed uplink now just retries next cycle; only `MAX_SEND_FAILURES` (3) in a row triggers the reset+rejoin, so a transient busy modem or duty-cycle rejection doesn't cause a disproportionate ~2 min blocking recovery.
- `g_lastSendTicks` changed from `long` to `uint32_t`.

## Watchdog — `Core/Src/utils.c`

`watchdogInit()` / `watchdogRefresh()`, ~28 s IWDG, written at register level because
`stm32l0xx_hal_iwdg.c` is not in the project and the `.ioc` has no IWDG — this avoids a
CubeMX regeneration. Refreshed from `tasksLoop` and from every blocking wait in the LoRa
driver.

Last-resort recovery: after `MAX_SILENCE_TICKS` (1 h) with no confirmed uplink,
`tasksLoop` stops refreshing and lets the watchdog reset the MCU.

`WATCHDOG_DISABLED` in `parameters.h` is available for step debugging, though OpenOCD's
`STOP_WATCHDOG 1` already freezes the IWDG while halted.

## Wind measurement — `Core/Src/hall.c`

Counter wrap was `65535 - LastPulses + Pulses`; corrected to `65536`. One pulse lost per wrap.

Pulses are counted by TIM2 in hardware (`TIM_CLOCKSOURCE_ETRMODE2`), so a blocking LoRa
send does **not** lose them — `calculateRPS` normalises over the real elapsed period.
Only gust resolution suffers, since a stretched window reports its mean.

## Timing summary

| Constant | Value |
|---|---|
| `COMMAND_TMO` | 5 s |
| `LORA_SEND_TMO` | 25 s |
| `JOIN_TMO` | 25 s |
| `LORA_INIT_RETRIES` | 1 |
| `MAX_SEND_FAILURES` | 3 |
| `MAX_SILENCE_TICKS` | 1 h |
| IWDG | ~28 s |

Recovery ladder: failures at 5/10 min are ignored → 15 min triggers reset+rejoin →
60 min without a confirmed uplink triggers a watchdog reset.

## Build tooling

- `Makefile` — standalone build using the CubeIDE toolchain, flags taken from `.cproject`. Plugin paths are globbed so a CubeIDE update doesn't break it. `make`, `make clean`, `make DEBUG=0` for the `-Os` build.
- Flashing: `make flash` (builds first), `make flash-only` (flashes `build/` as-is), `make reset`.
- `.vscode/tasks.json` — Build (default), Clean, Rebuild, Flash, Flash only, Reset target.
- `.vscode/launch.json` — two `cppdbg` configs (work with the installed cpptools) and one cortex-debug config (needs `marus25.cortex-debug`).
- `.vscode/c_cpp_properties.json` — fixes the missing-system-header IntelliSense errors.
- `.vscode/settings.json` — puts make, the toolchain and OpenOCD on the PATH for integrated terminals. **Requires a new terminal to take effect.**
- `.gitignore` — `build/`.

## Debug-only switches in `parameters.h`

| Define | Effect |
|---|---|
| `WATCHDOG_DISABLED` | stops the IWDG so halting doesn't trigger a reset |
| `DEBUG_SLEEP_ENABLED` | keeps the core clock running in `WFI` so the debugger stays in sync |

**Both must be commented out for field builds.** See `docs/debugging-stlink.md`.

## Hardware verification

First on-target run confirmed `loraStatus = 0xa6`:

| Bit | Flag | |
|---|---|---|
| 0x02 | SENT_DONE | set |
| 0x04 | JOIN_OK | set |
| 0x20 | ACK_RECEIVED | set |
| 0x80 | JOIN_DONE | set |
| 0x01 / 0x40 | SENT_ERROR / CRITICAL_ERROR | clear |

So the modem joined, sent a confirmed uplink and received the network ACK, with
`huart1.ErrorCode = 0` and `gState = READY`. The ACK requirement — the most speculative
change here — works against a real gateway.

Current size: `text 50380, data 112, bss 3600` — 50 KB of 64 KB flash, 3.7 KB of 8 KB RAM.

## Open items

- **Downlink dependency.** Requiring the ACK means a site with good uplink but marginal downlink will now read as a dead link and reset the modem every ~15 min. `AT+ADR=ON` should walk the rate down and recover; watch for reset cycling with traffic still arriving at the network server.
- **`AT+RETRY` is never set**, so the retransmission count is firmware-default. Now that the ACK is load-bearing, setting it explicitly would make timing deterministic.
- **`busy` string matching is unverified.** Kept in the `+CMSG` branch (harmless if the wording differs — the timeout catches it), removed from `+JOIN` where treating it as failure would abort a join already in progress.
- **Consider re-enabling overrun detection** on `huart1` so byte loss becomes a detectable error instead of silent corruption.
- `DEBUG_LORA` does a blocking 115200 transmit per received byte inside the ISR (~8% of the per-byte budget). Turn it and `AT+LOG=DEBUG` off for field builds.
