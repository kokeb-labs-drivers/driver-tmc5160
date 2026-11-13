# Coverage map

Every register, bit field, mode, pin, rating and coded range of the datasheet, and where the driver handles it.

The rows come from the design matrix this driver was built to: 593 rows, each with the datasheet section and printed
page, the driver API that handles it and a status:

| Status | Meaning |
| --- | --- |
| `OK` | The driver handles it and a host test covers it against the chip model |
| `GUARD` | A dangerous value or sequence: the driver refuses it unless the call carries an acknowledgement ([Guards](guards.md)) |
| `INFO` | Documented only: a pin or rating with no driver function, a fact the application must respect |
| `TODO` | Out of scope by the owner's decision (OTP programming); the driver never writes it |

The tests are in `tests/` and are named after the feature they check; nothing here has run on a chip. An `OK` row is
backed by an assertion that fails when the handled behaviour changes (checked by the mutation run in
[Testing](testing.md)); the DcStep (DCCTRL), encoder (ENCMODE) and bridge (DRV_CONF) bit positions are asserted
field by field in `tests/test_mutants.cpp`.

## Registers (63 rows)

| Id | Item | Datasheet | Driver API | Status |
| --- | --- | --- | --- | --- |
| REG-00 | 0x00 GCONF [RW, 18 bits, reset 0x00000000 (multistep_filt text says 1, A12)] | S6 p32-33 | `Flags` (`SetFlags`), `Diag` (`SetDiag`), `DirectMode`, `Flags::stop_input` | OK |
| REG-01 | 0x01 GSTAT [R+WC, 3 bits, reset 0x00000001 after power-up (reset flag set, A13)] | S6 p33 | `ReadGlobalStatus`, `ClearGlobalStatus(mask)` | OK |
| REG-02 | 0x02 IFCNT [R, 8 bits, reset 0] | S6 p33 | `ReadInterfaceCounter` (UART), used by `Verify::Counter` | OK |
| REG-03 | 0x03 NODECONF [W, 12 bits, reset 0] | S6 p34 | `ProgramNode`, `UartOptions` (NODEADDR, SENDDELAY); UART only, never in SPI | OK |
| REG-04 | 0x04 IOIN [R, 32 bits, reset VERSION 0x30 in bits 31-24] | S6 p34 | `ReadInputs` (pins, `version`), strap check in `EnsureInitialized` | OK |
| REG-04b | 0x04 OUTPUT [W, 1 bits, reset 1 (UART reset value)] | S6 p34 | `ProgramNode` (NAO polarity), UART only | OK |
| REG-05 | 0x05 X_COMPARE [W, 32 bits, reset 0] | S6 p34 | `SetPositionCompare` | OK |
| REG-06 | 0x06 OTP_PROG [W, 16 bits, reset 0] | S6 p34-35 | none: OTP programming is out of scope (A40) | TODO |
| REG-07 | 0x07 OTP_READ [R, 8 bits, reset OTP byte 0 (factory: FCLKTRIM, others 0)] | S6 p35,p37 | `ReadOtp` (decoded `OtpImage`) | OK |
| REG-08 | 0x08 FACTORY_CONF [RW, 5 bits, reset OTP (FCLKTRIM factory value)] | S6 p35 | `ReadClockTrim` only; never written (A41) | GUARD |
| REG-09 | 0x09 SHORT_CONF [W, 19 bits, reset S2VS=S2G=6 or 12 (OTP), SHORTFILTER 1, shortdelay 0] | S6 p35 | `Protection` (`SetProtection`), written only when declared | OK |
| REG-0A | 0x0A DRV_CONF [W, 22 bits, reset BBMTIME 0, BBMCLKS OTP 4 or 2, OTSELECT 0, DRVSTRENGTH 0, FILT_ISENSE 0] | S6 p35-36 | `Bridge` (`SetBridge`), written only when declared | OK |
| REG-0B | 0x0B GLOBALSCALER [W, 8 bits, reset 0 (full scale)] | S6 p36 | `SetCurrent` / `PlanCurrent` only; no raw setter | OK |
| REG-0C | 0x0C OFFSET_READ [R, 16 bits, reset 0] | S6 p36 | `ReadOffsets` (signed phase A and B) | OK |
| REG-10 | 0x10 IHOLD_IRUN [W, 20 bits, reset 0] | S6 p38 | `SetCurrent` / `PlanCurrent`, `SetStandstill` | OK |
| REG-11 | 0x11 TPOWERDOWN [W, 8 bits, reset 10] | S6 p38 | `SetStandstill` | OK |
| REG-12 | 0x12 TSTEP [R, 20 bits, reset 0xFFFFF at standstill] | S6 p38 | `ReadStepTime`, `ReadSnapshot` | OK |
| REG-13 | 0x13 TPWMTHRS [W, 20 bits, reset 0] | S6 p39 | `Thresholds` (`SetThresholds`) | OK |
| REG-14 | 0x14 TCOOLTHRS [W, 20 bits, reset 0] | S6 p39 | `Thresholds` | OK |
| REG-15 | 0x15 THIGH [W, 20 bits, reset 0] | S6 p39 | `Thresholds` | OK |
| REG-20 | 0x20 RAMPMODE [RW, 2 bits, reset 0] | S6 p40 | `MoveTo`, `RunVelocity`, `Hold`, `Stop` | OK |
| REG-21 | 0x21 XACTUAL [RW, 32 bits, reset 0] | S6 p40 | `ReadPosition`, `SetHomePosition` (guarded) | OK |
| REG-22 | 0x22 VACTUAL [R, 24 bits, reset 0] | S6 p40 | `ReadVelocity` | OK |
| REG-23 | 0x23 VSTART [W, 18 bits, reset 0] | S6 p40 | `RampProfile.vstart` | OK |
| REG-24 | 0x24 A1 [W, 16 bits, reset 0] | S6 p40 | `RampProfile.a1` | OK |
| REG-25 | 0x25 V1 [W, 20 bits, reset 0] | S6 p40 | `RampProfile.v1` | OK |
| REG-26 | 0x26 AMAX [W, 16 bits, reset 0] | S6 p40 | `RampProfile.amax` | OK |
| REG-27 | 0x27 VMAX [W, 23 bits, reset 0] | S6 p40 | `RampProfile.vmax`, `SetMaxVelocity`, `RunVelocity` | OK |
| REG-28 | 0x28 DMAX [W, 16 bits, reset 0] | S6 p40 | `RampProfile.dmax` | OK |
| REG-2A | 0x2A D1 [W, 16 bits, reset 0] | S6 p40 | `RampProfile.d1` (at least 1) | OK |
| REG-2B | 0x2B VSTOP [W, 18 bits, reset 1] | S6 p41 | `RampProfile.vstop` (at least 1) | OK |
| REG-2C | 0x2C TZEROWAIT [W, 16 bits, reset 0] | S6 p41 | `RampProfile::zero_wait` | OK |
| REG-2D | 0x2D XTARGET [RW, 32 bits, reset 0] | S6 p41 | `MoveTo`, `MoveBy`, `SetDirectCurrents` | OK |
| REG-33 | 0x33 VDCMIN [W, 23 bits, reset 0] | S6 p42 | `DcStep.vdcmin` | OK |
| REG-34 | 0x34 SW_MODE [RW, 12 bits, reset 0] | S6 p42-43 | `Switches` (`SetSwitches`), `SetStopOnStall` | OK |
| REG-35 | 0x35 RAMP_STAT [R+WC, 14 bits, reset 0 (event_pos_reached set at reset, A39)] | S6 p42,p44 | `ReadRampStatus`, `ClearRampEvents(mask)` | OK |
| REG-36 | 0x36 XLATCH [R, 32 bits, reset 0] | S6 p42 | `ReadLatchedPosition` | OK |
| REG-38 | 0x38 ENCMODE [RW, 11 bits, reset 0] | S6 p45-46 | `Encoder` (`SetEncoder`) | OK |
| REG-39 | 0x39 X_ENC [RW, 32 bits, reset 0] | S6 p45 | `ReadEncoderPosition`, `SetEncoderPosition` | OK |
| REG-3A | 0x3A ENC_CONST [W, 32 bits, reset 0x00010000 (1.0)] | S6 p45 | `EncoderScale` (binary or decimal, signed) | OK |
| REG-3B | 0x3B ENC_STATUS [R+WC, 2 bits, reset 0] | S6 p45 | `ReadEncoderStatus`, `ClearEncoderEvents` | OK |
| REG-3C | 0x3C ENC_LATCH [R, 32 bits, reset 0] | S6 p45 | `ReadEncoderLatch` | OK |
| REG-3D | 0x3D ENC_DEVIATION [W, 20 bits, reset 0] | S6 p45 | `Encoder.deviation` | OK |
| REG-60 | 0x60 MSLUT[0] [W, 32 bits, reset sine table word 0] | S6 p47,p106 | `MicrostepTable.words[0]` (`SetMicrostepTable`) | OK |
| REG-61 | 0x61 MSLUT[1] [W, 32 bits, reset sine table word 1] | S6 p47,p106 | `MicrostepTable.words[1]` (`SetMicrostepTable`) | OK |
| REG-62 | 0x62 MSLUT[2] [W, 32 bits, reset sine table word 2] | S6 p47,p106 | `MicrostepTable.words[2]` (`SetMicrostepTable`) | OK |
| REG-63 | 0x63 MSLUT[3] [W, 32 bits, reset sine table word 3] | S6 p47,p106 | `MicrostepTable.words[3]` (`SetMicrostepTable`) | OK |
| REG-64 | 0x64 MSLUT[4] [W, 32 bits, reset sine table word 4] | S6 p47,p106 | `MicrostepTable.words[4]` (`SetMicrostepTable`) | OK |
| REG-65 | 0x65 MSLUT[5] [W, 32 bits, reset sine table word 5] | S6 p47,p106 | `MicrostepTable.words[5]` (`SetMicrostepTable`) | OK |
| REG-66 | 0x66 MSLUT[6] [W, 32 bits, reset sine table word 6] | S6 p47,p106 | `MicrostepTable.words[6]` (`SetMicrostepTable`) | OK |
| REG-67 | 0x67 MSLUT[7] [W, 32 bits, reset sine table word 7] | S6 p47,p106 | `MicrostepTable.words[7]` (`SetMicrostepTable`) | OK |
| REG-68 | 0x68 MSLUTSEL [W, 32 bits, reset 0xFFFF8056] | S6 p47,p50,p106 | `MicrostepTable` (x1..x3, w0..w3) | OK |
| REG-69 | 0x69 MSLUTSTART [W, 24 bits, reset 0x00F70000] | S6 p47,p106 | `MicrostepTable` (start_sin, start_sin90) | OK |
| REG-6A | 0x6A MSCNT [R, 10 bits, reset 0] | S6 p47 | `ReadMicrostepCounter` | OK |
| REG-6B | 0x6B MSCURACT [R, 25 bits, reset 0] | S6 p47 | `ReadMicrostepCurrents` (cur_a, cur_b signed) | OK |
| REG-6C | 0x6C CHOPCONF [RW, 32 bits, reset 0x10410150] | S6 p48,p51-52 | `Chopper` (`SetChopper`), `Enable`/`Disable` (TOFF) | OK |
| REG-6D | 0x6D COOLCONF [W, 25 bits, reset 0] | S6 p48,p53 | `CoolStep`, `StallGuard` | OK |
| REG-6E | 0x6E DCCTRL [W, 24 bits, reset 0] | S6 p48 | `DcStep` (dc_time, dc_sg) | OK |
| REG-6F | 0x6F DRV_STATUS [R, 32 bits, reset 0] | S6 p48,p56 | `ReadDriverStatus` | OK |
| REG-70 | 0x70 PWMCONF [W, 32 bits, reset 0xC40C001E] | S6 p48,p54-55 | `StealthChop` (`SetStealthChop`) | OK |
| REG-71 | 0x71 PWM_SCALE [R, 17 bits, reset 0] | S6 p48 | `ReadStealthChopState` | OK |
| REG-72 | 0x72 PWM_AUTO [R, 16 bits, reset 0] | S6 p48-49 | `ReadStealthChopState` | OK |
| REG-73 | 0x73 LOST_STEPS [R, 20 bits, reset 0] | S6 p49 | `ReadLostSteps` (STEP/DIR mode) | OK |

## Register bit fields (197 rows)

