/**
 * @file tmc5160_types.hpp
 * @brief Public types of the TMC5160 driver: errors, `Result<T>`, configuration, validation and
 * advice.
 * @copyright Copyright (c) 2026 The driver-tmc5160 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Everything here is plain data with no platform dependency, so handlers, tests and tools can
 * include it without the driver template. The configuration defaults are the chip's reset values
 * (TMC5160 datasheet, chapter 6). `Config{}` is deliberately not valid: the motor and the power
 * stage must be declared, because the coil current follows from the sense resistor and the declared
 * motor limits.
 */
#ifndef TMC5160_TYPES_HPP
#define TMC5160_TYPES_HPP

#include <array>
#include <cstdint>

#include "tmc5160_detail.hpp"
#include "tmc5160_registers.hpp"
#include "tmc5160_version.h"

/** @brief The TMC5160 driver: everything it declares lives here; internals in `detail`. */
namespace tmc5160 {

/**
 * @defgroup tmc5160_types Driver types
 * @brief Errors, results, configuration and presets.
 * @{
 */

/**
 * @brief Why a call failed. Every call of the driver returns one of these in its `Result`.
 *
 * Values 0 to 9 are the shared base set; chip-specific values start at 16. `[[nodiscard]]` on
 * the type: a function that returns a bare `Error` cannot have it ignored either (KB-CPP20-037).
 */
enum class [[nodiscard]] Error : uint8_t {
  // Base set: the same names and values in every driver of the org (standards/driver.md).
  Ok = 0,               ///< The call succeeded.
  NotInitialized = 1,   ///< `EnsureInitialized()` has not succeeded, or `Deinitialize()` ran since.
  InvalidArgument = 2,  ///< An argument or a `Config` field is out of range; nothing was sent.
  Bus = 3,              ///< A port hook returned `false`.
  Timeout = 4,          ///< The chip did not answer in the allowed time or a bounded wait ran out.
  DeviceIdMismatch = 5, ///< IOIN.VERSION is not 0x30: another chip, no chip or a wiring fault.
  Crc = 6,              ///< The CRC-8 of a UART reply did not match (never produced on SPI).
  Busy = 7,             ///< The motor is moving or a ramp is running: the call needs standstill.
  DeviceFault = 8,      ///< The driver reports an error (GSTAT.drv_err): short or overtemperature.
  Unsupported = 9,      ///< The hook, bus or mode this call needs is absent.
  // Chip-specific values start at 16; each one is returned by some call.
  UnexpectedReset =
      16, ///< The chip reset (GSTAT.reset, SPI status bit 0): every write-only register is lost.
  VerifyFailed = 17,  ///< A write-back, echo or interface-counter check did not match.
  Refused = 18,       ///< A guard refused the call: a missing acknowledgement or an unsafe order.
  ModeMismatch = 19,  ///< The SD_MODE strap or the bus does not match `Config::mode`.
  LimitExceeded = 20, ///< The request exceeds a declared motor, board or ramp limit.
};

/**
 * @brief The name of an error, for logs.
 * @param error The error.
 * @return A static string such as `"Timeout"`; `"Unknown"` for a value that is not an enumerator.
 */
constexpr const char* ToString(Error error) noexcept {
  switch (error) {
  case Error::Ok:
    return "Ok";
  case Error::NotInitialized:
    return "NotInitialized";
  case Error::InvalidArgument:
    return "InvalidArgument";
  case Error::Bus:
    return "Bus";
  case Error::Timeout:
    return "Timeout";
  case Error::DeviceIdMismatch:
    return "DeviceIdMismatch";
  case Error::Crc:
    return "Crc";
  case Error::Busy:
    return "Busy";
  case Error::DeviceFault:
    return "DeviceFault";
  case Error::Unsupported:
    return "Unsupported";
  case Error::UnexpectedReset:
    return "UnexpectedReset";
  case Error::VerifyFailed:
    return "VerifyFailed";
  case Error::Refused:
    return "Refused";
  case Error::ModeMismatch:
    return "ModeMismatch";
  case Error::LimitExceeded:
    return "LimitExceeded";
  default:
    break;
  }
  return "Unknown";
}

/**
 * @brief The value of a call, or why there is none.
 *
 * Check `ok()` (or the `bool` conversion) before reading `value`; on failure `value` is `T{}`.
 *
 * @tparam T The value type. `Result<void>` carries only the error.
 */
template <typename T>
struct [[nodiscard]] Result {
  T value{};              ///< The value; `T{}` when the call failed.
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }

