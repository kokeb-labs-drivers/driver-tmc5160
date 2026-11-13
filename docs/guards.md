# Guards and acknowledgements

Which writes the driver refuses, why, and the acknowledgement that lifts a guard for one configuration.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Analog Devices (Trinamic) TMC5160A-DS 1.18:

    - Sample circuits and gate drive: 3 Sample Circuits, p. 15
    - Selecting sense resistors and the current scale: 9 Selecting Sense Resistors, p. 74
    - Temperature sensors and short protection: 11 Diagnostics and Protection, p. 78
    - DC motor and direct mode: 21 DC Motor or Solenoid, p. 110
    - Absolute maximum ratings and operational range: 28.1 Operational Range, p. 120

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

A stepper driver has registers that can destroy the motor or the bridge. The driver never exposes them raw: the current
comes from the declared motor and sense resistor ([Current and limits](current-and-limits.md)), and every other
dangerous value has a guard. A guarded call returns an error and writes nothing.

## Acknowledgements

`Config::ack` is a set of `Ack` values. Each one lifts exactly one guard, and the configuration that carries it says in
code that the application accepted the risk.

| `Ack` | Lifts | Datasheet |
| --- | --- | --- |
| `HoldAboveRun` | IHOLD above IRUN | section 6.2, page 38 |
| `ShortProtectionOff` | `diss2g` or `diss2vs` (short protection disabled) | section 11.2, page 78 |
| `LowOtLevel` | The 120 degree shutdown level, or 136 degrees with a supply above 24 V | section 6.1, page 36 |
| `HardStopWithStealthChop` | A hard stop while StealthChop is enabled | section 6.3, page 43 |
| `VstartAboveVstop` | VSTART above VSTOP | section 6.3, page 41 |
| `DirectMode` | Direct mode (coil currents from XTARGET) | section 21, page 110 |
| `VoltageModeNoCurrentLimit` | Direct mode with a voltage-mode chopper, which has no current limit | section 21, page 110 |

```cpp
--8<-- "tests/docs/api_tasks.cpp:guard"
```

## The guards

| Guard | Rule | Result |
| --- | --- | --- |
| Current ceiling | Run and hold current, rounded down, at most the smaller of `max_rms_ma` and `rated_rms_ma`, reduced by the 5 percent tolerance | `LimitExceeded` |
| Sense resistor bound | The sense resistor must allow the request (`VFS / Rsense / sqrt(2)`) | `LimitExceeded` |
| No raw current registers | GLOBALSCALER, IHOLD and IRUN are set only from a current plan; GLOBALSCALER 129 to 256 | by type |
| Hold current | Hold at most run | `Refused` |
| Supply | `supply_mv` 10 000 to 55 000 | `InvalidArgument` |
| Bridge | Written only when declared; break-before-make at least 130 percent of the declared MOSFET switching time; `bbm_time` at most 24; only with the driver off | `LimitExceeded` |
| Short levels | `s2vs_level` 4 to 15, `s2g_level` 2 to 15, and 12 to 15 when the highest supply is above 52 V | `Refused` |
| Short protection off | `diss2g`, `diss2vs` | `Refused` |
| Overtemperature | 136 C above 24 V and 120 C | `Refused` |
| Chopper | `toff` 1 to 15 (0 only through `Disable()`); `toff` 1 needs `tbl` 2 or more; effective hysteresis sum at most 16 | `InvalidArgument` |
| Thresholds | TPWMTHRS at least TCOOLTHRS at least THIGH where non-zero; CoolStep and stop on stall need TCOOLTHRS | `InvalidArgument` |
| StealthChop | With autoscale: IRUN at least 8, TPOWERDOWN at least 2, the run current above the lower limit when the coil resistance is known; only at standstill (the check of `Enable()`) | `Refused`, `Busy` |
| StealthChop and stop | A hard stop with StealthChop enabled | `Refused` |
| Soft stop and stall | `soft_stop` together with `stop_on_stall` | `Refused` |
| DcStep | Needs the default table, `high_fullstep` and `high_const_off`, TOFF 3 or more, VDCMIN 0 or 256 or more | `InvalidArgument` |
| Ramp | VMAX within the register limit and `max_velocity_reg`; D1 and VSTOP at least 1; VSTOP at least VSTART | `LimitExceeded`, `InvalidArgument` |
| Motion | `MoveTo()` and `MoveBy()` need the nine ramp registers written and a VMAX that is not 0 (after `SetMaxVelocity(0)`, until `SetMaxVelocity()` or `SetRamp()` gives a speed again); `SetMaxVelocity()` refuses 0 < VMAX < VSTART (the chip's or the profile's) | `Refused`, `InvalidArgument` |
| Home position | `SetHomePosition()` needs standstill: vzero, no TZEROWAIT pause and, in position mode with VMAX not 0, the target reached (`Stop()` or `Hold()` ends a move a switch or stall holds) | `Busy` |
| Stall release | `ReleaseStall()` holds, verifies standstill, then clears the event; the motor never restarts by itself. Switching the stop off while its event is latched first ends the ramp at the present position | `Busy` |
| Microstep table | Only when MSCNT is 0; the decoded wave inside the allowed range; segments in order | `Refused` |
| Direct mode | `Ack::DirectMode`; entry refused while moving; currents within the range of the chopper mode | `Refused` |
| DIAG | Bits that belong to the other mode are refused; `steps_skipped` is exclusive | `InvalidArgument` |
| UART addressing | `send_delay` at least 2 with several nodes; node 0 to 253 (254 with `multi_node`); `ProgramNode()` over SPI | `Refused`, `Unsupported` |
| Clock | External clock 4 to 18 MHz; the SPI clock when the port reports it | `InvalidArgument` |
| Encoder | Encoder calls in STEP/DIR mode | `Unsupported` |

## Registers the driver never writes

OTP_PROG and FACTORY_CONF are never written (the owner excluded OTP programming; a test sends every call and checks the
register was not touched). GCONF.test_mode is never set. Read-only registers are never written, and reserved bits are
written as 0.

## After a reset

Reading a write-only register returns `Refused`: the chip's answer is undefined (A4). The driver keeps a write-through
shadow of those registers and never reads them back. A chip reset (undervoltage, power-up) clears them all:
`UnexpectedReset` is returned, the driver becomes uninitialised, and `Enable()` refuses until `EnsureInitialized()` has
rewritten everything.