| Id | Item | Datasheet | Driver API | Status |
| --- | --- | --- | --- | --- |
| FLD-GCONF-01 | GCONF bit 0 recalibrate: zero-crossing recalibration on driver disable | p32-33 | `Flags.recalibrate` | OK |
| FLD-GCONF-02 | GCONF bit 1 faststandstill: standstill detection 2^18 instead of 2^20 clocks | p32-33 | `Flags.faststandstill` | OK |
| FLD-GCONF-03 | GCONF bit 2 en_pwm_mode: StealthChop enable (A14) | p32-33 | `StealthChop.enable` | OK |
| FLD-GCONF-04 | GCONF bit 3 multistep_filt: step input filtering for StealthChop (A12) | p32-33 | `Flags.multistep_filt` | OK |
| FLD-GCONF-05 | GCONF bit 4 shaft: invert motor direction | p32-33 | `Flags.shaft` | OK |
| FLD-GCONF-06 | GCONF bit 5 diag0_error: DIAG0 on driver error (SD_MODE=1 only) | p32-33 | `Diag.diag0.error` | OK |
| FLD-GCONF-07 | GCONF bit 6 diag0_otpw: DIAG0 on overtemperature pre-warning (SD_MODE=1 only) | p32-33 | `Diag.diag0.otpw` | OK |
| FLD-GCONF-08 | GCONF bit 7 diag0_stall / diag0_step: DIAG0 on stall (SD_MODE=1) or STEP output (SD_MODE=0) (A25) | p32-33 | `Diag::diag0_stall`, `diag0_step_output` | OK |
| FLD-GCONF-09 | GCONF bit 8 diag1_stall / diag1_dir: DIAG1 on stall (SD_MODE=1) or DIR output (SD_MODE=0) (A25) | p32-33 | `Diag::diag1_stall`, `diag1_dir_output` | OK |
| FLD-GCONF-10 | GCONF bit 9 diag1_index: DIAG1 on microstep index 0 (SD_MODE=1 only) | p32-33 | `Diag.diag1.index` | OK |
| FLD-GCONF-11 | GCONF bit 10 diag1_onstate: DIAG1 on chopper on-state (SD_MODE=1 only) | p32-33 | `Diag.diag1.on_state` | OK |
| FLD-GCONF-12 | GCONF bit 11 diag1_steps_skipped: DIAG1 toggle on DcStep skipped steps; exclusive (A25) | p32-33 | `Diag.diag1.steps_skipped` | GUARD |
| FLD-GCONF-13 | GCONF bit 12 diag0_int_pushpull: DIAG0 push-pull instead of open collector | p32-33 | `Diag.diag0_push_pull` | OK |
| FLD-GCONF-14 | GCONF bit 13 diag1_poscomp_pushpull: DIAG1 push-pull instead of open collector | p32-33 | `Diag.diag1_push_pull` | OK |
| FLD-GCONF-15 | GCONF bit 14 small_hysteresis: step-frequency hysteresis 1/32 instead of 1/16 | p32-33 | `Thresholds.small_hysteresis` | OK |
| FLD-GCONF-16 | GCONF bit 15 stop_enable: ENCA_DCIN stops the sequencer (emergency stop) | p32-33 | `Flags.stop_input` | OK |
| FLD-GCONF-17 | GCONF bit 16 direct_mode: coil currents from XTARGET | p32-33 | `DirectMode` (`EnterDirectMode`), acknowledged | GUARD |
| FLD-GCONF-18 | GCONF bit 17 test_mode: analog test output; never for users | p32-33 | none: never set | GUARD |
| FLD-GSTAT-01 | GSTAT bit 0 reset: IC has been reset; registers are at reset values | p33 | `GlobalStatus.reset` | OK |
| FLD-GSTAT-02 | GSTAT bit 1 drv_err: driver shut down by overtemperature or short | p33 | `GlobalStatus.drv_err` | OK |
| FLD-GSTAT-03 | GSTAT bit 2 uv_cp: charge pump undervoltage, latched | p33 | `GlobalStatus.uv_cp` | OK |
| FLD-IFCNT-01 | IFCNT bit 7..0 IFCNT: UART write counter, wraps 255 to 0 | p33 | `ReadInterfaceCounter` | OK |
| FLD-NODECONF-01 | NODECONF bit 7..0 NODEADDR: UART node address 0-253 (254 in the chain procedure, A31) | p34 | `UartOptions.node`, `ProgramNode` | OK |
| FLD-NODECONF-02 | NODECONF bit 11..8 SENDDELAY: reply delay in 8-bit-time steps; at least 2 with several nodes | p34 | `UartOptions.send_delay` | OK |
| FLD-IOIN-01 | IOIN bit 0 REFL_STEP: level of REFL_STEP | p34 | `InputPins.refl_step` | OK |
| FLD-IOIN-02 | IOIN bit 1 REFR_DIR: level of REFR_DIR | p34 | `InputPins.refr_dir` | OK |
| FLD-IOIN-03 | IOIN bit 2 ENCB_DCEN_CFG4: level of ENCB_DCEN_CFG4 | p34 | `InputPins.encb_dcen` | OK |
| FLD-IOIN-04 | IOIN bit 3 ENCA_DCIN_CFG5: level of ENCA_DCIN_CFG5 | p34 | `InputPins.enca_dcin` | OK |
| FLD-IOIN-05 | IOIN bit 4 DRV_ENN: level of DRV_ENN | p34 | `InputPins.drv_enn` | OK |
| FLD-IOIN-06 | IOIN bit 5 ENC_N_DCO_CFG6: level of ENCN_DCO_CFG6 | p34 | `InputPins.encn_dco` | OK |
| FLD-IOIN-07 | IOIN bit 6 SD_MODE: SD_MODE strap (1 = external step and dir) | p34 | `InputPins.sd_mode`, strap check in `EnsureInitialized` (A42) | OK |
| FLD-IOIN-08 | IOIN bit 7 SWCOMP_IN: single-wire comparator input | p34 | `InputPins.swcomp_in` | OK |
| FLD-IOIN-09 | IOIN bit 31..24 VERSION: 0x30 first version | p34 | `InputPins.version`, check in `EnsureInitialized` (A29) | OK |
| FLD-OUTPUT-01 | OUTPUT bit 0 OUTPUT: SDO_CFG0/NAO output polarity in UART mode | p34 | `ProgramNode` (nao_level) | OK |
| FLD-OTP_PROG-01 | OTP_PROG bit 2..0 OTPBIT: bit to program | p34-35 | none (A40) | TODO |
| FLD-OTP_PROG-02 | OTP_PROG bit 5..4 OTPBYTE: byte location, set to 00 | p34-35 | none (A40) | TODO |
| FLD-OTP_PROG-03 | OTP_PROG bit 15..8 OTPMAGIC: 0xBD enables programming | p34-35 | none (A40) | TODO |
| FLD-OTP_READ-01 | OTP_READ bit 4..0 otp0.0-4 FCLKTRIM: factory clock trim | p35,p37 | `OtpImage.clock_trim` | OK |
| FLD-OTP_READ-02 | OTP_READ bit 5 otp0.5 otp_S2_LEVEL: short levels 6 or 12 reset default | p35,p37 | `OtpImage.s2_level_12` | OK |
| FLD-OTP_READ-03 | OTP_READ bit 6 otp0.6 otp_BBM: BBMCLKS reset default 4 or 2 | p35,p37 | `OtpImage.bbm_clks_2` | OK |
| FLD-OTP_READ-04 | OTP_READ bit 7 otp0.7 otp_TBL: TBL reset default %10 or %01 | p35,p37 | `OtpImage.tbl_01` | OK |
| FLD-FACTORY_CONF-01 | FACTORY_CONF bit 4..0 FCLKTRIM: clock trim 0-31, factory preset to 12 MHz | p35 | `ReadClockTrim` only (A41) | GUARD |
| FLD-SHORT_CONF-01 | SHORT_CONF bit 3..0 S2VS_LEVEL: low-side short/overcurrent level, 4-15 | p35,p78 | `Protection.s2vs_level` | OK |
| FLD-SHORT_CONF-02 | SHORT_CONF bit 11..8 S2G_LEVEL: high-side short level, 2-15 (12-15 above 52 V, A11) | p35,p78 | `Protection.s2g_level` | OK |
| FLD-SHORT_CONF-03 | SHORT_CONF bit 17..16 SHORTFILTER: spike filter 100 ns, 1, 2, 3 us | p35,p78 | `Protection.filter` | OK |
| FLD-SHORT_CONF-04 | SHORT_CONF bit 18 shortdelay: short detection delay 750 or 1500 ns | p35,p78 | `Protection.long_delay` | OK |
| FLD-DRV_CONF-01 | DRV_CONF bit 4..0 BBMTIME: break-before-make 0-24 (A10) | p35-36,p18 | `Bridge.bbm_time` | OK |
| FLD-DRV_CONF-02 | DRV_CONF bit 11..8 BBMCLKS: digital BBM in clocks 0-15 (A9) | p35-36,p18 | `Bridge.bbm_clks` | OK |
| FLD-DRV_CONF-03 | DRV_CONF bit 17..16 OTSELECT: overtemperature shutdown 150, 143, 136, 120 C (A36) | p35-36,p18 | `Bridge.ot_level` | GUARD |
| FLD-DRV_CONF-04 | DRV_CONF bit 19..18 DRVSTRENGTH: gate driver current 0-3 | p35-36,p18 | `Bridge.drive_strength` | OK |
| FLD-DRV_CONF-05 | DRV_CONF bit 21..20 FILT_ISENSE: sense amplifier filter 100-400 ns | p35-36,p18 | `Bridge.sense_filter` | OK |
| FLD-GLOBALSCALER-01 | GLOBALSCALER bit 7..0 GLOBALSCALER: 0 or 32-255 (256 = 0); 1-31 not allowed (A19) | p36,p74 | `CurrentPlan.global_scaler` (32-256) | GUARD |
| FLD-OFFSET_READ-01 | OFFSET_READ bit 15..8 OFFSET_A: offset calibration phase A, signed | p36 | `Offsets.phase_a` | OK |
| FLD-OFFSET_READ-02 | OFFSET_READ bit 7..0 OFFSET_B: offset calibration phase B, signed | p36 | `Offsets.phase_b` | OK |
| FLD-IHOLD_IRUN-01 | IHOLD_IRUN bit 4..0 IHOLD: standstill current scale 1/32-32/32 | p38,p75 | `CurrentPlan.ihold` | GUARD |
| FLD-IHOLD_IRUN-02 | IHOLD_IRUN bit 12..8 IRUN: run current scale | p38,p75 | `CurrentPlan.irun` | GUARD |
| FLD-IHOLD_IRUN-03 | IHOLD_IRUN bit 19..16 IHOLDDELAY: power-down ramp, 2^18 clocks per step | p38,p75 | `Standstill.hold_delay` | OK |
| FLD-TPOWERDOWN-01 | TPOWERDOWN bit 7..0 TPOWERDOWN: delay to power-down, 2^18 clocks, at least 2 for autotune (A37) | p38 | `Standstill.power_down_delay` | OK |
| FLD-TSTEP-01 | TSTEP bit 19..0 TSTEP: time between 1/256 microsteps in clocks; 0xFFFFF at standstill | p38 | `ReadStepTime` | OK |
| FLD-TPWMTHRS-01 | TPWMTHRS bit 19..0 TPWMTHRS: StealthChop upper velocity (A15) | p39 | `Thresholds.pwm` | OK |
| FLD-TCOOLTHRS-01 | TCOOLTHRS bit 19..0 TCOOLTHRS: CoolStep and stall lower velocity (A15) | p39 | `Thresholds.cool` | OK |
| FLD-THIGH-01 | THIGH bit 19..0 THIGH: high-velocity threshold (A15) | p39 | `Thresholds.high` | OK |
| FLD-RAMPMODE-01 | RAMPMODE bit 1..0 RAMPMODE: 0 position, 1 velocity +, 2 velocity -, 3 hold | p40 | `RampMode` (internal to `MoveTo`, `RunVelocity`, `Hold`) | OK |
| FLD-XACTUAL-01 | XACTUAL bit 31..0 XACTUAL: actual position, signed | p40 | `ReadPosition`, `SetHomePosition` | OK |
| FLD-VACTUAL-01 | VACTUAL bit 23..0 VACTUAL: actual velocity, signed 24-bit | p40 | `ReadVelocity` | OK |
| FLD-VSTART-01 | VSTART bit 17..0 VSTART: start velocity | p40 | `RampProfile.vstart` | OK |
| FLD-A1-01 | A1 bit 15..0 A1: first acceleration | p40 | `RampProfile.a1` | OK |
| FLD-V1-01 | V1 bit 19..0 V1: phase threshold velocity, 0 disables A1/D1 | p40 | `RampProfile.v1` | OK |
| FLD-AMAX-01 | AMAX bit 15..0 AMAX: second acceleration | p40 | `RampProfile.amax` | OK |
| FLD-VMAX-01 | VMAX bit 22..0 VMAX: target velocity, at most 2^23 - 512 | p40 | `RampProfile.vmax` | GUARD |
| FLD-DMAX-01 | DMAX bit 15..0 DMAX: deceleration above V1 | p40 | `RampProfile.dmax` | OK |
| FLD-D1-01 | D1 bit 15..0 D1: deceleration below V1, at least 1 | p40 | `RampProfile.d1` | GUARD |
| FLD-VSTOP-01 | VSTOP bit 17..0 VSTOP: stop velocity, at least 1 | p41 | `RampProfile.vstop` | GUARD |
| FLD-TZEROWAIT-01 | TZEROWAIT bit 15..0 TZEROWAIT: wait after zero velocity, units of 512 clocks | p41 | `RampProfile.zero_wait` | OK |
| FLD-XTARGET-01 | XTARGET bit 31..0 XTARGET: target position, signed; coil currents in direct mode | p41 | `MoveTo`, `MoveBy`, `SetDirectCurrents` | OK |
| FLD-VDCMIN-01 | VDCMIN bit 22..8 VDCMIN: DcStep minimum velocity, bits 22-8 used (A27) | p42 | `DcStep.vdcmin` | OK |
| FLD-SW_MODE-01 | SW_MODE bit 0 stop_l_enable: stop at active left switch | p43 | `Switches.stop_left` | OK |
| FLD-SW_MODE-02 | SW_MODE bit 1 stop_r_enable: stop at active right switch | p43 | `Switches.stop_right` | OK |
| FLD-SW_MODE-03 | SW_MODE bit 2 pol_stop_l: left switch active level | p43 | `Switches.left_active_low` | OK |
| FLD-SW_MODE-04 | SW_MODE bit 3 pol_stop_r: right switch active level | p43 | `Switches.right_active_low` | OK |
| FLD-SW_MODE-05 | SW_MODE bit 4 swap_lr: swap REFL and REFR | p43 | `Switches.swap` | OK |
| FLD-SW_MODE-06 | SW_MODE bit 5 latch_l_active: latch position on active edge of REFL | p43 | `Switches.latch_left_active` | OK |
| FLD-SW_MODE-07 | SW_MODE bit 6 latch_l_inactive: latch position on inactive edge of REFL | p43 | `Switches.latch_left_inactive` | OK |
| FLD-SW_MODE-08 | SW_MODE bit 7 latch_r_active: latch position on active edge of REFR | p43 | `Switches.latch_right_active` | OK |
| FLD-SW_MODE-09 | SW_MODE bit 8 latch_r_inactive: latch position on inactive edge of REFR | p43 | `Switches.latch_right_inactive` | OK |
| FLD-SW_MODE-10 | SW_MODE bit 9 en_latch_encoder: also latch X_ENC on a switch event | p43 | `Switches.latch_encoder` | OK |
| FLD-SW_MODE-11 | SW_MODE bit 10 sg_stop: stop on stall (hard stop only, A38) | p43 | `SetStopOnStall`, `Switches.stop_on_stall` | GUARD |
| FLD-SW_MODE-12 | SW_MODE bit 11 en_softstop: soft stop with the deceleration ramp, not with sg_stop (A48) | p43 | `Switches.soft_stop` | GUARD |
| FLD-RAMP_STAT-01 | RAMP_STAT bit 0 status_stop_l: left switch active | p44 | `RampStatus.stop_left_active` | OK |
| FLD-RAMP_STAT-02 | RAMP_STAT bit 1 status_stop_r: right switch active | p44 | `RampStatus.stop_right_active` | OK |
| FLD-RAMP_STAT-03 | RAMP_STAT bit 2 status_latch_l: left latch ready, W1C | p44 | `RampStatus.latch_left` | OK |
| FLD-RAMP_STAT-04 | RAMP_STAT bit 3 status_latch_r: right latch ready, W1C | p44 | `RampStatus.latch_right` | OK |
| FLD-RAMP_STAT-05 | RAMP_STAT bit 4 event_stop_l: left stop event (cleared by hold or reversal) | p44 | `RampStatus.event_stop_left` | OK |
| FLD-RAMP_STAT-06 | RAMP_STAT bit 5 event_stop_r: right stop event | p44 | `RampStatus.event_stop_right` | OK |
| FLD-RAMP_STAT-07 | RAMP_STAT bit 6 event_stop_sg: stall stop event, W1C | p44 | `RampStatus.event_stop_stall`, `ReleaseStall` | OK |
| FLD-RAMP_STAT-08 | RAMP_STAT bit 7 event_pos_reached: target reached event, W1C, ORed to interrupt | p44 | `RampStatus.event_pos_reached` | OK |
| FLD-RAMP_STAT-09 | RAMP_STAT bit 8 velocity_reached: VACTUAL equals VMAX | p44 | `RampStatus.velocity_reached` | OK |
| FLD-RAMP_STAT-10 | RAMP_STAT bit 9 position_reached: XACTUAL equals XTARGET | p44 | `RampStatus.position_reached` | OK |
| FLD-RAMP_STAT-11 | RAMP_STAT bit 10 vzero: VACTUAL is zero | p44 | `RampStatus.velocity_zero` | OK |
| FLD-RAMP_STAT-12 | RAMP_STAT bit 11 t_zerowait_active: TZEROWAIT running | p44 | `RampStatus.zero_wait_active` | OK |
| FLD-RAMP_STAT-13 | RAMP_STAT bit 12 second_move: ramp reversed direction, W1C | p44 | `RampStatus.second_move` | OK |
| FLD-RAMP_STAT-14 | RAMP_STAT bit 13 status_sg: stall input active from CoolStep or DcStep | p44 | `RampStatus.stall` | OK |
| FLD-XLATCH-01 | XLATCH bit 31..0 XLATCH: position latched at a switch event | p42 | `ReadLatchedPosition` | OK |
| FLD-ENCMODE-01 | ENCMODE bit 0 pol_A: required A polarity for an N event | p46 | `Encoder.n_event.pol_a` | OK |
| FLD-ENCMODE-02 | ENCMODE bit 1 pol_B: required B polarity for an N event | p46 | `Encoder.n_event.pol_b` | OK |
| FLD-ENCMODE-03 | ENCMODE bit 2 pol_N: active polarity of N | p46 | `Encoder.n_event.pol_n` | OK |
| FLD-ENCMODE-04 | ENCMODE bit 3 ignore_AB: ignore A/B polarity for N | p46 | `Encoder.n_event.ignore_ab` | OK |
| FLD-ENCMODE-05 | ENCMODE bit 4 clr_cont: latch/clear on every N event | p46 | `Encoder.n_event.repeat` | OK |
| FLD-ENCMODE-06 | ENCMODE bit 5 clr_once: latch/clear on the next N event only | p46 | `Encoder.n_event.once` | OK |
| FLD-ENCMODE-07 | ENCMODE bit 6 pos_edge: N sensitivity: active-going edge | p46 | `Encoder.n_event.edge` | OK |
| FLD-ENCMODE-08 | ENCMODE bit 7 neg_edge: N sensitivity: inactive-going edge | p46 | `Encoder.n_event.edge` | OK |
| FLD-ENCMODE-09 | ENCMODE bit 8 clr_enc_x: clear X_ENC at the N event | p46 | `Encoder.n_event.clear_counter` | OK |
| FLD-ENCMODE-10 | ENCMODE bit 9 latch_x_act: also latch XACTUAL at the N event | p46 | `Encoder.n_event.latch_xactual` | OK |
| FLD-ENCMODE-11 | ENCMODE bit 10 enc_sel_decimal: decimal instead of binary ENC_CONST | p46 | `EncoderScale.decimal` | OK |
| FLD-X_ENC-01 | X_ENC bit 31..0 X_ENC: encoder position, signed | p45 | `ReadEncoderPosition`, `SetEncoderPosition` | OK |
| FLD-ENC_CONST-01 | ENC_CONST bit 31..16 ENC_CONST int: integer part, signed | p45,p107 | `EncoderScale.integer` | OK |
| FLD-ENC_CONST-02 | ENC_CONST bit 15..0 ENC_CONST frac: fraction, 1/65536 or 1/10000 (A43) | p45,p107 | `EncoderScale.fraction` | OK |
| FLD-ENC_STATUS-01 | ENC_STATUS bit 0 n_event: N event detected, W1C | p45 | `EncoderStatus.n_event` | OK |
| FLD-ENC_STATUS-02 | ENC_STATUS bit 1 deviation_warn: encoder deviates from XACTUAL, W1C while it persists | p45 | `EncoderStatus.deviation_warn` | OK |
| FLD-ENC_LATCH-01 | ENC_LATCH bit 31..0 ENC_LATCH: X_ENC latched on an N event | p45 | `ReadEncoderLatch` | OK |
| FLD-ENC_DEVIATION-01 | ENC_DEVIATION bit 19..0 ENC_DEVIATION: maximum deviation in steps, 0 = off | p45 | `Encoder.deviation` | OK |
| FLD-MSLUT0-01 | MSLUT[0] bit 31..0 ofs31..ofs00: increments for entries 0-31 (A6) | p47 | `MicrostepTable.words[0]` | OK |
| FLD-MSLUT1-01 | MSLUT[1] bit 31..0 ofs63..ofs32: increments for entries 32-63 (A6) | p47 | `MicrostepTable.words[1]` | OK |
| FLD-MSLUT2-01 | MSLUT[2] bit 31..0 ofs95..ofs64: increments for entries 64-95 (A6) | p47 | `MicrostepTable.words[2]` | OK |
| FLD-MSLUT3-01 | MSLUT[3] bit 31..0 ofs127..ofs96: increments for entries 96-127 (A6) | p47 | `MicrostepTable.words[3]` | OK |
| FLD-MSLUT4-01 | MSLUT[4] bit 31..0 ofs159..ofs128: increments for entries 128-159 (A6) | p47 | `MicrostepTable.words[4]` | OK |
| FLD-MSLUT5-01 | MSLUT[5] bit 31..0 ofs191..ofs160: increments for entries 160-191 (A6) | p47 | `MicrostepTable.words[5]` | OK |
| FLD-MSLUT6-01 | MSLUT[6] bit 31..0 ofs223..ofs192: increments for entries 192-223 (A6) | p47 | `MicrostepTable.words[6]` | OK |
| FLD-MSLUT7-01 | MSLUT[7] bit 31..0 ofs255..ofs224: increments for entries 224-255 (A6) | p47 | `MicrostepTable.words[7]` | OK |
| FLD-MSLUTSEL-01 | MSLUTSEL bit 1..0 W0: width code of segment 0 | p50 | `MicrostepTable.w[0]` | OK |
| FLD-MSLUTSEL-02 | MSLUTSEL bit 3..2 W1: width code of segment 1 | p50 | `MicrostepTable.w[1]` | OK |
| FLD-MSLUTSEL-03 | MSLUTSEL bit 5..4 W2: width code of segment 2 | p50 | `MicrostepTable.w[2]` | OK |
| FLD-MSLUTSEL-04 | MSLUTSEL bit 7..6 W3: width code of segment 3 | p50 | `MicrostepTable.w[3]` | OK |
| FLD-MSLUTSEL-05 | MSLUTSEL bit 15..8 X1: segment 1 start | p50 | `MicrostepTable.x1` | OK |
| FLD-MSLUTSEL-06 | MSLUTSEL bit 23..16 X2: segment 2 start | p50 | `MicrostepTable.x2` | OK |
| FLD-MSLUTSEL-07 | MSLUTSEL bit 31..24 X3: segment 3 start (0 < X1 < X2 < X3) | p50 | `MicrostepTable.x3` | OK |
| FLD-MSLUTSTART-01 | MSLUTSTART bit 7..0 START_SIN: table value at entry 0 | p47 | `MicrostepTable.start_sin` | OK |
| FLD-MSLUTSTART-02 | MSLUTSTART bit 23..16 START_SIN90: table value at entry 256 | p47 | `MicrostepTable.start_sin90` | OK |
| FLD-MSCNT-01 | MSCNT bit 9..0 MSCNT: position in the 1024-entry table | p47 | `ReadMicrostepCounter` | OK |
| FLD-MSCURACT-01 | MSCURACT bit 8..0 CUR_B: phase B current, signed (A7) | p47 | `Microstep.cur_b` | OK |
| FLD-MSCURACT-02 | MSCURACT bit 24..16 CUR_A: phase A current, signed (A7) | p47 | `Microstep.cur_a` | OK |
| FLD-CHOPCONF-01 | CHOPCONF bit 3..0 TOFF: off time, 0 = driver off; 1 only with TBL>=2 | p51-52 | `Chopper.toff`; `Enable`/`Disable` | GUARD |
| FLD-CHOPCONF-02 | CHOPCONF bit 6..4 HSTRT / TFD[2:0]: hysteresis start (chm=0) or fast decay time low bits (chm=1) | p51-52 | `SpreadCycle.hstrt`, `ConstOffTime.tfd` | OK |
| FLD-CHOPCONF-03 | CHOPCONF bit 10..7 HEND / OFFSET: hysteresis end (chm=0) or sine offset (chm=1) | p51-52 | `SpreadCycle.hend`, `ConstOffTime.offset` | OK |
| FLD-CHOPCONF-04 | CHOPCONF bit 11 fd3: MSB of TFD (chm=1) | p51-52 | `ConstOffTime.tfd` | OK |
| FLD-CHOPCONF-05 | CHOPCONF bit 12 disfdcc: no current comparator on fast decay (chm=1) | p51-52 | `ConstOffTime.no_comparator` | OK |
| FLD-CHOPCONF-06 | CHOPCONF bit 13 reserved 13: set to 0 | p51-52 | none: always 0 | OK |
| FLD-CHOPCONF-07 | CHOPCONF bit 14 chm: 0 SpreadCycle, 1 constant off time | p51-52 | `Chopper.mode` variant | OK |
| FLD-CHOPCONF-08 | CHOPCONF bit 16..15 TBL: blank time 16, 24, 36, 54 clocks | p51-52 | `Chopper.tbl` | OK |
| FLD-CHOPCONF-09 | CHOPCONF bit 17 reserved 17: set to 0 | p51-52 | none: always 0 | OK |
| FLD-CHOPCONF-10 | CHOPCONF bit 18 vhighfs: fullstep above THIGH | p51-52 | `Chopper.high_fullstep` | OK |
| FLD-CHOPCONF-11 | CHOPCONF bit 19 vhighchm: constant off time above THIGH | p51-52 | `Chopper.high_const_off` | OK |
| FLD-CHOPCONF-12 | CHOPCONF bit 23..20 TPFD: passive fast decay, 128 clocks per step | p51-52 | `Chopper.tpfd` | OK |
| FLD-CHOPCONF-13 | CHOPCONF bit 27..24 MRES: microstep resolution 256 ... fullstep | p51-52 | `Chopper.mres` | OK |
| FLD-CHOPCONF-14 | CHOPCONF bit 28 intpol: interpolate to 256 microsteps | p51-52 | `Chopper.interpolate` | OK |
| FLD-CHOPCONF-15 | CHOPCONF bit 29 dedge: step on both edges | p51-52 | `Chopper.double_edge` | OK |
| FLD-CHOPCONF-16 | CHOPCONF bit 30 diss2g: disable short-to-GND protection | p51-52 | `Protection.disable_s2g`, acknowledged | GUARD |
| FLD-CHOPCONF-17 | CHOPCONF bit 31 diss2vs: disable short-to-supply protection | p51-52 | `Protection.disable_s2vs`, acknowledged | GUARD |
| FLD-COOLCONF-01 | COOLCONF bit 3..0 SEMIN: CoolStep lower threshold, 0 = off | p53 | `CoolStep.semin` | OK |
| FLD-COOLCONF-02 | COOLCONF bit 6..5 SEUP: current increment 1, 2, 4, 8 | p53 | `CoolStep.seup` | OK |
| FLD-COOLCONF-03 | COOLCONF bit 11..8 SEMAX: CoolStep upper threshold offset | p53 | `CoolStep.semax` | OK |
| FLD-COOLCONF-04 | COOLCONF bit 14..13 SEDN: decrement rate 32, 8, 2, 1 readings | p53 | `CoolStep.sedn` | OK |
| FLD-COOLCONF-05 | COOLCONF bit 15 SEIMIN: minimum current 1/2 or 1/4 of IRUN | p53 | `CoolStep.min_quarter` | OK |
| FLD-COOLCONF-06 | COOLCONF bit 22..16 SGT: StallGuard2 threshold, signed -64..63 | p53 | `StallGuard.threshold` | OK |
| FLD-COOLCONF-07 | COOLCONF bit 24 SFILT: StallGuard2 filter | p53 | `StallGuard.filter` | OK |
| FLD-COOLCONF-08 | COOLCONF bit 31..25 reserved 4,7,12,23,31..25: reserved, set to 0 | p53 | none: always 0 | OK |
| FLD-DCCTRL-01 | DCCTRL bit 9..0 DC_TIME: upper PWM on-time limit, clocks | p48,p101 | `DcStep.dc_time` | OK |
| FLD-DCCTRL-02 | DCCTRL bit 23..16 DC_SG: stall PWM on-time, 16 clocks per count, 0 = off | p48,p101 | `DcStep.dc_sg` | OK |
| FLD-DRV_STATUS-01 | DRV_STATUS bit 9..0 SG_RESULT: StallGuard2 result 0-1023 (coil A on-time at standstill) | p56 | `DriverStatus.sg_result` | OK |
| FLD-DRV_STATUS-02 | DRV_STATUS bit 12 s2vsa: short to supply, phase A | p56 | `DriverStatus.s2vs_a` | OK |
| FLD-DRV_STATUS-03 | DRV_STATUS bit 13 s2vsb: short to supply, phase B | p56 | `DriverStatus.s2vs_b` | OK |
| FLD-DRV_STATUS-04 | DRV_STATUS bit 14 stealth: StealthChop active | p56 | `DriverStatus.stealth` | OK |
| FLD-DRV_STATUS-05 | DRV_STATUS bit 15 fsactive: fullstep active | p56 | `DriverStatus.full_step` | OK |
| FLD-DRV_STATUS-06 | DRV_STATUS bit 20..16 CS_ACTUAL: actual current scale | p56 | `DriverStatus.cs_actual` | OK |
| FLD-DRV_STATUS-07 | DRV_STATUS bit 24 StallGuard: stall detected | p56 | `DriverStatus.stall` | OK |
| FLD-DRV_STATUS-08 | DRV_STATUS bit 25 ot: overtemperature shutdown | p56 | `DriverStatus.over_temp` | OK |
| FLD-DRV_STATUS-09 | DRV_STATUS bit 26 otpw: overtemperature pre-warning | p56 | `DriverStatus.over_temp_warning` | OK |
| FLD-DRV_STATUS-10 | DRV_STATUS bit 27 s2ga: short to GND, phase A | p56 | `DriverStatus.s2g_a` | OK |
| FLD-DRV_STATUS-11 | DRV_STATUS bit 28 s2gb: short to GND, phase B | p56 | `DriverStatus.s2g_b` | OK |
| FLD-DRV_STATUS-12 | DRV_STATUS bit 29 ola: open load, phase A (informative) | p56 | `DriverStatus.open_load_a` | OK |
| FLD-DRV_STATUS-13 | DRV_STATUS bit 30 olb: open load, phase B (informative) | p56 | `DriverStatus.open_load_b` | OK |
| FLD-DRV_STATUS-14 | DRV_STATUS bit 31 stst: standstill | p56 | `DriverStatus.standstill` | OK |
| FLD-DRV_STATUS-15 | DRV_STATUS bit 23..21 reserved 11,10,23-21: ignore | p56 | none: masked | OK |
| FLD-PWMCONF-01 | PWMCONF bit 7..0 PWM_OFS: amplitude offset 0-255 | p54-55 | `StealthChop.ofs` | OK |
| FLD-PWMCONF-02 | PWMCONF bit 15..8 PWM_GRAD: amplitude gradient 0-255 | p54-55 | `StealthChop.grad` | OK |
| FLD-PWMCONF-03 | PWMCONF bit 17..16 pwm_freq: PWM frequency 2/1024, 2/683, 2/512, 2/410 of fCLK | p54-55 | `StealthChop.freq` | OK |
| FLD-PWMCONF-04 | PWMCONF bit 18 pwm_autoscale: automatic current control | p54-55 | `StealthChop.autoscale` | OK |
| FLD-PWMCONF-05 | PWMCONF bit 19 pwm_autograd: automatic gradient adaptation | p54-55 | `StealthChop.autograd` | OK |
| FLD-PWMCONF-06 | PWMCONF bit 21..20 freewheel: standstill mode with IHOLD=0 | p54-55 | `StealthChop.freewheel` | OK |
| FLD-PWMCONF-07 | PWMCONF bit 23..22 reserved 23,22: set to 0 | p54-55 | none: always 0 | OK |
| FLD-PWMCONF-08 | PWMCONF bit 27..24 PWM_REG: regulation gradient 1-15 | p54-55 | `StealthChop.reg` | OK |
| FLD-PWMCONF-09 | PWMCONF bit 31..28 PWM_LIM: amplitude limit when switching back | p54-55 | `StealthChop.lim` | OK |
| FLD-PWM_SCALE-01 | PWM_SCALE bit 7..0 PWM_SCALE_SUM: actual PWM duty 0-255 | p48 | `StealthChopState.scale_sum` | OK |
| FLD-PWM_SCALE-02 | PWM_SCALE bit 24..16 PWM_SCALE_AUTO: automatic offset, signed 9-bit | p48 | `StealthChopState.scale_auto` | OK |
| FLD-PWM_AUTO-01 | PWM_AUTO bit 7..0 PWM_OFS_AUTO: automatic offset result | p48-49 | `StealthChopState.ofs_auto` | OK |
| FLD-PWM_AUTO-02 | PWM_AUTO bit 23..16 PWM_GRAD_AUTO: automatic gradient result | p48-49 | `StealthChopState.grad_auto` | OK |
| FLD-LOST_STEPS-01 | LOST_STEPS bit 19..0 LOST_STEPS: steps skipped in DcStep, SD_MODE=1 only | p49 | `ReadLostSteps` | OK |

