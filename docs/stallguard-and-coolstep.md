# StallGuard2 and CoolStep

Load measurement, stall detection and the load-adaptive current.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Analog Devices (Trinamic) TMC5160A-DS 1.18:

    - Reference switches and SW_MODE: 6.3 Ramp Generator Registers, p. 43
    - CoolStep and StallGuard2 registers: 6.5 Motor Driver Registers, p. 53
    - StallGuard2: 13 StallGuard2 Load Measurement, p. 87
    - Homing with StallGuard: 13.4 Homing with StallGuard, p. 90
    - CoolStep: 14 CoolStep Operation, p. 91

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## StallGuard2

`SetStallGuard()` writes the threshold SGT (-64 to 63) and the filter. `ReadStallGuard()` returns the 10-bit SG_RESULT.
StallGuard2 is valid only above the velocity set in TCOOLTHRS, so `Thresholds::cool` must be set first; the driver
enforces that for `SetStopOnStall()` too. The value depends on the motor, the supply and the speed: tune SGT on the
machine (section 13.1, page 88).

```cpp
--8<-- "tests/docs/api_tasks.cpp:stall"
```

## Stop on stall

With `SetStopOnStall(true)` the ramp generator stops when the chip reports a stall; RAMP_STAT.event_stop_sg latches the
event. The motor does not restart by itself: `ReleaseStall()` holds, verifies standstill, clears the event, and only
then may a new move start (section 13.4, page 90). A soft stop together with stop on stall is refused.

Switching the stop off (`SetStopOnStall(false)`, or `SetSwitches()` without `stop_left` or `stop_right`) would make
the chip continue towards the old target (pages 43, 44, 90). When the matching event is latched, the driver therefore
reads RAMP_STAT first and ends the ramp at the present position (position mode, XTARGET = XACTUAL) before it writes
SW_MODE. The latched event itself stays until `ReleaseStall()`. A start-up does the same for a ramp or a stall stop
left by the last session (`BeginReport::stall_latched` tells).

## CoolStep

`SetCoolStep()` writes SEMIN, SEMAX, SEUP, SEDN and the minimum current. SEMIN 0 turns CoolStep off. CoolStep lowers the
current between IRUN and half (or a quarter) of IRUN when the load allows; it needs StallGuard2 to be valid, so TCOOLTHRS
applies (section 14.2, page 91).

## Limits of the measurement

StallGuard2 is not reliable at very low speed or with StealthChop; DcStep and CoolStep affect it. The driver exposes the
raw values and does not interpret them (A38).
