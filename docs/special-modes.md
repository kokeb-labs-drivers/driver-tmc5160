# DcStep, encoder and direct mode

The features that go beyond the ramp generator: DcStep, the ABN encoder, direct mode and STEP/DIR.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Analog Devices (Trinamic) TMC5160A-DS 1.18:

    - Encoder registers: 6.4 Encoder Registers, p. 45
    - STEP/DIR interface and MicroPlyer: 15 STEP/DIR Interface, p. 94
    - DcStep: 17 DcStep, p. 99
    - ABN incremental encoder interface: 20 ABN Incremental Encoder Interface, p. 107
    - DC motor and direct mode: 21 DC Motor or Solenoid, p. 110

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## DcStep

`SetDcStep()` writes VDCMIN and DCCTRL. DcStep needs the default microstep table, `high_fullstep` and `high_const_off`
set, TOFF 3 or more and DC_TIME above the blank time, otherwise `InvalidArgument`. VDCMIN is 0 (off) or 256 and up, with
bits 7..0 ignored by the chip (A27). `ReadLostSteps()` returns LOST_STEPS (section 17, page 99).

## Encoder

`SetEncoder()` writes ENCMODE, ENC_CONST and ENC_DEVIATION when `Encoder::declared`. The constant is
`units::EncoderScaleFor(steps_per_rev, counts_per_rev, ...)`: binary or decimal, with the sign rule of page 107 for
negative decimal constants (A43).

`ReadEncoderPosition()`, `SetEncoderPosition()`, `ReadEncoderLatch()`, `ReadEncoderStatus()` and
`ClearEncoderEvents()` cover the registers; the deviation warning latches in ENC_STATUS. In STEP/DIR mode the encoder
pins change function and the calls return `Unsupported`.

## Direct mode

Direct mode takes the coil currents (or PWM) from XTARGET (section 21, page 110). It needs `Ack::DirectMode`;
`EnterDirectMode()` is refused while the motor moves, and `SetDirectCurrents()` limits the values to the range of the
chopper mode. `ExitDirectMode()` holds, clears the mode and writes XTARGET = XACTUAL so the motor does not jump (A26).

## STEP/DIR

With SD_MODE high the chip takes STEP and DIR from pins and MicroPlyer interpolates to 256 microsteps. The serial
interface still configures the chip; the ramp, the encoder and direct mode are unavailable. DIAG outputs change their
function ([DIAG and status](diag-and-status.md)).

## Microstep table

[Microstep table](microstep-table.md) explains `SetMicrostepTable()`.