## Modes and features (88 rows)

| Id | Item | Datasheet | Driver API | Status |
| --- | --- | --- | --- | --- |
| MOD-01 | Interface: SPI, 40-bit datagram, address byte + 32 data bits | S4.1 p23 | `SpiInterface::Transfer` | OK |
| MOD-02 | SPI read is pipelined: data arrives with the next datagram | S4.1.1 p23-24 | `ReadRegister` (two transfers); batched reads (n+1) | OK |
| MOD-03 | SPI write reply returns the previous read data or the previous write data | S4.1.1 p24 | `Verify::Echo` (A5) | OK |
| MOD-04 | SPI_STATUS byte in every reply (8 flags) | S4.1.2 p24 | reset-flag handling of the status byte (A3) | OK |
| MOD-05 | SPI mode 3, MSB first, 40 clocks minimum, CSN low for the whole datagram | S4.2 p24, S4.3 p25 | port contract `kSpiMode = 3` (A1) | OK |
| MOD-06 | SPI daisy chain: more than 40 clocks shift through | S4.2 p24 | `SpiOptions.chain_length/index`, frame of 5n bytes (A34) | OK |
| MOD-07 | SPI clock limit: fCLK/2 (internal clock 4 MHz) | S4.3 p25 | `kMaxSckInternalHz`, optional `SckHz()` hook check (A2) | OK |
| MOD-08 | UART single-wire interface, 64-bit write, 32-bit read request, 64-bit reply | S5.1 p26-27 | `UartInterface::Exchange` | OK |
| MOD-09 | UART CRC-8 (polynomial 0x07), initial 0 | S5.2 p28 | `detail::Crc8` | OK |
| MOD-10 | UART automatic baud rate, 9000 baud to fCLK/16 | S5.1 p26 | `kUartMinBaud`, `UartOptions::baud` checked by `Validate()` | OK |
| MOD-11 | UART error recovery: 12 bit times idle after an error, 63-bit-time byte gap resets | S5.1 p26 | `DelayUs` of 12 bit times after a failed exchange (A33) | OK |
| MOD-12 | UART write acknowledge by IFCNT | S5.1 p26 | `Verify::Counter`, `ReadInterfaceCounter` | OK |
| MOD-13 | UART read reply delay SENDDELAY (8 bit times default, 2 or more with several nodes) | S5.1.2 p27, S5.4 p29 | `UartOptions.send_delay`, rule in `Validate` | OK |
| MOD-14 | UART node address, NAI/NAO chained addressing up to 255 nodes | S5.4 p29-30 | `ProgramNode` (A31) | OK |
| MOD-15 | UART differential SWP/SWN and RS485 use | S5.3 p28, Figure 5.2 p30 | board matter; port hook `Exchange` hides the transceiver | INFO |
| MOD-16 | Mode 1: motion controller (SD_MODE low) | S1 p5 | `Mode::MotionController` | OK |
| MOD-17 | Mode 2: STEP/DIR with SPI configuration (SD_MODE high, SPI_MODE high) | S1 p5, S15 p94 | `Mode::StepDir`; ramp, encoder, switch APIs return `Unsupported` | OK |
| MOD-18 | Mode 3: standalone STEP/DIR configured by CFG pins (SPI_MODE low, SD_MODE high) | S1 p5, S24 p117-118 | no driver use; documented, `EnsureInitialized` cannot talk to the chip | INFO |
| MOD-19 | UART mode is selected by SD_MODE and SPI_MODE both low | S2.2 p13, S5.3 p28 | `Config.bus`, strap check by an answered datagram | OK |
| MOD-20 | Microstep resolution 256 ... fullstep, MicroPlyer interpolation | S15.2 p95, S15.3 p96 | `Chopper.mres`, `interpolate` | OK |
| MOD-21 | STEP on both edges (dedge) | S15.1 p94 | `Chopper.double_edge` | OK |
| MOD-22 | Standstill detection 2^20 or 2^18 clocks, current reduction IRUN to IHOLD | S1.5 p8, S15.3 p96 | `Standstill`, `Flags.fast_standstill` | OK |
| MOD-23 | Automatic standstill power down: TPOWERDOWN, IHOLDDELAY | S6.2 p38, S9 p75 | `Standstill` | OK |
| MOD-24 | SixPoint ramp generator: VSTART, A1, V1, AMAX, VMAX, DMAX, D1, VSTOP, TZEROWAIT | S12.2 p82-83 | `RampProfile`, `MoveTo` | OK |
| MOD-25 | Positioning mode (RAMPMODE 0), on-the-fly target change, second_move | S12.2 p82 | `MoveTo`, `MoveBy` | OK |
| MOD-26 | Velocity mode (RAMPMODE 1, 2) using AMAX only | S12.2.3 p83 | `RunVelocity` | OK |
| MOD-27 | Hold mode (RAMPMODE 3) | S6.3.1 p40 | `Hold` | OK |
| MOD-28 | Early ramp termination options a to d | S12.2.4 p83 | `Stop(StopKind)`: `Ramp`, `Immediate`, `HoldTarget`, `Emergency` | OK |
| MOD-29 | Joystick control by changing VMAX in positioning mode | S12.2.5 p84 | `SetMaxVelocity` (clamped by `Config::max_velocity_reg`) | OK |
| MOD-30 | Velocity thresholds TPWMTHRS, TCOOLTHRS, THIGH and their ordering | S10 p76-77, S12.3 p84 | `Thresholds`, ordering rule (A15) | OK |
| MOD-31 | Hysteresis 1/16 or 1/32 on the TSTEP thresholds | S6.2 p38, S10 p76 | `Thresholds.small_hysteresis` | OK |
| MOD-32 | Real-world unit conversion (velocity, acceleration, TSTEP, update rate fCLK/512) | S12.1 p81 | `units::VelocityRegFromHz`, `AccelRegFromHzPerS`, `StepTimeFromVelocity`, ... | OK |
| MOD-33 | Reference switches: stop, polarity, swap, soft or hard stop | S12.4 p85, S6.3.2.1 p43 | `Switches` | OK |
| MOD-34 | Position latch on switch events (XLATCH), optional encoder latch | S12.4 p85, p43 | `Switches.latch_*`, `ReadLatchedPosition` | OK |
| MOD-35 | Homing procedure with a switch | S12.4 p85-86 | example `homing_switch`, primitives `Switches`, `SetHomePosition` | OK |
| MOD-36 | Homing with a third switch on the encoder N channel | S12.4 p86 | example, `Encoder.n_event` | OK |
| MOD-37 | Homing with StallGuard2 | S13.4 p90 | `SetStopOnStall`, `ReleaseStall`, example `homing_stall` | OK |
| MOD-38 | Ramp generator response time (first and last step delay, VSTOP 100-1000 advice) | S12.5 p86 | documented advice (no API) | INFO |
| MOD-39 | StealthChop2 voltage PWM, automatic tuning AT#1 and AT#2 | S7 p57-59 | `StealthChop`, `ReadStealthChopState` | OK |
| MOD-40 | StealthChop options: autoscale, autograd, feed-forward mode, PWM frequency | S7.2 p60 | `StealthChop` | OK |
| MOD-41 | StealthChop regulator and lower current limit formula | S7.3 p60, S7.3.1 p62 | `units::StealthChopLowerLimitMa`, `Validate` | OK |
| MOD-42 | Velocity-based PWM scaling, PWM_OFS and PWM_GRAD estimates | S7.4 p63-64 | not implemented (PWM_OFS and PWM_GRAD are set by the caller) | INFO |
| MOD-43 | StealthChop combined with SpreadCycle via TPWMTHRS | S7.5 p64-65 | `Thresholds.pwm` | OK |
| MOD-44 | StealthChop flags and open load behaviour, overcurrent warning | S7.6 p66 | docs; open-load test must use SpreadCycle | INFO |
| MOD-45 | Freewheeling and passive braking (IHOLD=0, FREEWHEEL) | S7.7 p66, p67 | `StealthChop.freewheel` (rule A18) | OK |
| MOD-46 | SpreadCycle chopper: TOFF, TBL, HSTRT, HEND, TPFD | S8.1 p69-71 | `SpreadCycle`, `Chopper` | OK |
| MOD-47 | Classic constant off-time chopper: TFD, OFFSET, disfdcc | S8.2 p72-73 | `ConstOffTime` | OK |
| MOD-48 | Chopper frequency estimate and TOFF start value | S8.1 p69 | not implemented (advice in the docs) | INFO |
| MOD-49 | Sense resistor selection and current formulas | S9 p74-75 | `PlanCurrent`, `units::RmsMa()`, `Motor`, `Board` | OK |
| MOD-50 | Current scaling chain: global scaler, IRUN, IHOLD, CoolStep | S9 p74-75, S14 p92 | `CurrentPlan` | OK |
| MOD-51 | StallGuard2 load measurement, SGT tuning, sfilt | S13 p87-90 | `StallGuard`, `ReadStallGuard` | OK |
| MOD-52 | StallGuard2 limits (velocity range, TCOOLTHRS) | S13.5 p90 | rule: `sg_stop` needs TCOOLTHRS (A38) | OK |
| MOD-53 | CoolStep: SEMIN, SEMAX, SEUP, SEDN, SEIMIN, CS_ACTUAL | S14 p91-93 | `CoolStep` | OK |
| MOD-54 | DcStep: VDCMIN, DC_TIME, DC_SG, vhighfs, vhighchm, TOFF | S17 p99-101 | `DcStep`, rules (A44) | OK |
| MOD-55 | DcStep with STEP/DIR: DCEN, DCO, DCIN pins, LOST_STEPS | S17.6 p102-104 | `ReadLostSteps`; the DCEN, DCIN and DCO pins are the board's: the driver never drives `CtrlPin::DcEnable` or `CtrlPin::DcIn` and never reads `InputPin::DcOut` | INFO |
| MOD-56 | DcStep velocity measurement from XACTUAL snapshots | S17.5 p101 | `units::DcStepVelocity()`, `ReadLostSteps()` | OK |
| MOD-57 | Programmable microstep table, segments, start values | S18 p105-106, S6.5.1 p50 | `MicrostepTable`, `SetMicrostepTable` (MSCNT zero rule) | OK |
| MOD-58 | Default table initialisation words | S18.2 p106 | `MicrostepTable::Default()` | OK |
| MOD-59 | Emergency stop: DRV_ENN pin and ENCA_DCIN stop_enable | S19 p106 | `Disable`, `Flags.stop_input`, hook `GpioSet(CtrlPin::DrvEnn)` | OK |
| MOD-60 | ABN encoder: constant, counter, N event, latch, deviation | S20 p107-109 | `Encoder`, `EncoderScale` | OK |
| MOD-61 | Encoder resolution matching table | S20.2 p108 | `units::EncoderScaleFor()` | OK |
| MOD-62 | DC motor or solenoid: direct mode | S21 p110 | `EnterDirectMode(kind)`, `SetDirectCurrents`, acknowledged (A26) | GUARD |
| MOD-63 | Short protection: s2g, s2vs, filter, delay, three retries, restart by disable and enable | S11.2 p78-79 | `Protection`, `ClearFaults` (toggles TOFF) | OK |
| MOD-64 | Overtemperature pre-warning 120 C and shutdown 136/143/150 C | S11.1 p78 | `Bridge.ot_level`, `DriverStatus` | OK |
| MOD-65 | Open load diagnostics (informative flags) | S11.3 p80 | `DriverStatus.open_load_*`, docs | OK |
| MOD-66 | Charge pump undervoltage flag uv_cp | S6.1 p33 | `GlobalStatus::uv_cp` | OK |
| MOD-67 | DIAG outputs in STEP/DIR mode | S16.1 p97 | `Diag` (StepDir variant) | OK |
| MOD-68 | DIAG outputs in motion-controller mode: interrupt, step, position compare, direction | S16.2 p97-98 | `Diag` (MotionController variant), `SetPositionCompare` | OK |
| MOD-69 | DIAG0 shows reset; event_pos_reached active after reset | S16.2 p97-98 | reset detected by flag, not by the pin (A39) | OK |
| MOD-70 | Power-up reset and reset on undervoltage of VSA, 5VOUT, VCC_IO | S25 p119 | `UnexpectedReset` handling, `EnsureInitialized` after reset | OK |
| MOD-71 | Software restart sequence: TOFF=0, re-initialise, stop ramp | S25 p119 | `EnsureInitialized` order | OK |
| MOD-72 | Internal clock 12 MHz or external clock 4 to 18 MHz | S26 p119, S28.2 p122 | `Clock`, `fclk_hz` | OK |
| MOD-73 | Clock source switch by the second rising edge; watchdog fallback | S26.2 p119 | documented, `Clock` is declared by the user (A28) | INFO |
| MOD-74 | OTP memory: read of defaults | S6.1.1 p37 | `ReadOtp` | OK |
| MOD-75 | OTP programming | S6.1 p34-35 | out of scope (A40) | TODO |
| MOD-76 | Chip identification: VERSION 0x30 | S6.1 p34 | `Identify`, check in `EnsureInitialized` (A29) | OK |
| MOD-77 | TMC5160 and TMC5160A both covered; A-version replaces non-A | S1 p2, S28.2 p122 | one driver, known differences listed (A45, A37) | OK |
| MOD-78 | TMC5130 compatibility | S1 p2 | out of scope, `DeviceIdMismatch` (A45) | INFO |
| MOD-79 | Application circuits, external gate supply, higher voltage with an external gate driver | S3 p15-22 | docs and `hardware.yml` circuits; `Protection.disable_s2g` acknowledged for the external driver | INFO |
| MOD-80 | MOSFET selection, DRVSTRENGTH versus Miller charge and RG | S3.3 p17 | docs table; advisory only | INFO |
| MOD-81 | Bridge tuning and break-before-make coverage (30 percent reserve) | S3.3 p18, S3.4 p19 | `Bridge.mosfet_switching_ns` check in `Validate` | OK |
| MOD-82 | Layout considerations | S29 p125-127 | docs | INFO |
| MOD-83 | Write-only registers need a software shadow | S1.3 p7 | shadow of every write-only register | OK |
| MOD-84 | Position compare strobe X_COMPARE on DIAG1 | S6.1 p34 | `SetPositionCompare` | OK |
| MOD-85 | Motor direction inversion (shaft) | S6.1 p32 | `Flags.invert_direction` | OK |
| MOD-86 | Zero-crossing recalibration option | S6.1 p32 | `Flags.recalibrate` | OK |
| MOD-87 | Quick configuration guide (22) and initialisation examples (23.1) | S22 p111-115, S23.1 p116 | examples `quickstart`, tests replay the p116 datagrams | OK |
| MOD-88 | Eval kit and boards (EVAL, BOB, SilentStepStick) | order codes p3 | README variants only; no driver behaviour | INFO |

