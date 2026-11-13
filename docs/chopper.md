# StealthChop and SpreadCycle

The two chopper modes, the thresholds that switch between them and the sequencing of StealthChop's automatic tuning.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Analog Devices (Trinamic) TMC5160A-DS 1.18:

    - Velocity dependent control: 6.2 Velocity Dependent Driver Feature Control Register Set, p. 38
    - Motor driver registers: 6.5 Motor Driver Registers, p. 47
    - Chopper configuration: 6.5 Motor Driver Registers, p. 51
    - StealthChop2 and automatic tuning: 7 StealthChop, p. 57
    - StealthChop and SpreadCycle combined: 7.5 Combine StealthChop and SpreadCycle, p. 64
    - Freewheeling and passive braking: 7.7 Freewheeling and Passive Braking, p. 66
    - SpreadCycle and classic chopper: 8 SpreadCycle and Classic Chopper, p. 68
    - Velocity based mode control: 10 Velocity Based Mode Control, p. 76

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## SpreadCycle and the classic chopper

`SetChopper()` writes CHOPCONF from `Chopper`: the chopper mode, TOFF, TBL, hysteresis, the microstep resolution and the
high-velocity options. TOFF 0 switches the driver off; the driver only writes it through `Disable()`. `toff` 1 needs
`tbl` 2 or more, and the effective hysteresis sum may not exceed 16 (section 8.1, page 69). The reset value of CHOPCONF
is `0x10410150` and the driver always writes the whole word from its shadow.

## StealthChop2

`SetStealthChop()` writes PWMCONF and the enable bit. StealthChop may only be switched on at standstill with IHOLD equal
to IRUN: the driver checks standstill as `Enable()` does (`Busy` otherwise), writes IHOLD = IRUN, enables the mode,
waits the automatic tuning time (AT#1, at most 130 ms plus a margin) with bounded `DelayUs()` calls, then restores
the hold current. The
second tuning step (AT#2) is a motion at 60 to 300 RPM that the application performs; `ReadStealthChopState()` returns
PWM_SCALE and PWM_AUTO to judge it.

```cpp
--8<-- "tests/docs/api_tasks.cpp:stealth"
```

With autoscale on, IRUN must be at least 8, TPOWERDOWN at least 2, and when the coil resistance is declared the run
current must be above the lower limit `tBLANK * fPWM * VM / Rcoil` (section 7.3, page 62), otherwise `Refused`.
Changing GLOBALSCALER or the supply invalidates the tuning (page 59). A hard stop of the switch or stall stop
(`Switches::soft_stop` false) with StealthChop on can cause a large overcurrent for tens of milliseconds, so it needs
`Ack::HardStopWithStealthChop`; the `Stop()` kinds are ramps and need none.

## Thresholds

| Register | Field | Meaning |
| --- | --- | --- |
| TPWMTHRS | `Thresholds::pwm` | StealthChop below this velocity (TSTEP above it), SpreadCycle above |
| TCOOLTHRS | `Thresholds::cool` | CoolStep and the stall output above this velocity |
| THIGH | `Thresholds::high` | The high-velocity chopper mode above this velocity |

TSTEP is a time, so a larger threshold value means a lower velocity. The ordering rule is TPWMTHRS at least TCOOLTHRS at
least THIGH where non-zero (section 10, page 76).

## Freewheeling and passive braking

`StealthChop::freewheel` chooses the standstill mode when IHOLD is 0 (section 7.7, page 66); the driver allows it only
with IHOLD 0 and StealthChop on.

## Not provided

PWM_OFS and PWM_GRAD estimate helpers (section 7.4, page 63) are not implemented; the caller sets the values in
`StealthChop::ofs` and `grad`, or relies on the automatic tuning.
