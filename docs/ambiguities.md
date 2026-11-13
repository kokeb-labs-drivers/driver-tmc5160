# Ambiguities

Where the datasheet is unclear or contradicts itself, the reading the driver takes, and how the tests cover the other one.

Forty-nine questions came up while reading the datasheet (revision 1.18). Each has a decision, and the decision is tested
against the model. A **knob** is a setting of the chip model in `tests/mock_tmc5160.hpp` (`Knobs`) or of the bus
(`Faults`) that makes the model behave the other way, so a test can run the driver against each reading. Where a row says
"guard", the decision is the driver's alone (a limit it enforces) and the chip has nothing to vary; the row names the
test. Nothing here was checked on a chip: confirming each reading on hardware is open work.

| Id | Question | Decision | Knob or test |
| --- | --- | --- | --- |
| A1 | SPI clock polarity and phase | Mode 3 (page 25 says "usually referred to as SPI MODE 3") | `Faults::spi_mode`: the model frames mode 3 only |
| A2 | Maximum SCK | 4 MHz internal clock, `fCLK / 2` less 10 percent external | `Faults::sck_hz` |
| A3 | Status byte age | One transfer old; never acted on in the same call | `Knobs::status_lag` |
| A4 | Reading a write-only register | Never read; `Refused`; the shadow answers | `Knobs::wo_read_mode` (zeros, last value, garbage) |
| A5 | Write echo | Used only by `Verify::Echo` | `Knobs::echo_writes` |
| A6 | Microstep table alignment | Bit n is the increment into entry n | `Knobs::lut_literal_reading` |
| A7 | MSCURACT field order | As documented, sign-extend 9 bits | `Knobs::mscuract_swap` |
| A8 | Reset defaults depend on OTP | Documented defaults; `OTP_READ` decoded, never written | `Knobs::otp_byte` |
| A9 | Break-before-make write hazard | Written only with the driver off | `Knobs::bbm_hazard` |
| A10 | BBMTIME maximum, 375 or 400 ns | The larger number in the guard | guard: bridge tests |
| A11 | S2G level, 6 or 12 above 52 V | 12 to 15 when the highest supply exceeds 52 V | guard: current tests |
| A12 | `multistep_filt` default | Always written explicitly | `Knobs::multistep_reset` |
| A13 | GSTAT after power-up | Cleared at start-up | `Knobs::gstat_reset_value` |
| A14 | `en_pwm_mode` or `en_spread` | `en_pwm_mode` | naming only |
| A15 | Threshold ordering | TPWMTHRS at least TCOOLTHRS at least THIGH, non-strict | guard: core tests |
| A16 | Hysteresis sum | Guard on the effective sum | guard: core tests |
| A17 | PWM_REG 0 | 1 to 15 enforced with autoscale | guard: core tests |
| A18 | Freewheel conditions | Only with IHOLD 0 | guard: core tests |
| A19 | GLOBALSCALER 1 to 31 | Never planned, rejected | guard: current tests |
| A20 | Full-scale voltage and the page 74 table | The formula with 325 mV and a 5 percent margin | `Knobs::vfs_mv` |
| A21 | Ramp limits | Register widths plus the VMAX limit | guard: motion tests |
| A22 | Trajectory exactness | The model is behavioural; only invariants are checked | none: no exact reading to vary |
| A23 | TSTEP formula | `2^24 * USC / (256 * V)` | units tests |
| A24 | RAMP_STAT clear mask | `0x10CC` | `Knobs::ramp_stat_wc` |
| A25 | DIAG semantics per SD_MODE | Per-mode fields in `Diag` | guard: core tests |
| A26 | Direct mode exit | Hold, clear, then XTARGET = XACTUAL | `Knobs::direct_mode_exit` |
| A27 | VDCMIN range | 23 bits, bits 7..0 ignored | `Knobs::vdcmin_range` |
| A28 | External clock | 4 to 18 MHz accepted | guard: core tests |
| A29 | VERSION of the A version | 0x30 accepted, other values `DeviceIdMismatch` | `Knobs::version_byte` |
| A30 | IFCNT in SPI | Never used on SPI | `Knobs::ifcnt_spi` |
| A31 | Node 254 | Allowed with `multi_node` | `Knobs::node_254` |
| A32 | UART reply timeout | SENDDELAY plus 64 bit times | `Knobs::uart_reply_delay_bits` |
| A33 | UART recovery | 12 bit times | `Knobs::uart_recovery_bits` |
| A34 | Daisy-chain order | Index 0 on MOSI | `Knobs::chain_order` |
| A35 | QFN pin count | Recorded as `QFN-WA` with a note | none: documentation |
| A36 | OTSELECT meaning | A table in the guard | guard: core tests |
| A37 | StealthChop autotune conditions | The A version; a longer autotune leaves it unfinished | `Knobs::at_a_version` |
| A38 | StallGuard values | Raw values over 10 bits; the model scripts SG_RESULT | `SetSgResult()`: test `A38` |
| A39 | Reset indication on DIAG0 | The application's choice | none: documented |
| A40 | OTP programming | Out of scope; never written | model write counter `forbidden_writes`: test `A40 and A41` |
| A41 | FACTORY_CONF | Never written | model write counters |
| A42 | Strap verification | Checked at start-up | `Knobs::sd_mode` |
| A43 | Decimal encoder constant, negatives | The page 107 rule | `Knobs::enc_decimal_sign` |
| A44 | DcStep rules | DC_TIME above tBLANK in clocks for the chosen TBL (16, 24, 36, 54), toff 3 to 15, full-step and constant off time | guard: test `A44` flips TBL |
| A45 | Revision and TMC5130 | TMC5160 and TMC5160A only | none: scope |
| A47 | Sense resistor range | 1 to 10000 mOhm accepted if the plan fits the rating | guard: test `A47` |
| A48 | Soft and hard stop with StealthChop | An acknowledgement is needed for the switch stop (`Stop()` kinds are ramps) | guard: switch configuration tests |
| A49 | X_COMPARE | A one-update pulse on the DIAG pin | `Knobs::xcompare_pulse`: test `A49` flips it |
| A50 | Supply range | 10 to 55 V (the cover says 8 to 60 V) | guard: `supply_range` test |
| A51 | TZEROWAIT and a new mode | The model ends a running pause on any RAMPMODE write; page 83 c only says the CPU may end the motion during the pause with option a or b | model reading: tests T252, T257, T268 |
| A52 | TZEROWAIT outside position mode | The model pauses after a position-mode ramp ends in standstill, at its target or short of it (page 41: the wait after ramping down to zero velocity before the next movement or direction inversion); velocity mode has no pause | model reading: T262 sweeps TZEROWAIT 0, T276 the pause after an arrival |
| A53 | TZEROWAIT after a hard switch stop | The model's hard switch stop has no pause, though page 43 says a switch stop uses TZEROWAIT before a restart | model reading: no test depends on it |
| A54 | Lowering VMAX in position mode | The model follows a lower VMAX (VMAX 0 included) with A1 and AMAX; DMAX, D1 and VSTOP only when it slows for the target or a reversal (page 84, joystick control) | model reading: T276, T283 |
| A55 | VSTOP in velocity mode | The model ignores VSTOP in velocity mode and runs down to 0 at AMAX (pages 83, 84: VSTOP belongs to the stop at the target) | model reading: T274, T283 |
| A56 | A velocity-mode start from rest | The model starts at VSTART, as in position mode; the datasheet does not say | model reading: T272 |

Readings with no model knob: A22 (the model is behavioural, so only invariants are checked), A35, A39 (the application's
choice), A41 (write counters), A45 (scope) and A51 to A56 (the model's fixed ramp readings). The model does not set
RAMP_STAT.second_move (bit 12) on a run-back; the driver never reads that bit. The rows that name a guard are run
against both sides of the limit in the named test. Ids are not renumbered: A46 was never used and is dropped.