## Pins (48 rows: 47 signal pins as numbered for TQFP-48 and QFN-WA, plus the pad)

| Id | Item | Datasheet | Driver API | Status |
| --- | --- | --- | --- | --- |
| PIN-01 | HB1 (TQFP 1, QFN 2): high-side gate driver output, bridge B high side 1 gate | S2.2 p12-14, Figures 2.1, 2.2 p11 | none: bridge hardware | INFO |
| PIN-02 | CB1 (TQFP 2, QFN 3): bootstrap, bootstrap capacitor positive | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-03 | 12VOUT (TQFP 3, QFN 4): power output, 11.5 V gate regulator output, low-side drivers supply | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-04 | VSA (TQFP 4, QFN 5): power, analog supply for the 11.5 V and 5 V regulators | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Board.supply_mv` declared | INFO |
| PIN-05 | 5VOUT (TQFP 5, QFN 6): power output, 5 V regulator output | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-06 | GNDA (TQFP 6, QFN 7): ground, analog ground | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-07 | SRAL (TQFP 7, QFN 8): analog in, sense resistor A, GND side | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Board.rsense_mohm` declared | INFO |
| PIN-08 | SRAH (TQFP 8, QFN 9): analog in, sense resistor A, upper side | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Board.rsense_mohm` | INFO |
| PIN-09 | SRBH (TQFP 9, QFN 10): analog in, sense resistor B, upper side | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Board.rsense_mohm` | INFO |
| PIN-10 | SRBL (TQFP 10, QFN 11): analog in, sense resistor B, GND side | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Board.rsense_mohm` | INFO |
| PIN-11 | TST_MODE (TQFP 11, QFN 12): digital in, test mode, tie to GND | S2.2 p12-14, Figures 2.1, 2.2 p11 | none: strap | INFO |
| PIN-12 | CLK (TQFP 12, QFN 13): digital in, clock input; GND for internal clock | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Clock` | OK |
| PIN-13 | CSN_CFG3 (TQFP 13, QFN 14): digital in, SPI chip select (SPI_MODE=1) or CFG3 | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Transfer` (CSN) | OK |
| PIN-14 | SCK_CFG2 (TQFP 14, QFN 15): digital in, SPI clock or CFG2 | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Transfer` (SCK) | OK |
| PIN-15 | SDI_CFG1 (TQFP 15, QFN 16): digital in, SPI data in, CFG1, or NAI in UART mode | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Transfer` (MOSI); UART chain input | OK |
| PIN-16 | SDO_CFG0 (TQFP 16, QFN 17): digital I/O, SPI data out (tristate), CFG0, or NAO in UART mode | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Transfer` (MISO); `ProgramNode` NAO | OK |
| PIN-17 | REFL_STEP (TQFP 17, QFN 18): digital in, left reference switch or STEP input | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Switches` / STEP source | OK |
| PIN-18 | REFR_DIR (TQFP 18, QFN 19): digital in, right reference switch or DIR input | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Switches` / DIR source | OK |
| PIN-19 | GNDD (TQFP 19, QFN -): ground, digital ground (TQFP 19 and 30; QFN 25) | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-20 | VCC_IO (TQFP 20, QFN 20): power, 3.3 V to 5 V I/O supply | S2.2 p12-14, Figures 2.1, 2.2 p11 | docs only (no driver function) | INFO |
| PIN-21 | SD_MODE (TQFP 21, QFN 21): digital in, low: ramp generator; high: STEP/DIR; both low with SPI_MODE: UART | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Config.mode`, strap check in `EnsureInitialized` | OK |
| PIN-22 | SPI_MODE (TQFP 22, QFN 22): digital in (pull-down), high: SPI; low with SD_MODE high: standalone | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Config.bus` | OK |
| PIN-23 | ENCB_DCEN_CFG4 (TQFP 23, QFN 23): digital in (pull-down), encoder B or DcStep enable or CFG4 | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Encoder`; the pin is the board's (the driver never drives `CtrlPin::DcEnable`) | INFO |
| PIN-24 | ENCA_DCIN_CFG5 (TQFP 24, QFN 24): digital in (pull-down), encoder A or DcStep gating or CFG5; emergency stop input | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Encoder`, `Flags.stop_input` | OK |
| PIN-25 | ENCN_DCO_CFG6 (TQFP 25, QFN 26): digital I/O, encoder N or DcStep ready output or CFG6 | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Encoder.n_event`, the pin is the board's (the driver never reads `InputPin::DcOut`) | INFO |
| PIN-26 | DIAG0_SWN (TQFP 26, QFN 27): digital I/O, DIAG0 interrupt or STEP output, or single wire N | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Diag`, `GpioGet(Pin::Diag0)` | OK |
| PIN-27 | DIAG1_SWP (TQFP 27, QFN 28): digital I/O (pull-down), DIAG1 position compare or DIR output, or single wire P | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Diag`, `GpioGet(Pin::Diag1)`; UART line | OK |
| PIN-28 | DRV_ENN (TQFP 28, QFN 29): digital in, driver enable, active low | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Enable`/`Disable` with `GpioSet(CtrlPin::DrvEnn)` | OK |
| PIN-29 | VCC (TQFP 29, QFN 30): power, 5 V digital supply, from 5VOUT | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-30 | CPO (TQFP 31, QFN 31): charge pump, charge pump capacitor output | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-31 | CPI (TQFP 32, QFN 32): charge pump, charge pump capacitor input | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-32 | VS (TQFP 33, QFN 33): power, motor and bridge supply | S2.2 p12-14, Figures 2.1, 2.2 p11 | `Board.supply_mv` declared | INFO |
| PIN-33 | VCP (TQFP 34, QFN 34): charge pump, charge pump voltage | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-34 | CA2 (TQFP 35, QFN 35): bootstrap, bootstrap capacitor positive, bridge A2 | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-35 | HA2 (TQFP 36, QFN 36): gate driver, high-side gate A2 | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-36 | BMA2 (TQFP 37, QFN 37): bridge, bridge centre A2 | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-37 | LA2 (TQFP 38, QFN 38): gate driver, low-side gate A2 | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-38 | LA1 (TQFP 39, QFN 39): gate driver, low-side gate A1 | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-39 | BMA1 (TQFP 40, QFN 40): bridge, bridge centre A1 | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-40 | HA1 (TQFP 41, QFN 41): gate driver, high-side gate A1 | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-41 | CA1 (TQFP 42, QFN 42): bootstrap, bootstrap capacitor positive, bridge A1 | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-42 | CB2 (TQFP 43, QFN 43): bootstrap, bootstrap capacitor positive, bridge B2 | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-43 | HB2 (TQFP 44, QFN 44): gate driver, high-side gate B2 | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-44 | BMB2 (TQFP 45, QFN 45): bridge, bridge centre B2 | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-45 | LB2 (TQFP 46, QFN 46): gate driver, low-side gate B2 | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-46 | LB1 (TQFP 47, QFN 47): gate driver, low-side gate B1 | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-47 | BMB1 (TQFP 48, QFN 1): bridge, bridge centre B1 | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |
| PIN-48 | EP (TQFP pad, QFN pad): ground, exposed die pad = GNDD, GNDP; heat path | S2.2 p12-14, Figures 2.1, 2.2 p11 | none | INFO |

