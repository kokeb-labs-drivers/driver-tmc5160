/**
 * @file tmc5160_status.hpp
 * @brief Decoded status registers of the TMC5160 and the pure decoders and encoders that build and
 * read register words.
 * @copyright Copyright (c) 2026 The driver-tmc5160 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Everything here is `constexpr` and free of I/O. The encoders turn a `Config` into the exact
 * register word the chip receives, with every reserved bit 0 (section 6, pages 31 to 56).
 */
#ifndef TMC5160_STATUS_HPP
#define TMC5160_STATUS_HPP

#include <array>
#include <cstdint>

#include "tmc5160_registers.hpp"
#include "tmc5160_types.hpp"
#include "tmc5160_units.hpp"

namespace tmc5160 {

/** @brief GSTAT, decoded (page 33). */
struct GlobalStatus {
  bool reset{false};   ///< The chip was reset since GSTAT was last cleared.
  bool drv_err{false}; ///< The driver shut down on overtemperature or a short.
  bool uv_cp{false};   ///< Charge pump undervoltage was seen.
};

/** @brief DRV_STATUS, decoded (page 56). */
struct DriverStatus {
  uint32_t sg_result{0};         ///< StallGuard2 result, 0 to 1023.
  uint32_t cs_actual{0};         ///< Actual current scale, 0 to 31.
  bool s2vs_a{false};            ///< Short to supply, phase A.
  bool s2vs_b{false};            ///< Short to supply, phase B.
  bool stealth{false};           ///< StealthChop is active.
  bool full_step{false};         ///< Full-step mode is active.
  bool stall{false};             ///< StallGuard2 reports a stall.
  bool over_temp{false};         ///< Overtemperature shutdown.
  bool over_temp_warning{false}; ///< Overtemperature pre-warning.
  bool s2g_a{false};             ///< Short to ground, phase A.
  bool s2g_b{false};             ///< Short to ground, phase B.
  bool open_load_a{false};       ///< Open load, phase A (informative).
  bool open_load_b{false};       ///< Open load, phase B (informative).
  bool standstill{false};        ///< The motor is at standstill.
  uint32_t raw{0};               ///< The register as read.
};

/** @brief RAMP_STAT, decoded (page 44). */
struct RampStatus {
  bool stop_left_active{false};  ///< The left switch is active.
  bool stop_right_active{false}; ///< The right switch is active.
  bool latch_left{false};        ///< A left latch is ready.
  bool latch_right{false};       ///< A right latch is ready.
  bool event_stop_left{false};   ///< A left stop event.
  bool event_stop_right{false};  ///< A right stop event.
  bool event_stop_stall{false};  ///< A stall stop event.
  bool event_pos_reached{false}; ///< The target position was reached.
  bool velocity_reached{false};  ///< VACTUAL equals VMAX.
  bool position_reached{false};  ///< XACTUAL equals XTARGET.
  bool velocity_zero{false};     ///< VACTUAL is zero.
  bool zero_wait_active{false};  ///< TZEROWAIT is running.
  bool second_move{false};       ///< The ramp reversed direction.
  bool stall{false};             ///< The stall input is active.
  uint32_t raw{0};               ///< The register as read.
};

/** @brief ENC_STATUS, decoded (page 45). */
struct EncoderStatus {
  bool n_event{false};        ///< An N event was detected.
  bool deviation_warn{false}; ///< The encoder deviates from XACTUAL.
};

/** @brief IOIN, decoded (page 34). */
struct InputPins {
  bool refl_step{false}; ///< Level of REFL_STEP.
  bool refr_dir{false};  ///< Level of REFR_DIR.
  bool encb_dcen{false}; ///< Level of ENCB_DCEN_CFG4.
  bool enca_dcin{false}; ///< Level of ENCA_DCIN_CFG5.
  bool drv_enn{false};   ///< Level of DRV_ENN.
  bool encn_dco{false};  ///< Level of ENCN_DCO_CFG6.
  bool sd_mode{false};   ///< Level of the SD_MODE strap.
  bool swcomp_in{false}; ///< Level of the single-wire comparator input.
  uint32_t version{0};   ///< VERSION, bits 31 to 24.
};

/** @brief OFFSET_READ, decoded (page 36). */
struct Offsets {
  int32_t phase_a{0}; ///< Offset calibration of phase A, signed.
  int32_t phase_b{0}; ///< Offset calibration of phase B, signed.
};

/** @brief OTP_READ, decoded (page 35, 37). */
struct OtpImage {
  uint32_t clock_trim{0};  ///< FCLKTRIM, 5 bits.
  bool s2_level_12{false}; ///< Short levels default to 12 instead of 6.
  bool bbm_clks_2{false};  ///< BBMCLKS defaults to 2 instead of 4.
  bool tbl_01{false};      ///< TBL defaults to %01 instead of %10.
  uint32_t raw{0};         ///< The register as read.
};

/** @brief PWM_SCALE and PWM_AUTO, decoded (page 48). */
struct StealthChopState {
  uint32_t scale_sum{0}; ///< PWM_SCALE_SUM, the actual duty, 0 to 255.
  int32_t scale_auto{0}; ///< PWM_SCALE_AUTO, signed 9 bits; near 0 after a successful tuning.
  uint32_t ofs_auto{0};  ///< PWM_OFS_AUTO.
  uint32_t grad_auto{0}; ///< PWM_GRAD_AUTO.
};

/** @brief MSCURACT, decoded (page 47): the unscaled microstep currents, -255 to 255. */
struct MicrostepCurrents {
  int32_t cur_a{0}; ///< Phase A (bits 24 to 16).
  int32_t cur_b{0}; ///< Phase B (bits 8 to 0).
};

/** @brief What `Service()` found and cleared. */
struct Events {
  GlobalStatus global{};   ///< GSTAT at the time of the call.
  RampStatus ramp{};       ///< RAMP_STAT at the time of the call.
  EncoderStatus encoder{}; ///< ENC_STATUS at the time of the call (zero in StepDir mode).
  DriverStatus driver{};   ///< DRV_STATUS at the time of the call.
};

/** @brief One pipelined batch of the values a monitor wants together. */
struct Snapshot {
  GlobalStatus global{}; ///< GSTAT.
  DriverStatus driver{}; ///< DRV_STATUS.
  RampStatus ramp{};     ///< RAMP_STAT.
  int32_t position{0};   ///< XACTUAL.
  int32_t velocity{0};   ///< VACTUAL, signed.
  uint32_t step_time{0}; ///< TSTEP.
  uint32_t mscnt{0};     ///< MSCNT.
};

/** @brief What start-up learned about the chip. */
struct BeginReport {
  uint32_t version{0};         ///< IOIN.VERSION.
  bool sd_mode{false};         ///< IOIN.SD_MODE: the strap level.
  bool uv_cp_seen{false};      ///< GSTAT showed a charge pump undervoltage at start-up.
  bool stall_latched{false};   ///< RAMP_STAT held a stall stop from the last session (not cleared).
  uint32_t advisories{0};      ///< `Advise()` of the configuration.
  uint32_t current_run_ma{0};  ///< The nominal run current the registers give.
  uint32_t current_hold_ma{0}; ///< The nominal hold current the registers give.
};

namespace detail {

/// Sign-extend the low `bits` bits of `v`.
[[nodiscard]] constexpr int32_t SignExtend(uint32_t v, uint32_t bits) noexcept {
  const uint32_t sign = uint32_t{1} << (bits - 1U);
  const uint32_t low = v & ((sign << 1U) - 1U);
  return static_cast<int32_t>((low ^ sign)) - static_cast<int32_t>(sign);
}

/// Bit `b` of `v` as a bool.
[[nodiscard]] constexpr bool Bit(uint32_t v, uint32_t b) noexcept {
  return ((v >> b) & 1U) != 0U;
}

/// `1 << pos` when `on`, else 0, without a branch.
[[nodiscard]] constexpr uint32_t Put(bool on, uint32_t pos) noexcept {
  return static_cast<uint32_t>(on) << pos;
}

[[nodiscard]] constexpr GlobalStatus DecodeGlobal(uint32_t raw) noexcept {
  return GlobalStatus{.reset = Bit(raw, 0), .drv_err = Bit(raw, 1), .uv_cp = Bit(raw, 2)};
}

[[nodiscard]] constexpr DriverStatus DecodeDriver(uint32_t raw) noexcept {
  DriverStatus d{};
  d.sg_result = raw & 0x3FFU;
  d.cs_actual = (raw >> 16U) & 0x1FU;
  d.s2vs_a = Bit(raw, 12);
  d.s2vs_b = Bit(raw, 13);
  d.stealth = Bit(raw, 14);
  d.full_step = Bit(raw, 15);
  d.stall = Bit(raw, 24);
  d.over_temp = Bit(raw, 25);
  d.over_temp_warning = Bit(raw, 26);
  d.s2g_a = Bit(raw, 27);
  d.s2g_b = Bit(raw, 28);
  d.open_load_a = Bit(raw, 29);
  d.open_load_b = Bit(raw, 30);
  d.standstill = Bit(raw, 31);
  d.raw = raw;
  return d;
}

[[nodiscard]] constexpr RampStatus DecodeRamp(uint32_t raw) noexcept {
  RampStatus r{};
  r.stop_left_active = Bit(raw, 0);
  r.stop_right_active = Bit(raw, 1);
  r.latch_left = Bit(raw, 2);
  r.latch_right = Bit(raw, 3);
  r.event_stop_left = Bit(raw, 4);
  r.event_stop_right = Bit(raw, 5);
  r.event_stop_stall = Bit(raw, 6);
  r.event_pos_reached = Bit(raw, 7);
  r.velocity_reached = Bit(raw, 8);
  r.position_reached = Bit(raw, 9);
  r.velocity_zero = Bit(raw, 10);
  r.zero_wait_active = Bit(raw, 11);
  r.second_move = Bit(raw, 12);
  r.stall = Bit(raw, 13);
  r.raw = raw;
  return r;
}

[[nodiscard]] constexpr InputPins DecodeInputs(uint32_t raw) noexcept {
  InputPins p{};
  p.refl_step = Bit(raw, 0);
  p.refr_dir = Bit(raw, 1);
  p.encb_dcen = Bit(raw, 2);
  p.enca_dcin = Bit(raw, 3);
  p.drv_enn = Bit(raw, 4);
  p.encn_dco = Bit(raw, 5);
  p.sd_mode = Bit(raw, 6);
  p.swcomp_in = Bit(raw, 7);
  p.version = raw >> 24U;
  return p;
}

[[nodiscard]] constexpr OtpImage DecodeOtp(uint32_t raw) noexcept {
  return OtpImage{.clock_trim = raw & 0x1FU,
                  .s2_level_12 = Bit(raw, 5),
                  .bbm_clks_2 = Bit(raw, 6),
                  .tbl_01 = Bit(raw, 7),
                  .raw = raw & 0xFFU};
}

[[nodiscard]] constexpr StealthChopState DecodeStealthChop(uint32_t scale,
                                                           uint32_t autos) noexcept {
  return StealthChopState{.scale_sum = scale & 0xFFU,
                          .scale_auto = SignExtend(scale >> 16U, 9),
                          .ofs_auto = autos & 0xFFU,
                          .grad_auto = (autos >> 16U) & 0xFFU};
}

[[nodiscard]] constexpr MicrostepCurrents DecodeMscuract(uint32_t raw) noexcept {
  return MicrostepCurrents{.cur_a = SignExtend(raw >> 16U, 9), .cur_b = SignExtend(raw, 9)};
}

/// GCONF from the configuration and the direct-mode state (page 32).
[[nodiscard]] constexpr uint32_t GconfWord(const Config& c, bool direct) noexcept {
  const Diag& d = c.diag;
  uint32_t w = Put(c.flags.recalibrate, 0) | Put(c.standstill.fast_standstill, 1) |
               Put(c.stealth.enable, 2) | Put(c.flags.multistep_filt, 3) |
               Put(c.flags.invert_direction, 4);
  w |= Put(d.diag0_error, 5) | Put(d.diag0_otpw, 6) | Put(d.diag0_stall || d.diag0_step_output, 7);
  w |= Put(d.diag1_stall || d.diag1_dir_output, 8) | Put(d.diag1_index, 9) |
       Put(d.diag1_on_state, 10);
  w |= Put(d.diag1_steps_skipped, 11) | Put(d.diag0_push_pull, 12) | Put(d.diag1_push_pull, 13);
  w |= Put(c.thresholds.small_hysteresis, 14) | Put(c.flags.stop_input, 15) | Put(direct, 16);
  return w;
}

/// CHOPCONF from the configuration; `toff` is the TOFF value to write (0 = driver off).
[[nodiscard]] constexpr uint32_t ChopconfWord(const Config& c, uint32_t toff) noexcept {
  const Chopper& k = c.chopper;
  const bool const_off = k.mode == ChopperMode::ConstOffTime;
  const uint32_t low = const_off ? (k.tfd & 7U) : k.hstrt;
  const uint32_t high = const_off ? k.offset : k.hend;
  uint32_t w = toff & 15U;
  w |= low << 4U;
  w |= high << 7U;
  w |= Put(const_off && ((k.tfd & 8U) != 0U), 11) | Put(const_off && k.no_comparator, 12);
  w |= Put(const_off, 14) | (static_cast<uint32_t>(k.tbl & 3U) << 15U);
  w |= Put(k.high_fullstep, 18) | Put(k.high_const_off, 19) |
       (static_cast<uint32_t>(k.tpfd & 15U) << 20U);
  w |= (static_cast<uint32_t>(k.mres) << 24U) | Put(k.interpolate, 28) | Put(k.double_edge, 29);
  w |= Put(c.protection.disable_s2g, 30) | Put(c.protection.disable_s2vs, 31);
  return w;
}

/// IHOLD_IRUN from a plan and the standstill settings (page 38).
[[nodiscard]] constexpr uint32_t IholdIrunWord(const units::CurrentPlan& p, const Standstill& s,
                                               uint32_t ihold) noexcept {
  return (ihold & 31U) | ((p.irun & 31U) << 8U) |
         (static_cast<uint32_t>(s.hold_delay & 15U) << 16U);
}

/// PWMCONF (page 54).
[[nodiscard]] constexpr uint32_t PwmconfWord(const StealthChop& s) noexcept {
  return static_cast<uint32_t>(s.ofs) | (static_cast<uint32_t>(s.grad) << 8U) |
         (static_cast<uint32_t>(s.freq & 3U) << 16U) | Put(s.autoscale, 18) | Put(s.autograd, 19) |
         (static_cast<uint32_t>(s.freewheel) << 20U) | (static_cast<uint32_t>(s.reg & 15U) << 24U) |
         (static_cast<uint32_t>(s.lim & 15U) << 28U);
}

/// COOLCONF (page 53).
[[nodiscard]] constexpr uint32_t CoolconfWord(const CoolStep& k, const StallGuard& g) noexcept {
  return static_cast<uint32_t>(k.semin & 15U) | (static_cast<uint32_t>(k.seup & 3U) << 5U) |
         (static_cast<uint32_t>(k.semax & 15U) << 8U) |
         (static_cast<uint32_t>(k.sedn & 3U) << 13U) | Put(k.min_quarter, 15) |
         ((static_cast<uint32_t>(static_cast<int32_t>(g.threshold)) & 0x7FU) << 16U) |
         Put(g.filter, 24);
}

/// DCCTRL (page 48).
[[nodiscard]] constexpr uint32_t DcctrlWord(const DcStep& d) noexcept {
  return static_cast<uint32_t>(d.dc_time & 0x3FFU) | (static_cast<uint32_t>(d.dc_sg) << 16U);
}

/// SHORT_CONF (page 35).
[[nodiscard]] constexpr uint32_t ShortconfWord(const Protection& p) noexcept {
  return static_cast<uint32_t>(p.s2vs_level & 15U) |
         (static_cast<uint32_t>(p.s2g_level & 15U) << 8U) |
         (static_cast<uint32_t>(p.filter & 3U) << 16U) | Put(p.long_delay, 18);
}

/// DRV_CONF (page 35).
[[nodiscard]] constexpr uint32_t DrvconfWord(const Bridge& b) noexcept {
  return static_cast<uint32_t>(b.bbm_time & 31U) | (static_cast<uint32_t>(b.bbm_clks & 15U) << 8U) |
         (static_cast<uint32_t>(b.ot_level) << 16U) |
         (static_cast<uint32_t>(b.drive_strength & 3U) << 18U) |
         (static_cast<uint32_t>(b.sense_filter & 3U) << 20U);
}

/// SW_MODE (page 43).
[[nodiscard]] constexpr uint32_t SwmodeWord(const Switches& s) noexcept {
  return Put(s.stop_left, 0) | Put(s.stop_right, 1) | Put(s.left_active_low, 2) |
         Put(s.right_active_low, 3) | Put(s.swap, 4) | Put(s.latch_left_active, 5) |
         Put(s.latch_left_inactive, 6) | Put(s.latch_right_active, 7) |
         Put(s.latch_right_inactive, 8) | Put(s.latch_encoder, 9) | Put(s.stop_on_stall, 10) |
         Put(s.soft_stop, 11);
}

/// ENCMODE (page 46).
[[nodiscard]] constexpr uint32_t EncmodeWord(const Encoder& e) noexcept {
  const NEvent& n = e.n_event;
  return Put(n.pol_a, 0) | Put(n.pol_b, 1) | Put(n.pol_n, 2) | Put(n.ignore_ab, 3) |
         Put(n.repeat, 4) | Put(n.once, 5) | Put(n.pos_edge, 6) | Put(n.neg_edge, 7) |
         Put(n.clear_counter, 8) | Put(n.latch_xactual, 9) | Put(e.scale.decimal, 10);
}

/// MSLUTSEL (page 50).
[[nodiscard]] constexpr uint32_t MslutselWord(const MicrostepTable& t) noexcept {
  return static_cast<uint32_t>(t.w[0] & 3U) | (static_cast<uint32_t>(t.w[1] & 3U) << 2U) |
         (static_cast<uint32_t>(t.w[2] & 3U) << 4U) | (static_cast<uint32_t>(t.w[3] & 3U) << 6U) |
         (static_cast<uint32_t>(t.x1) << 8U) | (static_cast<uint32_t>(t.x2) << 16U) |
         (static_cast<uint32_t>(t.x3) << 24U);
}

/// MSLUTSTART (page 47).
[[nodiscard]] constexpr uint32_t MslutstartWord(const MicrostepTable& t) noexcept {
  return static_cast<uint32_t>(t.start_sin) | (static_cast<uint32_t>(t.start_sin90) << 16U);
}

/// NODECONF (page 34).
[[nodiscard]] constexpr uint32_t NodeconfWord(uint32_t node, uint32_t send_delay) noexcept {
  return (node & 0xFFU) | ((send_delay & 15U) << 8U);
}

/// What the word encoders need besides the configuration.
struct WordContext {
  const Config& config;           ///< The configuration to encode.
  const units::CurrentPlan& plan; ///< The current plan.
  bool direct;                    ///< Direct mode is on (GCONF bit 16).
  bool enabled;                   ///< The driver is enabled (CHOPCONF TOFF as configured).
  bool hold_at_run;               ///< IHOLD is written equal to IRUN (StealthChop tuning).
};

/// An encoder of one register word.
using WordFn = uint32_t (*)(const WordContext&) noexcept;

/// One row of the encoder table.
struct WordEntry {
  uint8_t address; ///< Register address.
  WordFn encode;   ///< Builds the word from the context.
};

/// The encoder of every register the driver writes from a `Config`, the single place a word is
/// built.
inline constexpr std::array<WordEntry, 38> kWordTable{{
    {.address = reg::kGconf,
     .encode = [](const WordContext& x) noexcept { return GconfWord(x.config, x.direct); }},
    {.address = reg::kShortConf,
     .encode = [](const WordContext& x) noexcept { return ShortconfWord(x.config.protection); }},
    {.address = reg::kDrvConf,
     .encode = [](const WordContext& x) noexcept { return DrvconfWord(x.config.bridge); }},
    {.address = reg::kGlobalScaler,
     .encode = [](const WordContext& x) noexcept { return x.plan.global_scaler_reg; }},
    {.address = reg::kIholdIrun,
     .encode =
         [](const WordContext& x) noexcept {
           return IholdIrunWord(x.plan, x.config.standstill,
                                x.hold_at_run ? x.plan.irun : x.plan.ihold);
         }},
    {.address = reg::kTpowerdown,
     .encode =
         [](const WordContext& x) noexcept {
           return uint32_t{x.config.standstill.power_down_delay};
         }},
    {.address = reg::kTpwmthrs,
     .encode = [](const WordContext& x) noexcept { return x.config.thresholds.pwm; }},
    {.address = reg::kTcoolthrs,
     .encode = [](const WordContext& x) noexcept { return x.config.thresholds.cool; }},
    {.address = reg::kThigh,
     .encode = [](const WordContext& x) noexcept { return x.config.thresholds.high; }},
    {.address = reg::kVstart,
     .encode = [](const WordContext& x) noexcept { return x.config.ramp.vstart; }},
    {.address = reg::kA1, .encode = [](const WordContext& x) noexcept { return x.config.ramp.a1; }},
    {.address = reg::kV1, .encode = [](const WordContext& x) noexcept { return x.config.ramp.v1; }},
    {.address = reg::kAmax,
     .encode = [](const WordContext& x) noexcept { return x.config.ramp.amax; }},
    {.address = reg::kVmax,
     .encode = [](const WordContext& x) noexcept { return x.config.ramp.vmax; }},
    {.address = reg::kDmax,
     .encode = [](const WordContext& x) noexcept { return x.config.ramp.dmax; }},
    {.address = reg::kD1, .encode = [](const WordContext& x) noexcept { return x.config.ramp.d1; }},
    {.address = reg::kVstop,
     .encode = [](const WordContext& x) noexcept { return x.config.ramp.vstop; }},
    {.address = reg::kTzerowait,
     .encode = [](const WordContext& x) noexcept { return x.config.ramp.zero_wait; }},
    {.address = reg::kVdcmin,
     .encode = [](const WordContext& x) noexcept { return x.config.dcstep.vdcmin; }},
    {.address = reg::kSwMode,
     .encode = [](const WordContext& x) noexcept { return SwmodeWord(x.config.switches); }},
    {.address = reg::kEncmode,
     .encode = [](const WordContext& x) noexcept { return EncmodeWord(x.config.encoder); }},
    {.address = reg::kEncConst,
     .encode =
         [](const WordContext& x) noexcept { return units::EncConstWord(x.config.encoder.scale); }},
    {.address = reg::kEncDeviation,
     .encode = [](const WordContext& x) noexcept { return x.config.encoder.deviation; }},
    {.address = 0x60U,
     .encode = [](const WordContext& x) noexcept { return x.config.table.words[0]; }},
    {.address = 0x61U,
     .encode = [](const WordContext& x) noexcept { return x.config.table.words[1]; }},
    {.address = 0x62U,
     .encode = [](const WordContext& x) noexcept { return x.config.table.words[2]; }},
    {.address = 0x63U,
     .encode = [](const WordContext& x) noexcept { return x.config.table.words[3]; }},
    {.address = 0x64U,
     .encode = [](const WordContext& x) noexcept { return x.config.table.words[4]; }},
    {.address = 0x65U,
     .encode = [](const WordContext& x) noexcept { return x.config.table.words[5]; }},
    {.address = 0x66U,
     .encode = [](const WordContext& x) noexcept { return x.config.table.words[6]; }},
    {.address = 0x67U,
     .encode = [](const WordContext& x) noexcept { return x.config.table.words[7]; }},
    {.address = reg::kMslutsel,
     .encode = [](const WordContext& x) noexcept { return MslutselWord(x.config.table); }},
    {.address = reg::kMslutstart,
     .encode = [](const WordContext& x) noexcept { return MslutstartWord(x.config.table); }},
    {.address = reg::kChopconf,
     .encode =
         [](const WordContext& x) noexcept {
           return ChopconfWord(x.config, x.enabled ? x.config.chopper.toff : 0U);
         }},
    {.address = reg::kCoolconf,
     .encode =
         [](const WordContext& x) noexcept { return CoolconfWord(x.config.cool, x.config.stall); }},
    {.address = reg::kDcctrl,
     .encode = [](const WordContext& x) noexcept { return DcctrlWord(x.config.dcstep); }},
    {.address = reg::kPwmconf,
     .encode = [](const WordContext& x) noexcept { return PwmconfWord(x.config.stealth); }},
    {.address = reg::kNodeconf,
     .encode =
         [](const WordContext& x) noexcept {
           return NodeconfWord(x.config.uart.node, x.config.uart.send_delay);
         }},
}};

/**
 * @brief The register word for `address` from a context; 0 when the address has no encoder.
 * @param x The context.
 * @param address The register.
 * @return The word, with every reserved bit 0.
 */
[[nodiscard]] constexpr uint32_t WordFor(const WordContext& x, uint8_t address) noexcept {
  for (const WordEntry& e : kWordTable) {
    if (e.address == address) {
      return e.encode(x);
    }
  }
  return 0U;
}

// The encoders reproduce the reset values and the datasheet's examples (pages 32, 48, 116).
static_assert(PwmconfWord(StealthChop{}) == reg::kPwmconfReset);
static_assert(ChopconfWord(Config{}, 0U) == reg::kChopconfReset);
static_assert(MslutselWord(MicrostepTable{}) == reg::kDefaultMslutsel);
static_assert(MslutstartWord(MicrostepTable{}) == reg::kDefaultMslutstart);
static_assert((SignExtend(0x1FFU, 9) == -1) && (SignExtend(0x0FFU, 9) == 255));
static_assert((DecodeMscuract(0x01FF0001U).cur_a == -1) &&
              (DecodeMscuract(0x01FF0001U).cur_b == 1));

} // namespace detail

} // namespace tmc5160

#endif // TMC5160_STATUS_HPP
