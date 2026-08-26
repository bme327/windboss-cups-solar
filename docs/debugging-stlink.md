# ST-Link debugging: "Unable to start debugging. No process is associated with this object."

VS Code reports that message when the debug server exits immediately. The real error is
always from OpenOCD, not from the extension — run OpenOCD by hand to see it.

## Environment

STM32L051K8Tx, ST-Link/V2 (`VID:PID 0483:3748`, firmware `V2J43S7`), tooling from
`C:\ST\STM32CubeIDE_2.2.0`.

## Diagnosis

```powershell
$ocd = "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.openocd.win32_2.4.500.202604080855\tools\bin\openocd.exe"
$scr = "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.debug.openocd_2.3.400.202606220929\resources\openocd\st_scripts"
& $ocd -s $scr -f windboss-l051-turbine.cfg -c "init" -c "targets" -c "shutdown"
```

Good output names the CPU and shows the GDB server listening:

```
Info : SWD DPIDR 0x0bc11477
Info : [STM32L051K8Tx.cpu] Cortex-M0+ r0p1 processor detected
Info : Listening on port 3333 for gdb connections
```

If that fails, cross-check with ST's own tool, which gives clearer errors:

```powershell
& "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.500.202603051304\tools\bin\STM32_Programmer_CLI.exe" -c port=SWD mode=UR freq=4
```

Success prints `Device ID : 0x417` (STM32L0x1).

## What happened here

| Attempt | Result |
|---|---|
| OpenOCD, project cfg (`CLOCK_FREQ 8000`, clamped to 4 MHz) | `init mode failed (unable to connect to the target)` |
| OpenOCD @ 240 kHz, `reset_config none` | same |
| CubeProgrammer HOTPLUG / UR / POWERDOWN @ 240 kHz | `Unable to get core ID` |
| **CubeProgrammer UR @ 4 kHz** | **OK, Device ID 0x417** |
| Everything after that: 15 kHz → 950 kHz, and OpenOCD @ 4 MHz | all OK |

Speed alone does not explain it — 240 kHz failed before the 4 kHz connect and passed
after. What mattered was landing **one** successful connect while the part was held in
reset.

### Root cause

`tasksLoop()` calls `HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI)`
every pass, so the core is asleep most of the time and the initial attach has a narrow
window. The fix for that is the `DBGMCU` low-power debug bits, which keep the debug clock
alive through `WFI` — and OpenOCD only sets them (`ENABLE_LOW_POWER 1` in the cfg) *after*
connecting. Chicken-and-egg.

Once any debugger gets in and sets those bits, they persist until reset or power-cycle,
which is why every later attempt succeeded at any speed.

### Ruled out

- **SWD pins repurposed** — `MX_GPIO_Init()` only configures PA6/PA7/PA8 and PB3. PA13/PA14 are untouched.
- **Stale processes** — no leftover `openocd` / `gdb`, port 3333 free.
- **Wrong config** — the unmodified project cfg works fine now, so `CLOCK_FREQ 8000` was left alone.

## Recurrence

The `DBGMCU` bits clear on power-cycle, so this can come back.

**Proper fix — wire NRST.** `windboss-l051-turbine.cfg` already has
`connect_assert_srst` and `CONNECT_UNDER_RESET 1`, which is exactly right for
WFI-heavy firmware; it just needs the pin connected. `mode=UR` failing identically to
`mode=HOTPLUG` is the tell that NRST is not wired.

**Workaround — force a slow under-reset connect** to unlock it, then debug normally:

```powershell
STM32_Programmer_CLI.exe -c port=SWD mode=UR freq=4
```

## Notes

- `Target voltage: 3.28V` is **not** proof a board is attached. This dongle reported ~3.28 V with nothing connected at all. VTREF is tied to its own rail.
- ST's OpenOCD fork rejects `hla_swd` with `target/stm32l0x.cfg` (`hla newtap` error) — that path is not a usable fallback here.
- `STOP_WATCHDOG 1` in the cfg freezes the IWDG while halted, so `WATCHDOG_DISABLED` in `parameters.h` is usually unnecessary.

---

# `make flash` fails with "Unable to reset target"

```
Info : [STM32L051K8Tx.cpu] Examination succeed
Error: timed out while waiting for target halted
** Unable to reset target **
```

Note that attach *worked* — the DAP was examined and the GDB server came up. Only the
reset failed, so this is not a connection problem and slower `CLOCK_FREQ` does not help.

## Cause