  /**
   * @brief A successful result.
   * @param v The value.
   * @return `{v, Error::Ok}`.
   */
  static constexpr Result Success(T v) noexcept {
    return {v, Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{T{}, e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {T{}, e};
  }
};

/** @brief The result of a call that returns no value. */
template <>
struct [[nodiscard]] Result<void> {
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }

  /** @return A successful result. */
  static constexpr Result Success() noexcept {
    return {Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {e};
  }
};

/** @brief Severity passed to the optional `Log()` hook. */
enum class LogLevel : uint8_t {
  Error = 0, ///< A call failed.
  Warning,   ///< Something unexpected that the driver recovered from.
  Info,      ///< Lifecycle events: initialised, reset.
  Debug,     ///< Detail for bring-up.
};

/** @brief Output pins the port may drive through the optional `GpioSet()` hook. */
enum class CtrlPin : uint8_t {
  DrvEnn = 0,   ///< DRV_ENN, active low on the package pin: ACTIVE means the driver is enabled.
  DcEnable = 1, ///< ENCB_DCEN_CFG4: DcStep enable input (STEP/DIR mode).
  DcIn = 2,     ///< ENCA_DCIN_CFG5: DcStep input or emergency stop input.
};

/** @brief Input pins the port may read through the optional `GpioGet()` hook. */
enum class InputPin : uint8_t {
  Diag0 = 0, ///< DIAG0_SWN: interrupt or error output.
  Diag1 = 1, ///< DIAG1_SWP: position compare or stall output.
  DcOut = 2, ///< ENCN_DCO_CFG6: DcStep ready output.
};

/** @brief Logical level of a control pin; the port maps it to the physical polarity. */
enum class GpioSignal : uint8_t {
  INACTIVE = 0, ///< The pin function is deasserted.
  ACTIVE = 1,   ///< The pin function is asserted.
};

/** @brief Result of one UART exchange as the port reports it. */
enum class BusStatus : uint8_t {
  Ok = 0,      ///< All bytes were sent and all expected bytes received.
  Fault = 1,   ///< The line or the peripheral failed.
  Timeout = 2, ///< The reply did not arrive in time.
};

/** @brief Where the chip's step pulses come from (SD_MODE strap, section 2.2 page 13). */
enum class Mode : uint8_t {
  MotionController = 0, ///< SD_MODE low: the internal ramp generator moves the motor. Default.
  StepDir = 1,          ///< SD_MODE high: STEP and DIR pins; serial configuration, no ramp.
};

/** @brief Extra traffic a setter spends to prove a write (handbook KB-ARCH-041). */
enum class Verify : uint8_t {
  None = 0,     ///< No extra traffic.
  Echo = 1,     ///< SPI: one more transfer; its reply must mirror the data just written (page 24).
  ReadBack = 2, ///< Read the register back where the chip allows it (one or two transfers).
  Counter = 3,  ///< UART: one IFCNT read; it must have counted this write (page 33).
};

/** @brief Chopper mode (CHOPCONF.chm). */
enum class ChopperMode : uint8_t {
  SpreadCycle = 0,  ///< Hysteresis decay, page 51.
  ConstOffTime = 1, ///< Classic constant off time.
};

/** @brief Microstep resolution (CHOPCONF.MRES): the value is the shift from 256 microsteps. */
enum class Microsteps : uint8_t {
  M256 = 0, ///< 256 microsteps per full step.
  M128 = 1, ///< 128.
  M64 = 2,  ///< 64.
  M32 = 3,  ///< 32.
  M16 = 4,  ///< 16.
  M8 = 5,   ///< 8.
  M4 = 6,   ///< 4.
  M2 = 7,   ///< 2.
  Full = 8, ///< Full steps.
};

/** @brief Overtemperature shutdown level (DRV_CONF.OTSELECT, page 36). */
enum class OtLevel : uint8_t {
  C150 = 0, ///< 150 degrees Celsius.
  C143 = 1, ///< 143 degrees Celsius.
  C136 = 2, ///< 136 degrees Celsius.
  C120 = 3, ///< 120 degrees Celsius.
};

/** @brief Standstill mode with IHOLD = 0 (PWMCONF.freewheel, page 38). */
enum class Freewheel : uint8_t {
  Normal = 0,        ///< Normal operation.
  Freewheel = 1,     ///< Both bridges floating.
  ShortLowSide = 2,  ///< Coils shorted through the low-side MOSFETs.
  ShortHighSide = 3, ///< Coils shorted through the high-side MOSFETs.
};

/** @brief Ramp generator mode (RAMPMODE, page 40). */
enum class RampMode : uint8_t {
  Position = 0,         ///< Move to XTARGET with the six-point ramp.
  VelocityPositive = 1, ///< Run at VMAX in the positive direction.
  VelocityNegative = 2, ///< Run at VMAX in the negative direction.
  Hold = 3,             ///< Velocity stays constant; used as the safe state.
};

/** @brief How `Stop()` brings the motor to rest (section 6.3.2, page 43). */
enum class StopKind : uint8_t {
  Ramp = 0,       ///< Velocity mode with VMAX 0: decelerate with AMAX (page 83).
  Immediate = 1,  ///< VSTART 0 and VMAX 0 in velocity mode: still the AMAX ramp (no VSTART floor).
  HoldTarget = 2, ///< XTARGET = XACTUAL, then position mode: decelerates with DMAX/D1 past that
                  ///< position and, after TZEROWAIT, runs back to it (12.2.4 c, page 83).
  Emergency = 3,  ///< Driver off (`DRV_ENN`, TOFF 0): the motor is not held.
};

/**
 * @brief Acknowledgements: each lets one call do what the datasheet warns against.
 *
 * A call that needs one without it returns `Error::Refused` and writes nothing.
 * docs/configuration.md lists each with its reason.
 */
enum class Ack : uint8_t {
  HoldAboveRun = 1U << 0,            ///< IHOLD above IRUN (page 38).
  ShortProtectionOff = 1U << 1,      ///< `diss2g` or `diss2vs` set (page 22).
  LowOtLevel = 1U << 2,              ///< 120 degrees shutdown, or 136 degrees above 24 V (page 36).
  HardStopWithStealthChop = 1U << 3, ///< Hard stop in StealthChop: overcurrent risk (page 43).
  VstartAboveVstop = 1U << 4,        ///< VSTOP below VSTART (page 41).
  DirectMode = 1U << 5,              ///< Direct mode: coil currents from XTARGET (page 110).
  VoltageModeNoCurrentLimit = 1U << 6, ///< Direct mode with a voltage-mode chopper (page 110).
};

/** @brief A set of acknowledgements. */
struct AckSet {
  uint32_t bits{0}; ///< The `Ack` values or-ed together.

  /**
   * @brief Whether `a` is in the set.
   * @param a The acknowledgement.
   * @return `true` when present.
   */
  [[nodiscard]] constexpr bool Has(Ack a) const noexcept {
    return (bits & static_cast<uint32_t>(a)) != 0U;
  }
  /**
   * @brief A copy with `a` added.
   * @param a The acknowledgement.
   * @return The new set.
   */
  [[nodiscard]] constexpr AckSet With(Ack a) const noexcept {
    return AckSet{bits | static_cast<uint32_t>(a)};
  }
};

/** @brief The motor as its data sheet declares it; the current ceiling comes from here. */
struct Motor {
  uint32_t rated_rms_ma{0}; ///< Rated coil current, RMS, in mA. Required, 1 or more.
  uint32_t max_rms_ma{0};   ///< Hard ceiling in mA; 0 means `rated_rms_ma`.
  uint32_t coil_resistance_mohm{
      0}; ///< Coil resistance in mOhm; 0 = unknown (StealthChop lower limit).
  uint32_t fullsteps_per_rev{200}; ///< Full steps per revolution, 1 or more.
};

/** @brief The power stage as built: the sense resistor and the supply. */
struct Board {
  uint32_t rsense_mohm{0};   ///< Sense resistor per coil in mOhm. Required, 1 or more.
  uint32_t supply_mv{0};     ///< Motor supply VS in mV, 10000 to 55000 (section 28.1, page 120).
  uint32_t supply_max_mv{0}; ///< Highest supply that can occur in mV; 0 means `supply_mv`.
};

/** @brief The clock: internal 12 MHz or an external clock on CLK (page 119). */
struct Clock {
  bool external{false};    ///< `true` when an external clock drives CLK.
  uint32_t hz{12'000'000}; ///< The clock frequency: 12 MHz internal, 4 to 18 MHz external.
};

/** @brief Run and hold current requests, RMS in mA; `PlanCurrent()` turns them into registers. */
struct Current {
  uint32_t run_ma{0};  ///< Run current in mA, 1 or more.
  uint32_t hold_ma{0}; ///< Standstill current in mA; at most `run_ma` unless `Ack::HoldAboveRun`.
};

/** @brief Standstill behaviour (IHOLD_IRUN.IHOLDDELAY, TPOWERDOWN, GCONF.faststandstill). */
struct Standstill {
  uint8_t hold_delay{1}; ///< IHOLDDELAY 0 to 15, in 2^18 clocks per step of the current ramp.
  uint8_t power_down_delay{10}; ///< TPOWERDOWN 0 to 255, in 2^18 clocks; reset value 10.
  bool fast_standstill{false};  ///< `true`: standstill after 2^18 clocks instead of 2^20.
};

/** @brief Chopper configuration (CHOPCONF, section 6.5.2 page 51). */
struct Chopper {
  ChopperMode mode{ChopperMode::SpreadCycle}; ///< SpreadCycle or constant off time.
  uint8_t toff{3};           ///< Off time 1 to 15 (0 is `Disable()`); reset value 0 means off.
  uint8_t tbl{2};            ///< Blank time 0 to 3: 16, 24, 36, 54 clocks; reset value 2.
  uint8_t tpfd{4};           ///< Passive fast decay 0 to 15, 128 clocks per step; reset value 4.
  uint8_t hstrt{5};          ///< SpreadCycle hysteresis start 0 to 7 (reset 5).
  uint8_t hend{2};           ///< SpreadCycle hysteresis end 0 to 15 (reset 2).
  uint8_t tfd{0};            ///< Constant off time: fast decay time 0 to 15.
  uint8_t offset{0};         ///< Constant off time: sine wave offset 0 to 15.
  bool no_comparator{false}; ///< Constant off time: no current comparator on fast decay.
  Microsteps mres{Microsteps::M256}; ///< Microstep resolution.
  bool interpolate{true};            ///< Interpolate to 256 microsteps (reset 1).
  bool double_edge{false};           ///< Step on both STEP edges.
  bool high_fullstep{false};         ///< Full steps above THIGH.
  bool high_const_off{false};        ///< Constant off time above THIGH.
};

/** @brief StealthChop settings (PWMCONF, GCONF.en_pwm_mode, section 6.5.2 page 54). */
struct StealthChop {
  bool enable{false};   ///< GCONF.en_pwm_mode.
  bool autoscale{true}; ///< Automatic current control (reset 1).
  bool autograd{true};  ///< Automatic gradient adaptation (reset 1).
  uint8_t freq{0};      ///< PWM frequency 0 to 3: 2/1024, 2/683, 2/512, 2/410 of fCLK.
  uint8_t reg{4};       ///< PWM_REG 1 to 15 (reset 4).
  uint8_t lim{12};      ///< PWM_LIM 0 to 15 (reset 12).
  uint8_t ofs{30};      ///< PWM_OFS 0 to 255 (reset 30).
  uint8_t grad{0};      ///< PWM_GRAD 0 to 255.
  Freewheel freewheel{Freewheel::Normal}; ///< Standstill mode with IHOLD = 0.
};

/** @brief Velocity thresholds as TSTEP values (page 39); 0 disables. */
struct Thresholds {
  uint32_t pwm{0};              ///< TPWMTHRS: StealthChop below this velocity, 0 to 2^20 - 1.
  uint32_t cool{0};             ///< TCOOLTHRS: CoolStep and stall output above this velocity.
  uint32_t high{0};             ///< THIGH: the high-velocity chopper mode above this velocity.
  bool small_hysteresis{false}; ///< GCONF.small_hysteresis: 1/32 instead of 1/16.
};

/** @brief CoolStep (COOLCONF, section 13, page 87). */
struct CoolStep {
  uint8_t semin{0};        ///< Lower threshold 0 to 15; 0 turns CoolStep off.
  uint8_t semax{0};        ///< Upper threshold offset 0 to 15.
  uint8_t seup{0};         ///< Increment 0 to 3: 1, 2, 4, 8.
  uint8_t sedn{0};         ///< Decrement rate 0 to 3: 32, 8, 2, 1 readings.
  bool min_quarter{false}; ///< Minimum current a quarter instead of a half of IRUN.
};

/** @brief StallGuard2 (COOLCONF, section 13, page 87). */
struct StallGuard {
  int8_t threshold{0}; ///< SGT, -64 to 63.
  bool filter{false};  ///< SFILT.
};

/** @brief DcStep (VDCMIN, DCCTRL, section 17, page 100). */
struct DcStep {
  uint32_t vdcmin{0};  ///< VDCMIN, 0 (off) or 256 to 2^23 - 1; bits 7..0 are ignored.
  uint16_t dc_time{0}; ///< DC_TIME 0 to 1023 clocks.
  uint8_t dc_sg{0};    ///< DC_SG 0 to 255, 16 clocks per count; 0 is off.
};

/** @brief Gate driver settings (DRV_CONF, section 6.1 page 35); written only when `declared`. */
struct Bridge {
  bool declared{false}; ///< `true`: the application declares these values; `false`: not written.
  uint8_t bbm_time{0};  ///< BBMTIME 0 to 24 (about 8.3 ns each).
  uint8_t bbm_clks{4};  ///< BBMCLKS 0 to 15 clocks.
  uint8_t drive_strength{0};       ///< DRVSTRENGTH 0 to 3.
  uint8_t sense_filter{0};         ///< FILT_ISENSE 0 to 3: 100, 200, 300, 400 ns.
  OtLevel ot_level{OtLevel::C150}; ///< Overtemperature shutdown level.
  uint16_t mosfet_switching_ns{0}; ///< Slowest MOSFET switching time in ns, 0 = not declared.
};

/** @brief Short protection (SHORT_CONF, section 11.2 page 78); written only when `declared`. */
struct Protection {
  bool declared{false};    ///< `true`: the application declares these values; `false`: not written.
  uint8_t s2vs_level{6};   ///< S2VS_LEVEL 4 to 15.
  uint8_t s2g_level{6};    ///< S2G_LEVEL 2 to 15 (12 to 15 above 52 V).
  uint8_t filter{1};       ///< SHORTFILTER 0 to 3.
  bool long_delay{false};  ///< shortdelay: 1500 ns instead of 750 ns.
  bool disable_s2g{false}; ///< `diss2g`; needs `Ack::ShortProtectionOff`.
  bool disable_s2vs{false}; ///< `diss2vs`; needs `Ack::ShortProtectionOff`.
};

/** @brief General flags (GCONF). */
struct Flags {
  bool invert_direction{false}; ///< GCONF.shaft.
  bool recalibrate{false};      ///< GCONF.recalibrate.
  bool multistep_filt{false};   ///< GCONF.multistep_filt (page 33; reset text says 1, A12).
  bool stop_input{false};       ///< GCONF.stop_enable: ENCA_DCIN stops the sequencer.
};

/** @brief DIAG0 and DIAG1 outputs (GCONF bits 5 to 13, section 6.1 page 32). */
struct Diag {
  bool diag0_error{false};         ///< DIAG0 on driver error (StepDir mode).
  bool diag0_otpw{false};          ///< DIAG0 on pre-warning (StepDir mode).
  bool diag0_stall{false};         ///< DIAG0 on stall (StepDir mode).
  bool diag0_step_output{false};   ///< DIAG0 outputs the internal STEP (MotionController mode).
  bool diag1_stall{false};         ///< DIAG1 on stall (StepDir mode).
  bool diag1_dir_output{false};    ///< DIAG1 outputs the internal DIR (MotionController mode).
  bool diag1_index{false};         ///< DIAG1 on microstep index 0 (StepDir mode).
  bool diag1_on_state{false};      ///< DIAG1 on chopper on-state (StepDir mode).
  bool diag1_steps_skipped{false}; ///< DIAG1 toggles on skipped DcStep steps; exclusive.
  bool diag0_push_pull{false};     ///< DIAG0 push-pull instead of open drain.
  bool diag1_push_pull{false};     ///< DIAG1 push-pull instead of open drain.
};

/** @brief The six-point ramp (page 40). Defaults are the datasheet's quick-start values (page 116).
 */
struct RampProfile {
  uint32_t vstart{0};     ///< VSTART 0 to 2^18 - 1.
  uint32_t a1{1000};      ///< A1 0 to 65535.
  uint32_t v1{50'000};    ///< V1 0 to 2^20 - 1; 0 disables A1 and D1.
  uint32_t amax{500};     ///< AMAX 0 to 65535.
  uint32_t vmax{200'000}; ///< VMAX 0 to 2^23 - 512.
  uint32_t dmax{700};     ///< DMAX 0 to 65535.
  uint32_t d1{1400};      ///< D1 1 to 65535.
  uint32_t vstop{10};     ///< VSTOP 1 to 2^18 - 1.
  uint32_t zero_wait{0};  ///< TZEROWAIT 0 to 65535, 512 clocks per count.
};

/** @brief Reference switches and stop behaviour (SW_MODE, page 43). */
struct Switches {
  bool stop_left{false};            ///< Stop at an active left switch.
  bool stop_right{false};           ///< Stop at an active right switch.
  bool left_active_low{false};      ///< The left switch is active low.
  bool right_active_low{false};     ///< The right switch is active low.
  bool swap{false};                 ///< Swap left and right.
  bool latch_left_active{false};    ///< Latch XACTUAL on the active edge of left.
  bool latch_left_inactive{false};  ///< Latch XACTUAL on the inactive edge of left.
  bool latch_right_active{false};   ///< Latch XACTUAL on the active edge of right.
  bool latch_right_inactive{false}; ///< Latch XACTUAL on the inactive edge of right.
  bool latch_encoder{false};        ///< Latch X_ENC as well.
  bool stop_on_stall{false};        ///< Stop on stall (sg_stop).
  bool soft_stop{false};            ///< Stop with the deceleration ramp.
};

/** @brief Encoder N-channel event conditions (ENCMODE, page 46). */
struct NEvent {
  bool pol_a{false};         ///< Required A polarity.
  bool pol_b{false};         ///< Required B polarity.
  bool pol_n{false};         ///< Active polarity of N.
  bool ignore_ab{false};     ///< Ignore A and B.
  bool repeat{false};        ///< Latch or clear on every N event (clr_cont).
  bool once{false};          ///< Latch or clear on the next N event only (clr_once).
  bool pos_edge{false};      ///< Sensitive to the active-going edge.
  bool neg_edge{false};      ///< Sensitive to the inactive-going edge.
  bool clear_counter{false}; ///< Clear X_ENC at the event.
  bool latch_xactual{false}; ///< Latch XACTUAL at the event.
};

/** @brief The ENC_CONST accumulation factor (page 45, 107). */
struct EncoderScale {
  bool decimal{false};  ///< ENCMODE.enc_sel_decimal: fraction in 1/10000 instead of 1/65536.
  int16_t integer{1};   ///< Integer part, signed.
  uint16_t fraction{0}; ///< Fractional part: 1/65536 (binary) or 1/10000 (decimal, below 10000).
  bool negative{false}; ///< Decimal only: the constant is negative (page 107 rule).
};

/** @brief Incremental encoder (ABN), written only when `declared`. */
struct Encoder {
  bool declared{false};  ///< `true`: ENCMODE, ENC_CONST and ENC_DEVIATION are written.
  NEvent n_event{};      ///< N-channel conditions.
  EncoderScale scale{};  ///< Accumulation factor.
  uint32_t deviation{0}; ///< ENC_DEVIATION in steps, 0 = off, up to 2^20 - 1.
};

/** @brief A microstep table (MSLUT, MSLUTSEL, MSLUTSTART, section 6.5 page 47). */
struct MicrostepTable {
  std::array<uint32_t, 8> words{reg::kDefaultMslut}; ///< MSLUT[0] to MSLUT[7].
  std::array<uint8_t, 4> w{2, 1, 1, 1};              ///< Segment width codes W0 to W3, 0 to 3.
  uint8_t x1{128};                                   ///< Segment 1 start.
  uint8_t x2{255};                                   ///< Segment 2 start.
  uint8_t x3{255};                                   ///< Segment 3 start.
  uint8_t start_sin{0};                              ///< Table value at entry 0.
  uint8_t start_sin90{247};                          ///< Table value at entry 256.
};

/** @brief The UART options (used by a UART port only). */
struct UartOptions {
  uint8_t node{0};        ///< NODEADDR 0 to 253 (254 only inside the addressing procedure).
  uint8_t send_delay{8};  ///< SENDDELAY 0 to 15, in 8-bit times; at least 2 with `multi_node`.
  bool multi_node{false}; ///< Several nodes share the line.
  uint32_t baud{115'200}; ///< Baud rate the port runs; 9000 to fCLK / 16.
};

/** @brief The SPI options (used by an SPI port only). */
struct SpiOptions {
  uint8_t chain_length{1}; ///< Chips in the daisy chain, 1 to `kMaxChain`.
  uint8_t chain_index{0};  ///< This chip: 0 is the one whose SDI is on MOSI.
};

/** @brief Everything `EnsureInitialized()` writes to the chip. See docs/configuration.md. */
struct Config {
  Mode mode{Mode::MotionController}; ///< SD_MODE strap the board uses.
  Clock clock{};                     ///< The clock source.
  Motor motor{};                     ///< Declared motor; required.
  Board board{};                     ///< Declared power stage; required.
  uint32_t max_velocity_reg{0};      ///< Declared VMAX ceiling; 0 = only the register limit.
  bool include_tolerance{
      true};                ///< Reduce the current ceiling by the 5 percent full-scale tolerance.
  Current current{};        ///< Run and hold current.
  Standstill standstill{};  ///< Standstill behaviour.
  Chopper chopper{};        ///< Chopper.
  StealthChop stealth{};    ///< StealthChop.
  Thresholds thresholds{};  ///< Velocity thresholds.
  CoolStep cool{};          ///< CoolStep.
  StallGuard stall{};       ///< StallGuard2.
  DcStep dcstep{};          ///< DcStep.
  Bridge bridge{};          ///< Gate drivers.
  Protection protection{};  ///< Short protection.
  Flags flags{};            ///< General flags.
  Diag diag{};              ///< DIAG outputs.
  RampProfile ramp{};       ///< Ramp registers.
  Switches switches{};      ///< Reference switches.
  Encoder encoder{};        ///< Encoder.
  bool custom_table{false}; ///< `true`: write `table` (needs MSCNT = 0).
  MicrostepTable table{};   ///< The microstep table.
  UartOptions uart{};       ///< UART options.
  SpiOptions spi{};         ///< SPI options.
  Verify verify{Verify::None}; ///< Verification the start-up uses on its writes.
  AckSet ack{};                ///< Acknowledgements.
  bool begin_enabled{false};   ///< `true`: `EnsureInitialized()` leaves the driver enabled.
};

/** @name Constants */
/// @{
inline constexpr uint32_t kInternalClockHz = 12'000'000;    ///< Internal oscillator (page 119).
inline constexpr uint32_t kExternalClockMinHz = 4'000'000;  ///< Lowest external clock (page 122).
inline constexpr uint32_t kExternalClockMaxHz = 18'000'000; ///< Highest external clock (page 122).
inline constexpr uint32_t kVfsMv = 325;                 ///< Sense threshold, typical (page 123).
inline constexpr uint32_t kCurrentTolerancePercent = 5; ///< Full-scale current tolerance.
inline constexpr uint32_t kVsMinMv = 10'000;            ///< Lowest VS (page 120).
inline constexpr uint32_t kVsMaxMv = 55'000;            ///< Highest VS (page 120).
inline constexpr uint32_t kUartMinBaud = 9000;          ///< Lowest UART baud rate (page 26).
inline constexpr uint32_t kUartRecoveryBits = 12;       ///< Idle bit times after a UART error.
inline constexpr uint32_t kUartReadRetries = 2;         ///< Extra attempts of a failed UART read.
inline constexpr uint32_t kMaxChain = 8; ///< Longest SPI daisy chain the driver frames.
inline constexpr uint32_t kVmaxLimit = (1U << 23) - 512U; ///< Highest VMAX (page 41).
inline constexpr uint32_t kMaxSckInternalHz =
    4'000'000; ///< SPI clock with the internal clock (page 25).
/// @}

namespace detail {
/// Last enumerators, for `IsEnumerator`.
template <>
struct EnumLast<Mode> {
  static constexpr Mode kValue = Mode::StepDir;
};
template <>
struct EnumLast<Verify> {
  static constexpr Verify kValue = Verify::Counter;
};
template <>
struct EnumLast<ChopperMode> {
  static constexpr ChopperMode kValue = ChopperMode::ConstOffTime;
};
template <>
struct EnumLast<Microsteps> {
  static constexpr Microsteps kValue = Microsteps::Full;
};
template <>
struct EnumLast<OtLevel> {
  static constexpr OtLevel kValue = OtLevel::C120;
};
template <>
struct EnumLast<Freewheel> {
  static constexpr Freewheel kValue = Freewheel::ShortHighSide;
};
template <>
struct EnumLast<RampMode> {
  static constexpr RampMode kValue = RampMode::Hold;
};
template <>
struct EnumLast<StopKind> {
  static constexpr StopKind kValue = StopKind::Emergency;
};

/// Increment of a table entry for a width code and a table bit (page 47, 50): -1..+3.
[[nodiscard]] constexpr int32_t TableStep(uint32_t width_code, uint32_t bit) noexcept {
  return static_cast<int32_t>(width_code & 3U) - 1 + static_cast<int32_t>(bit & 1U);
}

/// The width code that applies to entry `n` (segments start at X1, X2, X3).
[[nodiscard]] constexpr uint32_t SegmentWidth(const MicrostepTable& t, uint32_t n) noexcept {
  if (n < t.x1) {
    return t.w[0];
  }
  if (n < t.x2) {
    return t.w[1];
  }
  return (n < t.x3) ? t.w[2] : t.w[3];
}

/// Entry `n` (0 to 255) of the decoded quarter wave: bit `n` is the increment into entry `n` (A6).
[[nodiscard]] constexpr int32_t TableEntry(const MicrostepTable& t, uint32_t n) noexcept {
  auto value = static_cast<int32_t>(t.start_sin);
  for (uint32_t i = 1U; i <= (n & 255U); ++i) {
    const uint32_t bit = (t.words[(i >> 5U) & 7U] >> (i & 31U)) & 1U;
    value += TableStep(SegmentWidth(t, i), bit);
  }
  return value;
}
} // namespace detail

/**
 * @brief Whether a table is acceptable: segment starts ordered, every entry within -248 to 247
 * (the peak may reach 248), and START_SIN90 equal to the last decoded entry (page 47, 105, A6).
 * @param table The table.
 * @return `Ok` or `InvalidArgument`.
 */
[[nodiscard]] constexpr Error CheckTable(const MicrostepTable& table) noexcept {
  if ((table.x1 == 0U) || (table.x1 >= table.x2) || (table.x2 > table.x3)) {
    return Error::InvalidArgument;
  }
  auto value = static_cast<int32_t>(table.start_sin);
  for (uint32_t i = 1U; i <= 255U; ++i) {
    const uint32_t bit = (table.words[(i >> 5U) & 7U] >> (i & 31U)) & 1U;
    value += detail::TableStep(detail::SegmentWidth(table, i), bit);
    if ((value < -248) || (value > 248)) {
      return Error::InvalidArgument;
    }
  }
  const auto last = static_cast<int32_t>(table.start_sin90);
  return (value == last) ? Error::Ok : Error::InvalidArgument;
}

/** @brief Soft guidance bits from `Advise()`; never an error. */
enum class Advice : uint16_t {
  IrunBelow16 = 1U << 0,          ///< IRUN below 16: coarse microsteps (page 38).
  GlobalScalerBelow129 = 1U << 1, ///< GLOBALSCALER below 129 (page 36).
  VstopBelow10 = 1U << 2,         ///< VSTOP below 10 (page 41).
  PwmLimBelow5 = 1U << 3,         ///< PWM_LIM below 5 (page 54).
  PwmFrequency = 1U << 4,         ///< PWM frequency outside 20 to 50 kHz (page 59).
  ToffBelow2 = 1U << 5,           ///< TOFF below 2 (page 51).
  ExternalClock = 1U << 6,        ///< External clock outside 10 to 16 MHz (page 122).
  TblOutside = 1U << 7,           ///< TBL outside 1 to 2 (page 51).
  HoldAbove70 = 1U << 8,          ///< Hold current above 70 percent of run (page 38).
};

namespace detail {
/// `Error::InvalidArgument` unless `ok`.
[[nodiscard]] constexpr Error Need(bool ok) noexcept {
  return ok ? Error::Ok : Error::InvalidArgument;
}
/// First failure of two checks.
[[nodiscard]] constexpr Error Both(Error a, Error b) noexcept {
  return (a != Error::Ok) ? a : b;
}
/// `lo <= v <= hi`.
[[nodiscard]] constexpr Error Range(uint32_t v, uint32_t lo, uint32_t hi) noexcept {
  return Need((v >= lo) && (v <= hi));
}
/// `a <= b` (an ordering rule).
[[nodiscard]] constexpr Error Order(uint32_t a, uint32_t b) noexcept {
  return Need(a <= b);
}
/// `ok` unless `when` is false, then no condition.
[[nodiscard]] constexpr Error Unless(bool when, Error e) noexcept {
  return when ? e : Error::Ok;
}

[[nodiscard]] constexpr Error ValidateClock(const Config& c) noexcept {
  Error e = Need(IsEnumerator(c.mode) && IsEnumerator(c.verify));
  e = Both(e, c.clock.external ? Range(c.clock.hz, kExternalClockMinHz, kExternalClockMaxHz)
                               : Need(c.clock.hz == kInternalClockHz));
  e = Both(e, Range(c.uart.node, 0U,
                    c.uart.multi_node ? 254U : 253U)); // 254: the addressing procedure (A31)
  e = Both(e, Range(c.uart.send_delay, c.uart.multi_node ? 2U : 0U, 15U));
  e = Both(e, Range(c.uart.baud, kUartMinBaud, c.clock.hz / 16U));
  e = Both(e, Range(c.spi.chain_length, 1U, kMaxChain));
  return Both(e, Order(static_cast<uint32_t>(c.spi.chain_index) + 1U, c.spi.chain_length));
}

[[nodiscard]] constexpr Error ValidateDeclared(const Config& c) noexcept {
  const uint32_t supply_max =
      (c.board.supply_max_mv == 0U) ? c.board.supply_mv : c.board.supply_max_mv;
  Error e = Range(c.motor.rated_rms_ma, 1U, 100'000U);
  e = Both(e, Range(c.motor.max_rms_ma, 0U, 100'000U));
  e = Both(e, Range(c.motor.fullsteps_per_rev, 1U, 1'000'000U));
  e = Both(e, Range(c.board.rsense_mohm, 1U, 10'000U));
  e = Both(e, Range(c.board.supply_mv, kVsMinMv, kVsMaxMv));
  e = Both(e, Range(supply_max, c.board.supply_mv, kVsMaxMv));
  e = Both(e, Range(c.current.run_ma, 1U, 100'000U));
  return Both(e, Range(c.standstill.hold_delay, 0U, 15U));
}

[[nodiscard]] constexpr Error ValidateChopper(const Config& c) noexcept {
  const Chopper& k = c.chopper;
  Error e = Need(IsEnumerator(k.mode) && IsEnumerator(k.mres));
  e = Both(e, Range(k.toff, 1U, 15U));
  e = Both(e, Range(k.tbl, (k.toff == 1U) ? 2U : 0U, 3U));
  e = Both(e, Range(k.tpfd, 0U, 15U));
  e = Both(e, Range(k.hstrt, 0U, 7U));
  e = Both(e, Range(k.hend, 0U, 15U));
  e = Both(e, Range(k.tfd, 0U, 15U));
  e = Both(e, Range(k.offset, 0U, 15U));
  const uint32_t effective = static_cast<uint32_t>(k.hstrt) + static_cast<uint32_t>(k.hend);
  return Both(e, Unless(k.mode == ChopperMode::SpreadCycle, Order(effective, 18U)));
}

[[nodiscard]] constexpr Error ValidateStealth(const Config& c) noexcept {
  const StealthChop& s = c.stealth;
  Error e = Need(IsEnumerator(s.freewheel));
  e = Both(e, Range(s.freq, 0U, 3U));
  e = Both(e, Range(s.reg, s.autoscale ? 1U : 0U, 15U));
  e = Both(e, Range(s.lim, 0U, 15U));
  return Both(e, Range(c.standstill.power_down_delay, (s.enable && s.autoscale) ? 2U : 0U, 255U));
}

[[nodiscard]] constexpr Error ValidateThresholds(const Config& c) noexcept {
  const Thresholds& t = c.thresholds;
  Error e = Range(t.pwm, 0U, 0xFFFFFU);
  e = Both(e, Range(t.cool, 0U, 0xFFFFFU));
  e = Both(e, Range(t.high, 0U, 0xFFFFFU));
  e = Both(e, Unless((t.cool != 0U) && (t.pwm != 0U), Order(t.cool, t.pwm)));
  e = Both(e, Unless((t.high != 0U) && (t.cool != 0U), Order(t.high, t.cool)));
  const bool needs_cool = (c.cool.semin != 0U) || c.switches.stop_on_stall;
  e = Both(e, Unless(needs_cool, Range(t.cool, 1U, 0xFFFFFU)));
  e = Both(e, Range(c.cool.semin, 0U, 15U));
  e = Both(e, Range(c.cool.semax, 0U, 15U));
  e = Both(e, Range(c.cool.seup, 0U, 3U));
  e = Both(e, Range(c.cool.sedn, 0U, 3U));
  e = Both(e, Need((c.stall.threshold >= -64) && (c.stall.threshold <= 63)));
  e = Both(e, Unless(c.dcstep.vdcmin != 0U, Range(c.dcstep.vdcmin, 256U, 0x7FFFFFU)));
  return Both(e, Range(c.dcstep.dc_time, 0U, 1023U));
}

[[nodiscard]] constexpr Error ValidateDcStep(const Config& c) noexcept {
  Error e = Need(!c.custom_table);
  e = Both(e, Need(c.chopper.high_fullstep && c.chopper.high_const_off));
  e = Both(e, Range(c.chopper.toff, 3U, 15U));
  // DC_TIME has a lower limit: tBLANK in clock cycles plus 1 to 100 (page 101, A44); at or below
  // tBLANK the load measurement never sees a reference pulse.
  constexpr std::array<uint32_t, 4> kBlankClocks{16U, 24U, 36U, 54U};
  const uint32_t lowest = kBlankClocks[static_cast<std::size_t>(c.chopper.tbl & 3U)] + 1U;
  return Unless(c.dcstep.vdcmin != 0U, Both(e, Range(c.dcstep.dc_time, lowest, 1023U)));
}

[[nodiscard]] constexpr uint32_t BbmNs(const Bridge& b) noexcept {
  const uint32_t from_time = (static_cast<uint32_t>(b.bbm_time) * 83U) / 10U;
  const uint32_t from_clks = static_cast<uint32_t>(b.bbm_clks) * 83U;
  return (from_time > from_clks) ? from_time : from_clks;
}

[[nodiscard]] constexpr Error ValidateBridge(const Config& c) noexcept {
  const Bridge& b = c.bridge;
  Error e = Need(IsEnumerator(b.ot_level));
  e = Both(e, Range(b.bbm_time, 0U, 24U));
  e = Both(e, Range(b.bbm_clks, 0U, 15U));
  e = Both(e, Range(b.drive_strength, 0U, 3U));
  e = Both(e, Range(b.sense_filter, 0U, 3U));
  e = Both(e, Unless(b.mosfet_switching_ns != 0U,
                     Order(static_cast<uint32_t>(b.mosfet_switching_ns) * 130U, BbmNs(b) * 100U)));
  const Protection& p = c.protection;
  const uint32_t supply_max =
      (c.board.supply_max_mv == 0U) ? c.board.supply_mv : c.board.supply_max_mv;
  e = Both(e, Range(p.s2vs_level, 4U, 15U));
  e = Both(e, Range(p.s2g_level, (supply_max > 52'000U) ? 12U : 2U, 15U));
  return Both(e, Range(p.filter, 0U, 3U));
}

[[nodiscard]] constexpr Error ValidateRamp(const Config& c) noexcept {
  const RampProfile& r = c.ramp;
  Error e = Range(r.vstart, 0U, 0x3FFFFU);
  e = Both(e, Range(r.a1, 0U, 0xFFFFU));
  e = Both(e, Range(r.v1, 0U, 0xFFFFFU));
  e = Both(e, Range(r.amax, 0U, 0xFFFFU));
  e = Both(e, Range(r.dmax, 0U, 0xFFFFU));
  e = Both(e, Range(r.d1, 1U, 0xFFFFU));
  e = Both(e, Range(r.vstop, 1U, 0x3FFFFU));
  e = Both(e, Range(r.zero_wait, 0U, 0xFFFFU));
  e = Both(e,
           Range(r.vmax, r.vstart, (c.max_velocity_reg == 0U) ? kVmaxLimit : c.max_velocity_reg));
  e = Both(e, Order(r.vmax, kVmaxLimit));
  return e;
}

[[nodiscard]] constexpr Error ValidateDiag(const Config& c) noexcept {
  const Diag& d = c.diag;
  const bool sd = c.mode == Mode::StepDir;
  const bool sd_only = d.diag0_error || d.diag0_otpw || d.diag0_stall || d.diag1_stall ||
                       d.diag1_index || d.diag1_on_state;
  Error e = Unless(!sd, Need(!sd_only));
  e = Both(e, Unless(sd, Need(!(d.diag0_step_output || d.diag1_dir_output))));
  const bool other1 = d.diag1_stall || d.diag1_index || d.diag1_on_state || d.diag1_dir_output;
  e = Both(e, Unless(d.diag1_steps_skipped, Need(!other1)));
  e = Both(e, Need(!(c.switches.soft_stop && c.switches.stop_on_stall)));
  e = Both(e, Range(c.encoder.deviation, 0U, 0xFFFFFU));
  return Both(e, Order(c.encoder.scale.fraction, c.encoder.scale.decimal ? 9999U : 65'535U));
}
} // namespace detail

namespace detail {
/// `Refused` unless `ok` or `ack` is given.
[[nodiscard]] constexpr Error NeedAck(bool needs, const Config& c, Ack ack) noexcept {
  return (needs && !c.ack.Has(ack)) ? Error::Refused : Error::Ok;
}
} // namespace detail

/**
 * @brief The guards that an acknowledgement can lift (section 2.5 of the design): each returns
 * `Refused` when the configuration needs an `Ack` it does not carry.
 * @param c The configuration.
 * @return `Ok` or `Refused`.
 */
[[nodiscard]] constexpr Error CheckAcks(const Config& c) noexcept {
  using detail::Both;
  using detail::NeedAck;
  const bool short_off = c.protection.disable_s2g || c.protection.disable_s2vs;
  const bool low_ot = (c.bridge.ot_level == OtLevel::C120) ||
                      ((c.bridge.ot_level == OtLevel::C136) && (c.board.supply_mv > 24'000U));
  const bool hard_stop =
      (c.switches.stop_left || c.switches.stop_right || c.switches.stop_on_stall) &&
      !c.switches.soft_stop;
  Error e = NeedAck(short_off, c, Ack::ShortProtectionOff);
  e = Both(e, NeedAck(c.bridge.declared && low_ot, c, Ack::LowOtLevel));
  e = Both(e, NeedAck(hard_stop && c.stealth.enable, c, Ack::HardStopWithStealthChop));
  return Both(e, NeedAck(c.ramp.vstart > c.ramp.vstop, c, Ack::VstartAboveVstop));
}

/**
 * @brief Check a configuration without touching the chip.
 *
 * Rejects every combination the datasheet forbids: the declared motor and power stage are missing
 * or out of range, a field is out of range, the chopper, thresholds, ramp, bridge, diag or table
 * rules of section 2.5 of the design are broken. Rules that need a current plan (the ceiling) are
 * checked by `PlanCurrent()`.
 *
 * @param config The configuration.
 * @return `Error::Ok` or `Error::InvalidArgument`.
 */
[[nodiscard]] constexpr Error Validate(const Config& config) noexcept {
  Error e = detail::ValidateClock(config);
  e = detail::Both(e, detail::ValidateDeclared(config));
  e = detail::Both(e, detail::ValidateChopper(config));
  e = detail::Both(e, detail::ValidateStealth(config));
  e = detail::Both(e, detail::ValidateThresholds(config));
  e = detail::Both(e, detail::ValidateDcStep(config));
  e = detail::Both(e, detail::ValidateBridge(config));
  e = detail::Both(e, detail::ValidateRamp(config));
  e = detail::Both(e, detail::ValidateDiag(config));
  return detail::Both(e, config.custom_table ? CheckTable(config.table) : Error::Ok);
}

/**
 * @brief Soft guidance for a configuration: bits of `Advice`, never an error.
 * @param config The configuration.
 * @return The `Advice` bits that apply.
 */
[[nodiscard]] constexpr uint32_t Advise(const Config& config) noexcept {
  uint32_t bits = 0U;
  const auto flag = [&bits](bool on, Advice a) noexcept {
    bits |= static_cast<uint32_t>(on) * static_cast<uint32_t>(a);
  };
  flag(config.chopper.toff < 2U, Advice::ToffBelow2);
  flag((config.chopper.tbl < 1U) || (config.chopper.tbl > 2U), Advice::TblOutside);
  flag(config.ramp.vstop < 10U, Advice::VstopBelow10);
  flag(config.stealth.lim < 5U, Advice::PwmLimBelow5);
  flag(config.stealth.freq != 0U, Advice::PwmFrequency);
  flag(
      config.clock.external && ((config.clock.hz < 10'000'000U) || (config.clock.hz > 16'000'000U)),
      Advice::ExternalClock);
  flag((config.current.hold_ma * 10U) > (config.current.run_ma * 7U), Advice::HoldAbove70);
  return bits;
}

/**
 * @brief Reached only when a `CheckedConfig` is built from an invalid configuration: it is never
 * defined, so the build fails and names it (KB-CPP20-013).
 */
void ConfigIsInvalid() noexcept;

/**
 * @brief A configuration proven valid at build time.
 *
 * `constexpr CheckedConfig kBoard{MakeConfig(motor, board)};` fails to compile, naming
 * `ConfigIsInvalid`, when `Validate()` rejects it. The driver still validates at start-up.
 */
class CheckedConfig {
public:
  /**
   * @brief Check `config` at build time.
   * @param config The configuration; it must pass `Validate()`.
   */
  consteval explicit CheckedConfig(const Config& config) noexcept : config_(config) {
    if (Validate(config) != Error::Ok) {
      ConfigIsInvalid();
    }
  }
  /** @return The checked configuration. */
  [[nodiscard]] constexpr const Config& Get() const& noexcept {
    return config_;
  }
  /** @brief Not on a temporary: the reference would dangle (KB-CPP20-044). */
  void Get() const&& = delete;

private:
  Config config_;
};

/**
 * @brief A valid configuration for a declared motor and power stage: run current 90 percent of the
 * rated current less the tolerance reserve, hold current 30 percent of run (page 38), the
 * datasheet's quick-start ramp, SpreadCycle with TOFF 3.
 * @param motor The motor; `rated_rms_ma` must be set.
 * @param board The power stage; `rsense_mohm` and `supply_mv` must be set.
 * @return The configuration; check it with `Validate()`.
 */
[[nodiscard]] constexpr Config MakeConfig(const Motor& motor, const Board& board) noexcept {
  Config c{};
  c.motor = motor;
  c.board = board;
  const uint32_t ceiling = (motor.max_rms_ma == 0U) ? motor.rated_rms_ma : motor.max_rms_ma;
  c.current.run_ma = (ceiling * 85U) / 100U;
  c.current.hold_ma = (c.current.run_ma * 3U) / 10U;
  return c;
}

// A declared motor and power stage give a valid configuration; the default table decodes (A6).
static_assert(Validate(Config{}) == Error::InvalidArgument);
static_assert(Validate(MakeConfig(Motor{.rated_rms_ma = 1000},
                                  Board{.rsense_mohm = 100, .supply_mv = 24'000})) == Error::Ok);
static_assert(CheckTable(MicrostepTable{}) == Error::Ok);

/** @return The driver version, e.g. `"0.1.0"`, from the generated `tmc5160_version.h`. No I/O. */
constexpr const char* GetDriverVersion() noexcept {
  return TMC5160_VERSION_STRING;
}
/** @return The major version. No I/O. */
constexpr uint32_t GetDriverVersionMajor() noexcept {
  return TMC5160_VERSION_MAJOR;
}
/** @return The minor version. No I/O. */
constexpr uint32_t GetDriverVersionMinor() noexcept {
  return TMC5160_VERSION_MINOR;
}
/** @return The patch version. No I/O. */
constexpr uint32_t GetDriverVersionPatch() noexcept {
  return TMC5160_VERSION_PATCH;
}

/** @} */

} // namespace tmc5160

#endif // TMC5160_TYPES_HPP