## Ratings, electrical, timing, package (53 rows)

| Id | Item | Datasheet | Driver API | Status |
| --- | --- | --- | --- | --- |
| ELE-01 | Absolute maximum: Supply VS, VSA with inductive load: -0.5 to 60 V | S27 p120 | docs only | INFO |
| ELE-02 | Absolute maximum: Supply and bridge short-time peak 64 V | S27 p120 | docs only | INFO |
| ELE-03 | Absolute maximum: VSA when different from VS: -0.5 to 60 V | S27 p120 | docs only | INFO |
| ELE-04 | Absolute maximum: Peaks on Cxx bootstrap pins and VCP: 76 V | S27 p120 | docs only | INFO |
| ELE-05 | Absolute maximum: V12VOUT: -0.5 to 14 V | S27 p120 | docs only | INFO |
| ELE-06 | Absolute maximum: Peaks on BM pins: -6 V to VS+6 V | S27 p120 | docs only | INFO |
| ELE-07 | Absolute maximum: Cxx relative to BM: -0.5 to 16 V | S27 p120 | docs only | INFO |
| ELE-08 | Absolute maximum: VCC_IO: -0.5 to 5.5 V | S27 p120 | docs only | INFO |
| ELE-09 | Absolute maximum: VCC: -0.5 to 5.5 V | S27 p120 | docs only | INFO |
| ELE-10 | Absolute maximum: Logic input: -0.5 V to VIO+0.5 V | S27 p120 | docs only | INFO |
| ELE-11 | Absolute maximum: Digital pin current: +/-500 mA short peak | S27 p120 | docs only | INFO |
| ELE-12 | Absolute maximum: 5V regulator output current 30 mA, dissipation 1 W | S27 p120 | docs only | INFO |
| ELE-13 | Absolute maximum: 12V regulator output current 20 mA, dissipation 0.5 W | S27 p120 | docs only | INFO |
| ELE-14 | Absolute maximum: Junction temperature -50 to 150 C | S27 p120 | docs only | INFO |
| ELE-15 | Absolute maximum: Storage temperature -55 to 150 C | S27 p120 | docs only | INFO |
| ELE-16 | Absolute maximum: ESD interface pins 4 kV HBM, handling 1 kV HBM | S27 p120 | docs only | INFO |
| ELE-17 | Operating junction temperature -40 to 125 C | S28.1 p120 | docs only | INFO |
| ELE-18 | Motor supply VS 10 V to 55 V | S28.1 p120 | `Board.supply_mv` validated 10000..55000 (`kVsMinMv`, `kVsMaxMv`) | OK |
| ELE-19 | Supply VSA 10 V to 50 V (13 V max when 12VOUT is bridged) | S28.1 p120 | docs; `Board.supply_mv` bound | OK |
| ELE-20 | Lower supply down to 8 V with reduced specification, S2G protection not functional | S28.1 p120 | refused: `Board.supply_mv` below 10 V (A50) | GUARD |
| ELE-21 | VCC_IO 3.00 V to 5.25 V | S28.1 p120 | docs only | INFO |
| ELE-22 | Title page range 8 V to 60 V versus operating range 10 V to 55 V | p1, p120 | A50: operating range wins | INFO |
| ELE-23 | Supply current, driver disabled, 12 MHz: 18 mA typ, 24 mA max (VS=VSA=24 V) | S28.2 p121 | docs | INFO |
| ELE-24 | VSA supply current 15 mA typ; operating 25 mA typ; IVCC 10 mA (12 MHz), 12.5 mA (16 MHz); IVIO 15-30 uA | S28.2 p121 | docs | INFO |
| ELE-25 | RDSON off drivers: low side 1.8 ohm typ (3 max), high side 2.2 ohm typ (3.5 max) | S28.2 p121 | docs (RG limits) | INFO |
| ELE-26 | Gate drive current low side 200/400/600 mA, high side 150/300/450 mA (DRVSTRENGTH 0, 2, 3) | S28.2 p121 | docs table for `Bridge.drive_strength` | INFO |
| ELE-27 | tBBM0 100 ns (75 min), tBBM16 200 ns, tBBM24 375 ns typ (500 max) | S28.2 p121 | `Bridge` effective BBM computation (A10) | OK |
| ELE-28 | Charge pump: output VS+V12VOUT-2 V, undervoltage 4.5-6.5 V (5 V typ), fCP = fCLKOSC/16 | S28.2 p121 | `GlobalStatus::uv_cp` | OK |
| ELE-29 | 5VOUT 4.8 to 5.2 V; 12VOUT 10.8 to 12.2 V (11.5 V typ) | S28.2 p122 | docs | INFO |
| ELE-30 | Clock oscillator 11.5 to 12.5 MHz at 50 C (11.7 at -50 C, 12.1 at 150 C), +-4 percent practical | S28.2 p122, S26.1 p119 | `kInternalClockHz = 12'000'000`, `Clock::Internal` | OK |
| ELE-31 | External clock 4 to 18 MHz (10-16 recommended; 18 MHz at 50 percent duty) | S28.2 p122, S26.2 p119 | `Clock::External{hz}` validated 4..18 MHz, advisory outside 10..16 | OK |
| ELE-32 | External clock high/low time 16 ns; first pulse 16 ns (A) or 30/25 ns (non-A); timeout 32-48 cycles | S28.2 p122 | docs for the board (clock generator) | INFO |
| ELE-33 | Short detector delay tSD0 0.85 us typ, tSD1 1.6 us typ (shortdelay 0 or 1) | S28.2 p122 | docs; `Protection.long_delay` | INFO |
| ELE-34 | S2VS level 0.625 V typ at 6, 1.56 V at 15; S2G level 0.625 V at 6, 1.56 V at 15 (VS<52 V) | S28.2 p122 | docs only | INFO |
| ELE-35 | S2G level at VS<55 V and level 15: 0.85 V min | S28.2 p122 | rule A11 | INFO |
| ELE-36 | VSA undervoltage reset 4 V (3.6-4.6), 5VOUT reset 3.5 V, VCC_IO reset 2.5 V (2.0-3.0, 0.3 V hysteresis) | S28.2 p123 | docs; reset model | OK |
| ELE-37 | Overtemperature: pre-warning 120 C (100-140), shutdown 136, 143, 150 C | S28.2 p123, S11.1 p78 | `Bridge.ot_level` | OK |
| ELE-38 | Sense input peak threshold VSRT 325 mV typ (GLOBALSCALER=0, CS=31) | S28.2 p123 | `kVfsMv = 325` (A20) | OK |
| ELE-39 | Motor current full-scale tolerance +-5 percent | S28.2 p123 | `kCurrentTolerancePercent = 5` in the ceiling check (A20) | OK |
| ELE-40 | Digital pins: VIL 0.3 VIO, VIH 0.7 VIO, hysteresis 0.12 VIO, IOL 2 mA at 0.2 V, leakage +-10 uA, pull 132-200 kohm | S28.2 p123 | docs; port notes | INFO |
| ELE-41 | Typical power dissipation 0.6 W; RthJA 21 K/W; RthJB 8 K/W; RthJC 3 K/W (TQFP48-EP) | S28.3 p124 | docs; no driver use | INFO |
| ELE-42 | SPI: tCC 10 ns, tCSH > 2 tCLK + 10 ns, tCL and tCH > tCLK + 10 ns, tDU 10 ns, tDH 10 ns, tDO tFILT + 5 ns, tFILT 12-30 ns | S4.3 p25 | `kMinCsHighNs`, `kCsSetupNs` constants; the rest is the port's | OK |
| ELE-43 | SPI fSCK 4 MHz with the internal clock, 8 MHz with a 16 MHz external clock; at most fCLK/2 with 10 percent margin on the internal oscillator | S4.3 p25 | optional `SckHz()` hook check (A2) | OK |
| ELE-44 | Inputs (SPI, DRV_ENN) filter pulses shorter than 20 ns | S4.3 p25 | docs | INFO |
| ELE-45 | STEP/DIR timing: fSTEP fCLK/2 (dedge=0), fCLK/4 (dedge=1); tSL, tSH 100 ns; DIR setup and hold 20 ns; fFS fCLK/512 | S15.1 p94 | docs; the step source is the board's | OK |
| ELE-46 | Encoder: fCNT up to 2/3 fCLK (below fCLK), A/B/N low and high time 3 tCLK + 20 ns | S20.1 p108 | docs; `Encoder` advisory on counting rate | INFO |
| ELE-47 | UART: 9000 baud minimum at 20 MHz, maximum fCLK/16; byte gap limit 63 bit times; recovery 12 bit times; reply after SENDDELAY | S5.1 p26-27 | `kUartMinBaud = 9000`, `Validate()` of `UartOptions::baud`, `kUartRecoveryBits = 12` | OK |
| ELE-48 | Update rate of the ramp generator fCLK/512; velocity unit t = 2^24/fCLK; acceleration unit ta^2 = 2^41/fCLK^2 | S6.3.2 p42, S12.1 p81 | `units` conversions | OK |
| ELE-49 | TZEROWAIT unit 512 clocks (about 2 s at 16 MHz maximum); TPOWERDOWN unit 2^18 clocks (about 4 s); IHOLDDELAY unit 2^18 clocks | S6.3.1 p41, S6.2 p38 | `units::ZeroWaitUs`, `PowerDownUs`, `HoldDelayUs` | OK |
| ELE-50 | Package TQFP48-EP 7 x 7 mm body, 9 x 9 mm with leads, 0.5 mm pitch, 1.2 mm max, exposed pad 5.0 x 5.0 mm | S30.1 p128-129 | `hardware.yml` package.outline | INFO |
| ELE-51 | Package QFN-WA 8 x 8 mm, 0.5 mm pitch, 0.85 mm nominal, exposed pad 6.25 x 6.25 mm | S30.2 p130-131 | `hardware.yml` variant | INFO |
| ELE-52 | Order codes TMC5160A-TA, -TA-T, -WA, -WA-T; temperature -40 to +125 C | order codes p3, S30.3 p131 | `hardware.yml` ordering | INFO |
| ELE-53 | Boards: TMC5160-EVAL 85 x 55 mm, TMC5160-EVAL-KIT 126 x 85 mm, TMC5160-BOB 38 x 28 mm, Silent StepStick 20 x 15 mm | order codes p3 | README variants (no chip behaviour) | INFO |

