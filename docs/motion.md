# Motion

The ramp generator, the two motion modes, the units of the ramp registers and the calls that move and stop the motor.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Analog Devices (Trinamic) TMC5160A-DS 1.18:

    - Velocity dependent control: 6.2 Velocity Dependent Driver Feature Control Register Set, p. 38
    - Ramp generator registers: 6.3 Ramp Generator Registers, p. 40
    - Reference switches and SW_MODE: 6.3 Ramp Generator Registers, p. 43
    - Ramp status and events: 6.3 Ramp Generator Registers, p. 44
    - Real world unit conversion: 12.1 Real World Unit Conversion, p. 81
    - Motion profiles and early ramp termination: 12.2 Motion Profiles, p. 82
    - Velocity thresholds: 12.3 Velocity Thresholds, p. 84
    - Reference switches: 12.4 Reference Switches, p. 85
    - STEP/DIR interface and MicroPlyer: 15 STEP/DIR Interface, p. 94
    - Emergency stop: 19 Emergency Stop, p. 106
    - Clock oscillator and input: 26 Clock Oscillator and Input, p. 119

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Modes

| Mode | `Config::mode` | Strap | Moves by |
| --- | --- | --- | --- |
| Motion controller | `MotionController` | SD_MODE low | The internal ramp generator: `MoveTo()`, `MoveBy()`, `RunVelocity()` |
| STEP/DIR | `StepDir` | SD_MODE high | Pulses on STEP and DIR; MicroPlyer interpolates to 256 microsteps |

`EnsureInitialized()` reads IOIN and compares the SD_MODE strap with `Config::mode`; a mismatch returns `ModeMismatch`
and the chip is left with the driver off. In STEP/DIR mode the ramp calls return `Unsupported`.

## The ramp profile

`RampProfile` holds the six-point ramp of the datasheet (section 6.3, page 40): VSTART, A1, V1, AMAX, VMAX, DMAX, D1,
VSTOP and TZEROWAIT. `SetRamp()` writes all of them before any XTARGET, so a move never starts on half a profile.
The defaults are the datasheet's quick-start values. Velocities are in the chip's own unit (microsteps per `2^24`
clocks); acceleration in `fCLK^2 / (512 * 256 * 2^24)` per second.

```cpp
--8<-- "tests/docs/api_tasks.cpp:motion"
```

## Units

`units::VelocityRegFromHz(hz, fclk)` and `units::AccelRegFromHzPerS()` convert real-world values to register values,
with a 128-bit oracle in the tests. The relations (section 12.1, page 81): `v[Hz] = v_reg * fCLK / 2^24`,
`a[Hz/s] = a_reg * fCLK^2 / (512 * 256 * 2^24)`. With the internal 12 MHz clock one velocity count is 0.715 Hz. The
clock is `Config::clock`: an external clock changes every value.

## Calls

| Call | What it does |
| --- | --- |
| `MoveTo(position)` | Position mode, XTARGET = `position` |
| `MoveBy(delta)` | Position mode relative to XACTUAL; a target that overflows is `LimitExceeded` (after one read) |
| `RunVelocity(v)` | Velocity mode; the sign picks the direction (write order and the run-back bound below) |
| `SetMaxVelocity(v)` | VMAX while moving; above `Config::max_velocity_reg` it returns `LimitExceeded`, below VSTART (not 0) `InvalidArgument`; 0 writes VSTART 0 first (page 84) |
| `Hold()` | RAMPMODE Hold: the present velocity stays (a stop event still ends it); use `Stop()` to slow down |
| `Stop(kind)` | `Ramp`, `Immediate`, `HoldTarget` or `Emergency` (early ramp termination, section 12.2.4, page 83) |
| `SetHomePosition(x)` | XACTUAL and XTARGET together; needs standstill |
| `ReadPosition()`, `ReadVelocity()`, `ReadTarget()`, `PositionReached()` | The motion state |

`Emergency` switches the driver off (DRV_ENN and TOFF 0): the motor is not held, and the ramp generator is told to stop
(VMAX 0, velocity mode) so that a later `Enable()` does not resume the move; `Enable()` returns `Busy` until RAMP_STAT
shows vzero set, no TZEROWAIT pause and, in position mode with VMAX not 0, position_reached. On UART the TOFF 0 write
has no reply, so `Disable()` and `Emergency` read IFCNT afterwards (one extra transfer) and return `VerifyFailed` when
the chip did not count the write; DRV_ENN, when wired, is already high by then. Without a DRV_ENN hook a lost write
leaves the chopper on, which only the error tells. Writing XACTUAL in position mode starts a move, so
`SetHomePosition()` writes the hold mode first.

## Reference switches and homing

`SetSwitches()` programs SW_MODE: stop on the left or right switch, polarity, swap, and the latches. The latched
position is `ReadLatchedPosition()`. Homing is not a driver call: the right speed, back-off and sequence depend on the
machine. [`examples/host/homing_switch.cpp`](../examples/host/homing_switch.cpp) and `homing_stall.cpp` show both
procedures with the driver's primitives. They home in velocity mode. A position move that a switch or a stall stops
has not reached its target, so `SetHomePosition()` answers `Busy` for it until `Stop()` (any kind) or `Hold()` ends
the move: a homing loop that polls `SetHomePosition()` after a position move must stop it first, or it waits forever.
After a position move arrives, the chip waits TZEROWAIT before the next move (pages 41, 83), and the standstill check
answers `Busy` during that pause too.

`RunVelocity()` called while a position stop pauses for TZEROWAIT or turns back after an overshoot: the ramp may
start its run-back before the mode write arrives, by at most VSTART*t + a*t^2/2 + (VSTART + a*t)^2/(2*AMAX)
microsteps, t the time to the accepted mode write (one SPI datagram; one 8-byte UART frame, 80 bit times: 8.3 ms at
9600 baud, 0.7 ms at 115200) and a = max(A1, AMAX) when V1 > 0 and VSTART < V1, else AMAX (V1 = 0 disables A1,
page 40). A TZEROWAIT longer than t avoids it (page 83 c). From position mode the motor also keeps, or speeds
towards, the old VMAX for that one write before the new VMAX applies. The bound counts from the call: a run-back that
already moves keeps its speed for that write too, and 8.3 ms is the least t at 9600 baud (the port and the line add
to it).

A new `MoveTo()` (or `MoveBy()`) while a `Stop(HoldTarget)` pauses for TZEROWAIT retargets at once: the RAMPMODE
write ends the pause (A51), and the ramp heads for the new target without the rest of the wait. `SetMaxVelocity()`
during the pause changes only the speed of the run-back that follows. Wait for the pause to end (the standstill
check is `Busy` until then) when the extra wait matters to the mechanics.

`SetMaxVelocity(0)` stops a position move (VSTART 0 first, page 84) and stays in force: `MoveTo()` and `MoveBy()`
return `Refused` until `SetMaxVelocity()` or `SetRamp()` gives a speed again.

## XACTUAL units

In the model and on the chip, XACTUAL counts microsteps at 256 per full step; MSCNT is `XACTUAL & 1023` in the sequencer.
TSTEP is `fCLK / |v_hz|` with 256 microsteps (section 6.2, page 38).

## Limits

`Config::max_velocity_reg` is an optional ceiling for VMAX beyond the register limit of `2^23 - 512`; a ramp above it is
refused. A VSTOP below 10 gives a slow termination (section 12.5, page 86): this is documented advice, not a guard.