OpenOCD's `program` command always begins with `reset init`. That pulses NRST, which
clears `DBGMCU_CR`, so the low-power debug bits set by `ENABLE_LOW_POWER 1` at
`examine-end` are gone by the time the core boots. The firmware reaches the `WFI` in
`tasksLoop()` before OpenOCD can halt it, HCLK stops, and the halt request times out.

NRST *is* wired — the earlier "not wired" conclusion above was wrong. Asserting reset is
precisely what breaks this case.

## Fix

`halt` before letting `program` reset. A bare `halt` needs no vector catch and no reset,
and it pulls the core out of sleep, so the following `reset init` lands cleanly:

```make
-c "init" -c "halt" -c "program build/$(TARGET).elf verify"
```

This is what `make flash` / `make flash-only` now do. The trailing `reset exit` that
originally appeared here was removed later — see the next section.

### Ruled out

- **`halt` itself** — works from a cold, sleeping target every time; only `reset halt` races.
- **Adapter speed** — 4 MHz is fine; examination succeeds at that speed.
- **Skipping the reset entirely** (`flash write_image erase` on a halted target) — works,
  but the RAM loader can't run from a faulted core, so programming drops to page writes
  (~19 s) and the target is left in HardFault.

`Warn: target was in unknown state when halt was requested` is expected with
`connect_assert_srst`, and `Couldn't use loader` is a benign fallback to page writes.

---

# `make flash` never resets the target

Flashing reports `** Verified OK **` and `** Resetting Target **` and exits 0, but the
board stays dead until it is power-cycled.

## Cause

The `exit` option of `program`. The built-in proc runs `poll off` *before* its `reset run`:

```tcl
if {[info exists reset]} {
        if {$exit == 1} {
                # also disable target polling, we are shutting down anyway
                poll off
        }
        echo "** Resetting Target **"
        reset run
}
if {$exit == 1} {
        shutdown
}
```

The reset is issued at a target OpenOCD has stopped tracking, and `shutdown` drops the
connection immediately after. Note that `** Resetting Target **` is echoed
unconditionally, so it is never evidence that the core actually resumed.

Any built-in proc can be dumped to check this — no hardware needed:

```powershell
& $ocd -c "echo [info body program]" -c "shutdown"
```

## Fix

Keep `shutdown`, otherwise OpenOCD stays up as a GDB server on 3333 and `make` never
returns. Just issue the reset as its own polled step instead of via `exit`:

```make
-c "init" -c "halt" -c "program build/$(TARGET).elf verify" -c "reset run" -c "shutdown"
```

With polling left on, target state is reported after the reset instead of going silent.

## Do not trust post-flash state read through the project cfg

`windboss-l051-turbine.cfg` sets `connect_assert_srst` and `CONNECT_UNDER_RESET 1`, so
**every attach resets the chip**. Any "state after flashing" observed that way is the
result of your own attach, not of `make flash`. Confirm liveness from uplink data instead.

---

# HardFault only under a debug session


Symptom: the firmware HardFaults within minutes under the debugger, but a plain
`make flash` followed by a reset runs indefinitely.

## Cause

`tasksLoop()` ends with `HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI)`,
so the core is asleep most of the time. By default the debug clock stops in sleep mode.
Halting or stepping the core while it is in `WFI` desynchronises the debugger, and the
resumed PC can be garbage.

**The resulting fault frame is fiction.** Ours showed:

- PC in `.bss` at an even (non-Thumb) address
- an LR pointing at a `str` instruction, which no `BL` could ever have produced
- a near-empty stack (32 bytes below `_estack`)

None of it described a real call chain, and reconstructing one from the stack residue
produced entirely misleading answers. If those three signs appear together, suspect the
debugger before the code.

## The watchdog compounds it

`HardFault_Handler` is `while(1)` and never refreshes the IWDG, so ~28 s after any fault
the watchdog resets the MCU. That produces a crash → reset → crash loop which zeroes
`.bss` and destroys the fault context between observations. Landing in `Reset_Handler`
while stepping is the tell.

## Fix

Two switches in `Core/Inc/parameters.h`:

| Define | Effect |
|---|---|
| `DEBUG_SLEEP_ENABLED` | calls `HAL_DBGMCU_EnableDBGSleepMode()` in `tasksInit()`, keeping the core clock alive through `WFI` |
| `WATCHDOG_DISABLED` | stops the IWDG so a halt isn't cut short by a reset |

**Comment both out for field builds.** Without the watchdog there is no recovery from a
hang, and keeping the debug clock running in sleep costs battery.
