# DIAG and status

The status registers, the DIAG pins and the one call that reads the events.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Analog Devices (Trinamic) TMC5160A-DS 1.18:

    - Global status and interface counter: 6.1 General Configuration Registers, p. 33
    - Ramp status and events: 6.3 Ramp Generator Registers, p. 44
    - Driver status: 6.5 Motor Driver Registers, p. 55
    - Temperature sensors and short protection: 11 Diagnostics and Protection, p. 78
    - Open load diagnostics: 11.3 Open Load Diagnostics, p. 80
    - DIAG outputs: 16 DIAG Outputs, p. 97

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Status

| Call | Register | Content |
| --- | --- | --- |
| `ReadGlobalStatus()` | GSTAT | `reset`, `drv_err`, `uv_cp` |
| `ReadDriverStatus()` | DRV_STATUS | StallGuard result, CS_ACTUAL, short, overtemperature, open load, standstill |
| `ReadRampStatus()` | RAMP_STAT | Stop events, position reached, velocity reached, latch ready |
| `ReadEncoderStatus()` | ENC_STATUS | N event, deviation warning |
| `ReadInputs()` | IOIN | Pin levels and the version byte |
| `ReadSnapshot()` | several | GSTAT, DRV_STATUS, RAMP_STAT, XACTUAL, VACTUAL, TSTEP, MSCNT in one batch |
| `Service()` | several | The event registers, decoded; the latched events are cleared |

The write-to-clear bits of RAMP_STAT are bits 12, 7, 6, 3 and 2 (mask `0x10CC`); the read-only status bits are never
written (A24).

## The SPI status byte

Every SPI reply starts with the status byte of the previous transfer (A3). The driver reads bit 0 (reset) and bit 1
(driver error) on every call: a reset flag while the driver believes the chip is configured returns `UnexpectedReset`
and makes the driver uninitialised; a driver error makes the motion calls return `DeviceFault` until `ClearFaults()`.
Bits 7 to 2 are exposed to the application and never gate a call.

## DIAG0 and DIAG1

`SetDiag()` chooses what the pins report. In motion controller mode DIAG0 can output the internal STEP and DIAG1 the
DIR; in STEP/DIR mode they report the error, the pre-warning, the stall, the index and the chopper on-state, and DIAG1
the skipped DcStep steps. Open-drain or push-pull is a bit per pin. The pins need a pull-up of 47 kOhm or less in open
drain (section 16, page 97). With a `GpioGet()` hook, `Diag0Active()` and `Diag1Active()` read the pin level, taking the
polarity into account.

```cpp
--8<-- "tests/docs/api_tasks.cpp:diag"
```

## Errors

A short to ground or supply, or an overtemperature, shuts the bridge down and sets GSTAT.drv_err: the driver stays off
until it is disabled and enabled again. `ClearFaults()` disables the driver (and stops the ramp generator), clears
GSTAT and enables it again when the cause is gone and the motor stands still (a motor that still turns leaves the driver
disabled and the call returns `Busy`: call `Enable()` later); while the overtemperature flag still reads set it
refuses. Open load flags are informative only and are not
reliable at standstill or in StealthChop (section 11.3, page 80).