## Coded fields, ranges and guarded values (144 rows)

| Id | Item | Datasheet | Driver API | Status |
| --- | --- | --- | --- | --- |
| COD-001 | CHOPCONF.MRES = %0000: 256 (native) microsteps | S6.5.2 p51 | `Microsteps::M256` | OK |
| COD-002 | CHOPCONF.MRES = %0001: 128 microsteps | S6.5.2 p51 | `Microsteps::M128` | OK |
| COD-003 | CHOPCONF.MRES = %0010: 64 microsteps | S6.5.2 p51 | `Microsteps::M64` | OK |
| COD-004 | CHOPCONF.MRES = %0011: 32 microsteps | S6.5.2 p51 | `Microsteps::M32` | OK |
| COD-005 | CHOPCONF.MRES = %0100: 16 microsteps | S6.5.2 p51 | `Microsteps::M16` | OK |
| COD-006 | CHOPCONF.MRES = %0101: 8 microsteps | S6.5.2 p51 | `Microsteps::M8` | OK |
| COD-007 | CHOPCONF.MRES = %0110: 4 microsteps | S6.5.2 p51 | `Microsteps::M4` | OK |
| COD-008 | CHOPCONF.MRES = %0111: 2 microsteps | S6.5.2 p51 | `Microsteps::M2` | OK |
| COD-009 | CHOPCONF.MRES = %1000: fullstep | S6.5.2 p51 | `Microsteps::Full` | OK |
| COD-010 | CHOPCONF.MRES = %1001..%1111: undefined | p51 | unreachable (enum has no such value) | GUARD |
| COD-011 | CHOPCONF.TBL = %00: blank time 16 clocks | S6.5.2 p51-52 | `Blank::k16` | OK |
| COD-012 | CHOPCONF.TBL = %01: blank time 24 clocks (recommended) | S6.5.2 p51-52 | `Blank::k24` | OK |
| COD-013 | CHOPCONF.TBL = %10: blank time 36 clocks (recommended) | S6.5.2 p51-52 | `Blank::k36` | OK |
| COD-014 | CHOPCONF.TBL = %11: blank time 54 clocks | S6.5.2 p51-52 | `Blank::k54` | OK |
| COD-015 | CHOPCONF.TOFF = 0: driver disabled, all bridges off | p52 | `Disable`; `Chopper.toff` must be 1-15 | GUARD |
| COD-016 | CHOPCONF.TOFF = 1: only with TBL >= 2 | p52 | `Validate` rule | GUARD |
| COD-017 | CHOPCONF.TOFF = 2..15: NCLK = 24 + 32*TOFF | p52 | `Chopper.toff`, the register value | OK |
| COD-018 | CHOPCONF.HSTRT = %000..%111: adds 1..8 to HEND | p52 | `SpreadCycle.hstrt` 0-7 | OK |
| COD-019 | CHOPCONF.HEND = %0000..%1111: hysteresis -3..12 | p52 | `SpreadCycle.hend` 0-15 | OK |
| COD-020 | CHOPCONF.HSTRT+HEND = effective sum: must be <= 16 (A16) | p52, p71 | `Validate` rule | GUARD |
| COD-021 | CHOPCONF.TFD = %0000..%1111: fast decay 32*TFD clocks, 0 = slow decay only | p52 | `ConstOffTime.tfd` 0-15 | OK |
| COD-022 | CHOPCONF.OFFSET = %0000..%1111: sine offset -3..12 | p52 | `ConstOffTime.offset` 0-15 | OK |
| COD-023 | CHOPCONF.TPFD = 0: passive fast decay disabled | p51 | `Chopper.tpfd` 0 | OK |
| COD-024 | CHOPCONF.TPFD = 1..15: 128*TPFD clocks | p51 | `Chopper.tpfd` 1-15 | OK |
| COD-025 | DRV_CONF.OTSELECT = %00: shutdown 150 C | S6.1 p36 | `OtLevel::C150` | OK |
| COD-026 | DRV_CONF.OTSELECT = %01: shutdown 143 C | S6.1 p36 | `OtLevel::C143` | OK |
| COD-027 | DRV_CONF.OTSELECT = %10: shutdown 136 C (not recommended above VSA 24 V) | S6.1 p36 | `OtLevel::C136` | GUARD |
| COD-028 | DRV_CONF.OTSELECT = %11: shutdown 120 C (not recommended, no hysteresis) | S6.1 p36 | `OtLevel::C120` | GUARD |
| COD-029 | DRV_CONF.DRVSTRENGTH = %00: weak | S6.1 p36 | `Bridge::drive_strength` 0 | OK |
| COD-030 | DRV_CONF.DRVSTRENGTH = %01: weak + TC (medium above OTPW) | S6.1 p36 | `Bridge::drive_strength` 1 | OK |
| COD-031 | DRV_CONF.DRVSTRENGTH = %10: medium | S6.1 p36 | `Bridge::drive_strength` 2 | OK |
| COD-032 | DRV_CONF.DRVSTRENGTH = %11: strong | S6.1 p36 | `Bridge::drive_strength` 3 | OK |
| COD-033 | DRV_CONF.FILT_ISENSE = %00: sense filter about 100 ns | S6.1 p36 | `Bridge::sense_filter` 0 | OK |
| COD-034 | DRV_CONF.FILT_ISENSE = %01: sense filter about 200 ns | S6.1 p36 | `Bridge::sense_filter` 1 | OK |
| COD-035 | DRV_CONF.FILT_ISENSE = %10: sense filter about 300 ns | S6.1 p36 | `Bridge::sense_filter` 2 | OK |
| COD-036 | DRV_CONF.FILT_ISENSE = %11: sense filter about 400 ns | S6.1 p36 | `Bridge::sense_filter` 3 | OK |
| COD-037 | DRV_CONF.BBMTIME = 0..24: 100 ns * 32/(32 - BBMTIME) (A10) | p18, p35 | `Bridge.bbm_time` 0-24 | OK |
| COD-038 | DRV_CONF.BBMTIME = 25..31: not recommended, use BBMCLKS | p35 | refused | GUARD |
| COD-039 | DRV_CONF.BBMCLKS = 0..15: clock cycles (about 83 ns each); longer of the two rules | p36 | `Bridge.bbm_clks` 0-15 | OK |
| COD-040 | SHORT_CONF.SHORTFILTER = 0: spike filter 100 ns | S6.1 p35 | `ShortFilter::k0` | OK |
| COD-041 | SHORT_CONF.SHORTFILTER = 1: spike filter 1 us | S6.1 p35 | `ShortFilter::k1` | OK |
| COD-042 | SHORT_CONF.SHORTFILTER = 2: spike filter 2 us | S6.1 p35 | `ShortFilter::k2` | OK |
| COD-043 | SHORT_CONF.SHORTFILTER = 3: spike filter 3 us | S6.1 p35 | `ShortFilter::k3` | OK |
| COD-044 | SHORT_CONF.S2VS_LEVEL = 0..3: 1-3 trigger in normal operation; 0 undefined | p35,p78 | refused (A11) | GUARD |
| COD-045 | SHORT_CONF.S2VS_LEVEL = 4..15: 4 most sensitive, 6-8 recommended | p35,p78 | `Protection.s2vs_level` | OK |
| COD-046 | SHORT_CONF.S2G_LEVEL = 0..1: undefined | p35 | refused | GUARD |
| COD-047 | SHORT_CONF.S2G_LEVEL = 2..11: 2 most sensitive; below 12 refused when VS can exceed 52 V (A11) | p35,p78-79 | `Protection.s2g_level` | GUARD |
| COD-048 | SHORT_CONF.S2G_LEVEL = 12..15: 12-15 for supplies up to 55 V | p79 | `Protection.s2g_level` | OK |
| COD-049 | PWMCONF.pwm_freq = 0: fPWM = 2/1024 fCLK | S6.5.4 p55, Table 7.1 p60 | `PwmFrequency::k0` | OK |
| COD-050 | PWMCONF.pwm_freq = 1: fPWM = 2/683 fCLK | S6.5.4 p55, Table 7.1 p60 | `PwmFrequency::k1` | OK |
| COD-051 | PWMCONF.pwm_freq = 2: fPWM = 2/512 fCLK | S6.5.4 p55, Table 7.1 p60 | `PwmFrequency::k2` | OK |
| COD-052 | PWMCONF.pwm_freq = 3: fPWM = 2/410 fCLK | S6.5.4 p55, Table 7.1 p60 | `PwmFrequency::k3` | OK |
| COD-053 | PWMCONF.freewheel = %00: normal operation | S6.5.4 p54 | `Freewheel::k0` | OK |
| COD-054 | PWMCONF.freewheel = %01: freewheeling | S6.5.4 p54 | `Freewheel::k1` | OK |
| COD-055 | PWMCONF.freewheel = %10: coil short, low side | S6.5.4 p54 | `Freewheel::k2` | OK |
| COD-056 | PWMCONF.freewheel = %11: coil short, high side | S6.5.4 p54 | `Freewheel::k3` | OK |
| COD-057 | PWMCONF.PWM_REG = 0: undefined (A17) | p54 | refused | GUARD |
| COD-058 | PWMCONF.PWM_REG = 1..15: 0.5 to 7.5 increments per fullstep | p54 | `StealthChop.reg` | OK |
| COD-059 | PWMCONF.PWM_LIM = 0..15: upper limit for the first PWM_SCALE_AUTO after switching back; default 12 | p54 | `StealthChop.lim` | OK |
| COD-060 | COOLCONF.SEUP = 0: current step 1 | S6.5.3 p53 | `CoolStep.seup` | OK |
| COD-061 | COOLCONF.SEUP = 1: current step 2 | S6.5.3 p53 | `CoolStep.seup` | OK |
| COD-062 | COOLCONF.SEUP = 2: current step 4 | S6.5.3 p53 | `CoolStep.seup` | OK |
| COD-063 | COOLCONF.SEUP = 3: current step 8 | S6.5.3 p53 | `CoolStep.seup` | OK |
| COD-064 | COOLCONF.SEDN = 0: 32 readings per decrement | S6.5.3 p53 | `CoolStep.sedn` | OK |
| COD-065 | COOLCONF.SEDN = 1: 8 readings per decrement | S6.5.3 p53 | `CoolStep.sedn` | OK |
| COD-066 | COOLCONF.SEDN = 2: 2 readings per decrement | S6.5.3 p53 | `CoolStep.sedn` | OK |
| COD-067 | COOLCONF.SEDN = 3: 1 readings per decrement | S6.5.3 p53 | `CoolStep.sedn` | OK |
| COD-068 | COOLCONF.SEIMIN = 0 / 1: minimum current 1/2 or 1/4 of IRUN | p53 | `CoolStep.min_quarter` | OK |
| COD-069 | COOLCONF.SEMIN = 0: CoolStep off | p53 | `CoolStep.semin` 0 | OK |
| COD-070 | COOLCONF.SEMIN = 1..15: lower threshold SEMIN*32; needs TCOOLTHRS >= 1 | p53, p92 | `CoolStep.semin` | GUARD |
| COD-071 | COOLCONF.SEMAX = 0..15: upper threshold (SEMIN+SEMAX+1)*32 | p53 | `CoolStep.semax` | OK |
| COD-072 | COOLCONF.SGT = -64..63: signed 7 bit; 0 start value | p53, p87 | `StallGuard.threshold` | OK |
| COD-073 | RAMPMODE = 0: position | S6.3.1 p40 | `RampMode` | OK |
| COD-074 | RAMPMODE = 1: velocity positive | S6.3.1 p40 | `RampMode` | OK |
| COD-075 | RAMPMODE = 2: velocity negative | S6.3.1 p40 | `RampMode` | OK |
| COD-076 | RAMPMODE = 3: hold | S6.3.1 p40 | `RampMode` | OK |
| COD-077 | RAMPMODE = 4..7: not defined; field is 2 bits | p40 | unreachable | OK |
| COD-078 | MSLUTSEL.Wx = %00: entry increments -1/+0 | S6.5.1 p50 | `MicrostepTable.w[x]` 0-3 | OK |
| COD-079 | MSLUTSEL.Wx = %01: entry increments +0/+1 | S6.5.1 p50 | `MicrostepTable.w[x]` 0-3 | OK |
| COD-080 | MSLUTSEL.Wx = %10: entry increments +1/+2 | S6.5.1 p50 | `MicrostepTable.w[x]` 0-3 | OK |
| COD-081 | MSLUTSEL.Wx = %11: entry increments +2/+3 | S6.5.1 p50 | `MicrostepTable.w[x]` 0-3 | OK |
| COD-082 | MSLUTSEL.X1,X2,X3 = 0 < X1 < X2 < X3: segment borders | p50 | `Validate` of `MicrostepTable` | GUARD |
| COD-083 | MSLUTSTART.START_SIN90 = 247 with default table: table value at entry 256 | p47,p106 | `MicrostepTable.start_sin90` | OK |
| COD-084 | NODECONF.SENDDELAY = 0: 8 bit times (not allowed with several nodes) | S6.1 p34 | `UartOptions.send_delay` | GUARD |
| COD-085 | NODECONF.SENDDELAY = 1: 8 bit times (not allowed with several nodes) | S6.1 p34 | `UartOptions.send_delay` | GUARD |
| COD-086 | NODECONF.SENDDELAY = 2, 3: 3 * 8 bit times | S6.1 p34 | `UartOptions.send_delay` | OK |
| COD-087 | NODECONF.SENDDELAY = 4, 5: 5 * 8 bit times | S6.1 p34 | `UartOptions.send_delay` | OK |
| COD-088 | NODECONF.SENDDELAY = 6, 7: 7 * 8 bit times | S6.1 p34 | `UartOptions.send_delay` | OK |
| COD-089 | NODECONF.SENDDELAY = 8, 9: 9 * 8 bit times | S6.1 p34 | `UartOptions.send_delay` | OK |
| COD-090 | NODECONF.SENDDELAY = 10, 11: 11 * 8 bit times | S6.1 p34 | `UartOptions.send_delay` | OK |
| COD-091 | NODECONF.SENDDELAY = 12, 13: 13 * 8 bit times | S6.1 p34 | `UartOptions.send_delay` | OK |
| COD-092 | NODECONF.SENDDELAY = 14, 15: 15 * 8 bit times | S6.1 p34 | `UartOptions.send_delay` | OK |
| COD-093 | NODECONF.NODEADDR = 0..253: node address; 254 only in the chain procedure; 255 reserved for master replies | p34, p27, p29 | `UartOptions::node` 0-253 (254 with `multi_node`, A31) | OK |
| COD-094 | ENCMODE.neg_edge/pos_edge = 00: N active during an active level | S6.4.1 p46 | `NEvent::pos_edge`, `neg_edge` | OK |
| COD-095 | ENCMODE.neg_edge/pos_edge = 01: active-going edge | S6.4.1 p46 | `NEvent::pos_edge`, `neg_edge` | OK |
| COD-096 | ENCMODE.neg_edge/pos_edge = 10: inactive-going edge | S6.4.1 p46 | `NEvent::pos_edge`, `neg_edge` | OK |
| COD-097 | ENCMODE.neg_edge/pos_edge = 11: both edges | S6.4.1 p46 | `NEvent::pos_edge`, `neg_edge` | OK |
| COD-098 | ENC_CONST = binary mode: value = integer.fraction/65536, signed | p45,p107 | `EncoderScale` (binary) | OK |
| COD-099 | ENC_CONST = decimal mode: fraction 0..9999 in units of 1/10000, negative by two's complement rule (A43) | p107-108 | `EncoderScale` (decimal) | OK |
| COD-100 | ENC_CONST = 1.0, -1.0, 25.6, -25.6, 51.2: examples from the datasheet | p107-108 | tests replay them | OK |
| COD-101 | ENC_DEVIATION = 0: function off | p45 | `Encoder.deviation` 0 | OK |
| COD-102 | TPWMTHRS/TCOOLTHRS/THIGH = 0: threshold disabled (see A15 for each meaning) | p39,p65 | `Thresholds` fields 0 | OK |
| COD-103 | TPWMTHRS >= TCOOLTHRS >= THIGH = ordering: required order of the three thresholds in TSTEP units | S10 p76 | `Validate` rule (A15) | GUARD |
| COD-104 | RampProfile.VMAX = 0..2^23-512: maximum 8388096 | p40 | `RampProfile::vmax` and the `Config::max_velocity_reg` ceiling | GUARD |
| COD-105 | RampProfile.VMAX = > 2^23-512: out of range | p40 | refused | GUARD |
| COD-106 | RampProfile.VSTART = 0..2^18-1: | p40 | `RampProfile.vstart` | OK |
| COD-107 | RampProfile.VSTOP = 0: not allowed in positioning mode | p41 | refused (A21) | GUARD |
| COD-108 | RampProfile.VSTOP = 1..9: allowed; advisory below 10, below 100 slow termination | p41,p82,p86 | documented advice (no API) | OK |
| COD-109 | RampProfile.VSTOP < VSTART = : short moves may not terminate; refused unless allowed (A21) | p40,p82 | `RampProfile` rule | GUARD |
| COD-110 | RampProfile.VMAX < VSTART = : positioning needs VMAX >= VSTART | p40 | refused in positioning | GUARD |
| COD-111 | RampProfile.D1 = 0: not allowed in positioning mode, even if V1 = 0 | p40 | refused | GUARD |
| COD-112 | RampProfile.V1 = 0: A1 and D1 phases off, AMAX and DMAX only | p40 | `RampProfile.v1` 0 (D1 still >= 1) | OK |
| COD-113 | RampProfile.AMAX = 0: velocity mode would not move | p40 | refused by `RunVelocity` | GUARD |
| COD-114 | RampProfile.A1, AMAX, DMAX, D1 = 0..65535: 16 bit | p40-41 | `RampProfile` | OK |
| COD-115 | RampProfile.V1 = 0..2^20-1: 20 bit | p40 | `RampProfile.v1` | OK |
| COD-116 | RampProfile.TZEROWAIT = 0..65535: units of 512 clocks | p41 | `RampProfile.zero_wait` | OK |
| COD-117 | RampProfile.XTARGET = -2^31..2^31-1: wraps; displacement at most 2^31-1 | p41 | `MoveTo`, `MoveBy` (overflow refused) | GUARD |
| COD-118 | XACTUAL write = any: starts a motion in positioning mode | p40 | `SetHomePosition` requires VACTUAL = 0, sets hold first | GUARD |
| COD-119 | GLOBALSCALER = 0 or 256: full scale | p36 | `CurrentPlan.global_scaler` = 256 | OK |
| COD-120 | GLOBALSCALER = 1..31: not allowed | p36 | unreachable | GUARD |
| COD-121 | GLOBALSCALER = 32..255: 32/256 ... 255/256; above 128 recommended | p36 | `CurrentPlan.global_scaler`; advisory below 128 | OK |
| COD-122 | IRUN / IHOLD = 0..31: 1/32 ... 32/32; IRUN below 8 does not work with automatic tuning | p38,p62 | `CurrentPlan`; rule with autoscale (A37) | GUARD |
| COD-123 | IHOLD = 0: with StealthChop selects freewheel or coil short by FREEWHEEL | p38,p66 | `StealthChop.freewheel` rule (A18) | OK |
| COD-124 | IHOLDDELAY = 0: instant power down | p38 | `Standstill.hold_delay` 0 | OK |
| COD-125 | IHOLDDELAY = 1..15: 1-15 * 2^18 clocks per step | p38 | `Standstill.hold_delay` | OK |
| COD-126 | TPOWERDOWN = 0..255: 2^18 clocks per count; at least 2 for automatic tuning | p38 | `Standstill.power_down_delay`; rule (A37) | GUARD |
| COD-127 | VDCMIN = 0: DcStep off | p42 | `DcStep.vdcmin` 0 | OK |
| COD-128 | VDCMIN = 1..255: below 256 not meaningful; refused (A27) | p42 | refused | GUARD |
| COD-129 | VDCMIN = 256..2^23-1: bits 22..8 used | p42,p101 | `DcStep.vdcmin`, rounded down to a multiple of 256 | OK |
| COD-130 | DCCTRL.DC_TIME = 0..1023: above tBLANK in clocks + 1..100 (A44) | p48,p101 | `DcStep.dc_time`; refused at or below tBLANK | GUARD |
| COD-131 | DCCTRL.DC_SG = 0..255: 0 disables; slightly above DC_TIME/16 | p48,p101 | `DcStep.dc_sg` | OK |
| COD-132 | Direct mode current A, B = -248..248: safe range with SpreadCycle; -255..255 in StealthChop voltage mode | p110 | `SetDirectCurrents` clamps and refuses beyond (A26) | GUARD |
| COD-133 | Microstep step width = 2^MRES: microsteps per step | p51 | `Chopper.mres` | OK |
| COD-134 | SW_MODE.en_softstop + sg_stop = both set: not allowed together | p43 | refused (A48) | GUARD |
| COD-135 | SW_MODE hard stop + StealthChop = : severe overcurrent possible; acknowledgement needed (A48) | p43 | `Ack::HardStopWithStealthChop` | GUARD |
| COD-136 | GCONF DIAG options in MotionController mode = bits 5,6,9,10,11: valid only with SD_MODE=1 | p32 | refused in MotionController mode (A25) | GUARD |
| COD-137 | GCONF diag1_steps_skipped = bit 11: not together with other DIAG1 options | p32 | exclusive in `Diag` | GUARD |
| COD-138 | CHOPCONF.diss2g / diss2vs = 1: protection off | p51,p22,p78 | `Ack::ShortProtectionOff` | GUARD |
| COD-139 | Clock.External.hz = 4e6..18e6: external clock range | p122 | `Clock::External` validated | GUARD |
| COD-140 | Board.rsense_mohm = 1..10000: sanity range; table covers 22-220 mOhm (A47) | p74 | `Board` validated | GUARD |
| COD-141 | Board.supply_mv = 10000..55000: operating VS | p120 | `Board` validated | GUARD |
| COD-142 | Motor.rated_rms_ma = 1..max for the sense resistor: declared limit; ceiling for every current | p74 | `Motor`, `PlanCurrent` | GUARD |
| COD-143 | Limits.max_velocity = declared: optional ceiling for VMAX | - | `Config::max_velocity_reg`, refused above | GUARD |
| COD-144 | SpiOptions.chain_length = 1..kMaxChain: chain size | p24 | `SpiOptions`, Kconfig `TMC5160_SPI_CHAIN_LENGTH` | GUARD |
