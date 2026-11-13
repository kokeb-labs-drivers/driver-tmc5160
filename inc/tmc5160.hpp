/**
 * @file tmc5160.hpp
 * @brief TMC5160 driver: the one header to include.
 * @copyright Copyright (c) 2026 The driver-tmc5160 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * `tmc5160::TMC5160<Port>` drives one TMC5160 or TMC5160A stepper motor controller and driver
 * through a platform port you write: derive it from `SpiInterface` (SPI, optionally in a daisy
 * chain) or from `UartInterface` (single-wire UART). The driver has no platform code, allocates
 * nothing, throws nothing, takes no lock and is not thread-safe: give each object one owner, or
 * guard it with one mutex.
 *
 * The coil current is never a raw number: it follows from the declared motor, the sense resistor
 * and the supply (`Config::motor`, `Config::board`), and every write that could hurt the motor or
 * the power stage has a guard (docs/configuration.md lists them).
 */
#ifndef TMC5160_HPP
#define TMC5160_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "tmc5160_detail.hpp"
#include "tmc5160_registers.hpp"
#include "tmc5160_spi_interface.hpp"
#include "tmc5160_status.hpp"
#include "tmc5160_types.hpp"
#include "tmc5160_uart_interface.hpp"
#include "tmc5160_units.hpp"
#include "tmc5160_version.h"

namespace tmc5160 {

/// Satisfied by a port of exactly one bus kind.
template <typename Port>
concept AnyPort = SpiPort<Port> != UartPort<Port>;

namespace detail {

/// The port declares something called `GpioSet`.
template <typename Port>
concept DeclaresGpioSet =
    requires(Port& port) { port.GpioSet(CtrlPin::DrvEnn, GpioSignal::ACTIVE); };
/// The port drives control pins: `bool GpioSet(CtrlPin, GpioSignal) noexcept`.
template <typename Port>
concept HasGpioSet =
    requires { ExactHook<Port, bool(CtrlPin, GpioSignal) noexcept>::Take(&Port::GpioSet); };
/// The port declares something called `GpioGet`.
template <typename Port>
concept DeclaresGpioGet =
    requires(Port& port, bool& level) { port.GpioGet(InputPin::Diag0, level); };
/// The port reads pins: `bool GpioGet(InputPin, bool&) noexcept`.
template <typename Port>
concept HasGpioGet =
    requires { ExactHook<Port, bool(InputPin, bool&) noexcept>::Take(&Port::GpioGet); };
/// The port declares something called `DelayUs`.
template <typename Port>
concept DeclaresDelayUs = requires(Port& port) { port.DelayUs(1_u32); };
/// The port waits: `void DelayUs(uint32_t us) noexcept`.
template <typename Port>
concept HasDelayUs = requires { ExactHook<Port, void(uint32_t) noexcept>::Take(&Port::DelayUs); };
/// The port declares something called `SckHz`.
template <typename Port>
concept DeclaresSckHz = requires(const Port& port) { port.SckHz(); };
/// The port reports its SPI clock: `uint32_t SckHz() const noexcept`.
template <typename Port>
concept HasSckHz = requires { ExactHook<Port, uint32_t() noexcept>::Take(&Port::SckHz); };
/// The port declares something called `Log`.
template <typename Port>
concept DeclaresLog = requires(Port& port) { port.Log(LogLevel::Info, ""); };
/// The port takes log lines: `void Log(LogLevel, const char* message) noexcept`.
template <typename Port>
concept HasLog =
    requires { ExactHook<Port, void(LogLevel, const char*) noexcept>::Take(&Port::Log); };

/// Wait through the port; a port without `DelayUs()` fails to compile at the first call that waits.
template <typename Port>
void DelayUs(Port& port, uint32_t us) noexcept {
  if constexpr (HasDelayUs<Port>) {
    port.DelayUs(us);
  } else {
    static_assert(kAlwaysFalse<Port>,
                  "tmc5160: the port must implement `void DelayUs(uint32_t us) noexcept`");
  }
}

/// Log through the port, or nothing when the port has no `Log()`.
template <typename Port>
void Log(Port& port, LogLevel level, const char* message) noexcept {
  if constexpr (HasLog<Port>) {
    port.Log(level, message);
  } else {
    static_cast<void>(port);
    static_cast<void>(level);
    static_cast<void>(message);
  }
}

/// A register and the value to write to it.
struct Write {
  uint8_t address; ///< Register.
  uint32_t value;  ///< Word.
};

} // namespace detail

/** @brief What `SetStealthChop()` does about IHOLD while the chip tunes itself (page 58, 65). */
enum class TuningPolicy : uint8_t {
  HoldAtRunForAt1 = 0, ///< Wait the AT#1 time at IHOLD = IRUN, then restore the hold current.
  LeaveHoldAtRun = 1,  ///< Leave IHOLD = IRUN; the caller restores it with `SetCurrent()`.
};

/**
 * @defgroup tmc5160_driver TMC5160 driver
 * @brief The driver class.
 * @{
 */

// clang-format off: the table below must keep one row per line.
/**
 * @brief Driver for one TMC5160 on a platform port.
 *
 * Lifecycle: construct with the configuration (no I/O), then `EnsureInitialized()`, the one way to
 * start (bus up, strap and version check, GSTAT cleared, every register written from the
 * configuration with the driver still off), use, `Deinitialize()`. Every call that touches the chip
 * returns `Error::NotInitialized` until `EnsureInitialized()` has succeeded. After a chip reset
 * (`UnexpectedReset`) the driver is uninitialised again: call `EnsureInitialized()`.
 *
 * Optional port hooks, found at compile time:
 *
 * | Hook | Used for | Without it |
 * | --- | --- | --- |
 * | `void DelayUs(uint32_t us) noexcept` | UART recovery, StealthChop tuning wait | Compile error where a wait is needed |
 * | `bool GpioSet(CtrlPin, GpioSignal) noexcept` | DRV_ENN, DcStep pins | `Enable()` and `Disable()` use TOFF only; the pin must be tied low |
 * | `bool GpioGet(InputPin, bool&) noexcept` | DIAG0, DIAG1, DcStep output levels | `Diag0Active()` and `Diag1Active()` return `Unsupported` |
 * | `uint32_t SckHz() const noexcept` | SPI clock check against the chip's limit | No check |
 * | `void Log(LogLevel, const char*) noexcept` | One line per lifecycle event or failure | Nothing is logged |
 *
 * @tparam Port Your class derived from `SpiInterface<Port>` or `UartInterface<Port>`.
 */
// clang-format on
template <AnyPort Port>
class TMC5160 {
  static_assert(
      !detail::DeclaresGpioSet<Port> || detail::HasGpioSet<Port>,
      "tmc5160: the optional GpioSet hook must be `bool GpioSet(CtrlPin, GpioSignal) noexcept`");
  static_assert(
      !detail::DeclaresGpioGet<Port> || detail::HasGpioGet<Port>,
      "tmc5160: the optional GpioGet hook must be `bool GpioGet(InputPin, bool&) noexcept`");
  static_assert(!detail::DeclaresDelayUs<Port> || detail::HasDelayUs<Port>,
                "tmc5160: the DelayUs hook must be `void DelayUs(uint32_t us) noexcept`");
  static_assert(!detail::DeclaresSckHz<Port> || detail::HasSckHz<Port>,
                "tmc5160: the optional SckHz hook must be `uint32_t SckHz() const noexcept`");
  static_assert(
      !detail::DeclaresLog<Port> || detail::HasLog<Port>,
      "tmc5160: the optional Log hook must be `void Log(LogLevel, const char* message) noexcept`");

public:
  /**
   * @brief Bind the driver to a port and store the configuration. No I/O, cannot fail.
   * @param port The port; it must outlive the driver.
   * @param config What `EnsureInitialized()` will apply; checked there, before any I/O.
   */
  explicit TMC5160(Port& port, const Config& config = {}) noexcept : port_(port), config_(config) {}

  /** @brief One driver object per chip. */
  TMC5160(const TMC5160&) = delete;
  /** @brief One driver object per chip. */
  TMC5160& operator=(const TMC5160&) = delete;

  /// @name Lifecycle
  /// @{

  /**
   * @brief Start the chip, once: the only way to initialise the driver.
   *
   * Validates the configuration and the current plan, brings the bus up, quiesces the chip (TOFF 0,
   * DRV_ENN inactive), checks IOIN (VERSION 0x30, SD_MODE strap equals `Config::mode`), clears
   * GSTAT, then writes every register from the configuration; the driver stays disabled unless
   * `Config::begin_enabled`. Each write is verified as `Config::verify` says. It sends the start-up
   * traffic of docs/configuration.md and blocks only in port calls.
   *
   * @return `Ok`, `InvalidArgument`, `LimitExceeded`, `Refused` (the configuration or the current
   *         plan fails a guard; no I/O), `Unsupported` (the port's SPI clock is too fast), `Bus`,
   *         `Timeout`, `Crc` (UART), `DeviceIdMismatch`, `ModeMismatch`, `DeviceFault`,
   *         `VerifyFailed` or `UnexpectedReset`; with `Config::begin_enabled` also `Busy` (the
   *         motor of the last session is still turning or in its TZEROWAIT pause: the start-up is
   *         undone with the bridge off, call it again once the motor stands still).
   */
  Result<void> EnsureInitialized() noexcept;

  /** @return true after a successful `EnsureInitialized()` and before `Deinitialize()` or a reset.
   * No I/O. */
  [[nodiscard]] bool IsInitialized() const noexcept {
    return initialized_;
  }

  /**
   * @brief Switch the driver off (DRV_ENN inactive, TOFF 0) and mark the driver uninitialised.
   *
   * Also ends the ramp (VSTART 0, VMAX 0, velocity mode) as `Disable()` does. The driver is
   * uninitialised afterwards even if a write failed; a failed switch-off keeps the duty, so a later
   * `Deinitialize()` or `Stop()` sends it again. On a driver that never sent anything (no
   * `EnsureInitialized()` call, or the last switch-off completed) it does nothing and returns
   * `Ok`; after a failed start-up it still switches the chip off.
   *
   * @return `Ok`, `Bus`, `Timeout`, `Crc` (UART), `VerifyFailed` or `UnexpectedReset`.
   */
  Result<void> Deinitialize() noexcept;

  /** @return What start-up learned; zeros before the first start. No I/O. */
  [[nodiscard]] const BeginReport& GetBeginReport() const& noexcept {
    return report_;
  }
  /** @brief Not on a temporary driver: the reference would dangle (KB-CPP20-044). */
  void GetBeginReport() const&& = delete;

  /// @}
  /// @name Enable, faults, current
  /// @{

  /**
   * @brief Enable the driver: DRV_ENN active (when the port can), then TOFF from the configuration.
   *
   * In the motion-controller mode the ramp generator must stand still first: the call returns
   * `Busy` while RAMP_STAT.vzero is clear, a TZEROWAIT pause runs or, in position mode with VMAX
   * not 0, position_reached is clear (one read, pages 44, 83 c: vzero alone is also set where the
   * ramp turns back to a target it passed), so the bridge is never energised into a motion that
   * began earlier. Call it again once the motor stands still. `Ok` does not mean the ramp is
   * idle: a velocity-mode ramp held by a stop is left as it is. A stall stop is latched until
   * `ReleaseStall()`; a reference switch stop holds only while the switch is active, and the motor
   * restarts towards the target when the switch is released (pages 43, 44). A position move that
   * a switch or a stall holds short of its target is `Busy` until `Stop()` or `Hold()` ends it.
   * @param verify Extra traffic to prove the CHOPCONF write.
   * @return `Ok`, `NotInitialized`, `Busy` (the motor is not at standstill), `DeviceFault` (an
   * error is latched: `ClearFaults()`), `Bus`, `VerifyFailed` or `UnexpectedReset`.
   */
  Result<void> Enable(Verify verify = Verify::None) noexcept;

  /**
   * @brief Disable the driver: DRV_ENN inactive first, then TOFF 0, then (motion-controller mode)
   * the ramp generator is stopped (VSTART 0, VMAX 0, velocity mode; XTARGET 0 in direct mode) so a
   * later `Enable()` cannot resume the motion. The configured TOFF is kept.
   * @param verify Extra traffic to prove the writes.
   * @return `Ok`, `NotInitialized`, `Bus`, `VerifyFailed` or `UnexpectedReset`. The call never
   * returns `Busy`: the bridge goes off whatever the motor does.
   */
  Result<void> Disable(Verify verify = Verify::None) noexcept;

  /** @return true while the driver is enabled by `Enable()` or `Config::begin_enabled`. No I/O. */
  [[nodiscard]] bool IsEnabled() const noexcept {
    return enabled_;
  }

  /**
   * @return true while a started driver has an error latched (SPI status bit 1 or GSTAT.drv_err);
   * motion calls refuse. False before start-up and after a reset, which clears GSTAT. No I/O.
   */
  [[nodiscard]] bool HasFault() const noexcept {
    return initialized_ && fault_;
  }

  /**
   * @brief Recover from a latched error once its cause is gone: disable (which stops the ramp
   * generator), read DRV_STATUS, clear GSTAT, enable again when the driver was enabled.
   *
   * A motor that is still turning when the call ends is not energised again: the faults are
   * cleared, the driver stays disabled and the call returns `Busy`; call `Enable()` once the motor
   * stands still.
   * @return `Ok`, `NotInitialized`, `Busy` (cleared, but not enabled again: the motor still turns),
   *         `Refused` (overtemperature or pre-warning still set, page 79), `DeviceFault` (the error
   *         returns at once), `Bus` or `UnexpectedReset`.
   */
  Result<void> ClearFaults() noexcept;

  /**
   * @brief Set the run and hold current from the declared motor and sense resistor.
   *
   * The only way to write IHOLD_IRUN and GLOBALSCALER. The request is rounded down, checked against
   * the ceiling and the sense resistor (`units::PlanCurrent()`), and nothing is written when a
   * guard says no.
   *
   * @param run_ma Run current, RMS mA.
   * @param hold_ma Hold current, RMS mA.
   * @param verify Extra traffic: `Echo` one transfer (SPI), `Counter` one IFCNT read (UART).
   * @return The plan, or `NotInitialized`, `InvalidArgument`, `LimitExceeded`, `Refused`, `Busy`
   * (StealthChop tuning needs the run current at or above the lower limit), `Bus`, `VerifyFailed`,
   * `UnexpectedReset`.
   */
  Result<units::CurrentPlan> SetCurrent(uint32_t run_ma, uint32_t hold_ma,
                                        Verify verify = Verify::None) noexcept;

  /** @return The plan in force; zeros before the first start. No I/O. */
  [[nodiscard]] const units::CurrentPlan& GetCurrentPlan() const& noexcept {
    return plan_;
  }
  /** @brief Not on a temporary driver: the reference would dangle (KB-CPP20-044). */
  void GetCurrentPlan() const&& = delete;

  /**
   * @brief Set the standstill behaviour: IHOLDDELAY, TPOWERDOWN and fast standstill detection.
   * @param standstill The settings.
   * @param verify Extra traffic to prove the writes.
   * @return `Ok`, `NotInitialized`, `InvalidArgument`, `Refused`, `Bus`, `VerifyFailed` or
   * `UnexpectedReset`.
   */
  Result<void> SetStandstill(const Standstill& standstill, Verify verify = Verify::None) noexcept;

  /// @}
  /// @name Motion (motion controller mode)
  /// @{

  /**
   * @brief Replace the six-point ramp: the nine ramp registers in the order the datasheet allows.
   * @param ramp The profile; VMAX, D1 and VSTOP are checked (`Validate()` rules).
   * @param verify Extra traffic to prove the writes.
   * @return `Ok`, `NotInitialized`, `Unsupported` (StepDir mode), `InvalidArgument`, `Refused`,
   * `Bus`, `VerifyFailed` or `UnexpectedReset`.
   */
  Result<void> SetRamp(const RampProfile& ramp, Verify verify = Verify::None) noexcept;

  /**
   * @brief Move to an absolute position (position mode); the motor starts at once. Writes
   * XTARGET, position mode, then VSTART and VMAX, so the motor never starts in the old velocity
   * direction or towards an older target (pages 83, 84).
   * @param position Target in microsteps of the selected resolution.
   * @param verify Extra traffic to prove the writes.
   * @return `Ok`, `NotInitialized`, `Unsupported`, `DeviceFault` (error latched), `Refused` (direct
   * mode; the ramp not yet written; or VMAX 0 after `SetMaxVelocity(0)`, until `SetMaxVelocity()`
   * or `SetRamp()` gives a speed again: `RunVelocity()` does not, and the move could never start),
   * `Bus`, `VerifyFailed` or `UnexpectedReset`.
   */
  Result<void> MoveTo(int32_t position, Verify verify = Verify::None) noexcept;

  /**
   * @brief Move relative to the present position: reads XACTUAL, adds `delta`, writes XTARGET.
   * @param delta Distance in microsteps.
   * @param verify Extra traffic to prove the writes.
   * @return As `MoveTo()`, and `LimitExceeded` when the sum overflows 32 bits (after one read).
   */
  Result<void> MoveBy(int32_t delta, Verify verify = Verify::None) noexcept;

  /**
   * @brief Run at a constant velocity until stopped.
   *
   * Write order: in the same velocity mode VMAX, VSTART, mode; from the other velocity mode the
   * mode, VSTART, VMAX (the new speed never reaches the move the other way, and a start from rest
   * begins at VSTART); from position mode the mode, VMAX, VSTART with no read; from hold mode one
   * VACTUAL read decides (at rest or running the asked way: VMAX first; else the mode first).
   *
   * A position ramp in its TZEROWAIT pause, or turning back after an overshoot, can start its own
   * run-back before the mode write arrives. The backward travel is at most
   * VSTART*t + a*t^2/2 + (VSTART + a*t)^2/(2*AMAX) microsteps, with t the time from the call to
   * the accepted mode write (one SPI datagram, or one 8-byte UART frame: 80 bit times, 8.3 ms at
   * 9600 baud) and a = max(A1, AMAX) when V1 > 0 and VSTART < V1, else AMAX (V1 = 0 disables
   * A1, page 40). Set TZEROWAIT longer than t when the application calls this during a position
   * stop (page 83 c). From position mode the motor also keeps, or speeds towards, the old VMAX
   * for that one write before the new VMAX applies. The bound counts from the call: a run-back
   * that already moves keeps its speed for that write too, and 8.3 ms is the least t at 9600
   * baud (the port and the line add to it).
   * @param velocity Velocity register units, signed (see `units::VelocityRegFromHz()`).
   * @param verify Extra traffic to prove the writes.
   * @return `Ok`, `NotInitialized`, `Unsupported`, `DeviceFault`, `Refused`, `LimitExceeded` (above
   * the declared maximum or 2^23 - 512), `InvalidArgument` (AMAX 0), `Bus`, `VerifyFailed`,
   * `UnexpectedReset`.
   */
  Result<void> RunVelocity(int32_t velocity, Verify verify = Verify::None) noexcept;

  /**
   * @brief Change VMAX (a running move follows it). VMAX must be at least VSTART, the chip's and
   * the profile's (the next move writes the profile's again; page 40); VMAX 0 stops a position
   * move, so VSTART 0 is written first (page 84). The value is kept for later moves: after VMAX 0,
   * `MoveTo()` and `MoveBy()` are `Refused` until `SetMaxVelocity()` or `SetRamp()` gives a speed
   * again (`RunVelocity()` does not). A failed call leaves the kept value as it was.
   * @param vmax The new maximum velocity register value.
   * @param verify Extra traffic to prove the write.
   * @return As `RunVelocity()`, and `InvalidArgument` for 0 < `vmax` < VSTART (nothing sent).
   */
  Result<void> SetMaxVelocity(uint32_t vmax, Verify verify = Verify::None) noexcept;

  /**
   * @brief Hold the velocity (RAMPMODE 3): the present velocity stays (a stop event still ends it).
   * It does not slow the motor down; use `Stop()` for that.
   * @param verify Extra traffic to prove the write.
   * @return `Ok`, `NotInitialized`, `Unsupported`, `Bus`, `VerifyFailed` or `UnexpectedReset`.
   */
  Result<void> Hold(Verify verify = Verify::None) noexcept;

  /**
   * @brief Bring the motor to rest.
   * @param kind `Ramp` (velocity mode, VMAX 0), `Immediate` (VSTART and VMAX 0), `HoldTarget`
   *        (XTARGET = XACTUAL) or `Emergency` (driver off).
   * @param verify Extra traffic to prove the writes.
   * @return `Ok`, `NotInitialized`, `Unsupported`, `InvalidArgument`, `Bus`,
   * `VerifyFailed` or `UnexpectedReset`. Every kind is a ramp except `Emergency`.
   */
  Result<void> Stop(StopKind kind, Verify verify = Verify::None) noexcept;

  /**
   * @brief Set the position counter (homing result): needs standstill (the check of `Enable()`);
   * holds first, then writes XACTUAL and XTARGET.
   * @param position The new XACTUAL and XTARGET.
   * @return `Ok`, `NotInitialized`, `Unsupported`, `Busy` (the motor moves, or a position move has
   * not reached its target: `Stop()` or `Hold()` first), `Bus` or `UnexpectedReset`.
   */
  Result<void> SetHomePosition(int32_t position) noexcept;

  /** @return XACTUAL, or `NotInitialized`, `Bus`, `UnexpectedReset`. */
  Result<int32_t> ReadPosition() noexcept;
  /** @return VACTUAL (signed 24 bit), or `NotInitialized`, `Bus`, `UnexpectedReset`. */
  Result<int32_t> ReadVelocity() noexcept;
  /** @return XTARGET, or `NotInitialized`, `Bus`, `UnexpectedReset`. */
  Result<int32_t> ReadTarget() noexcept;
  /** @return RAMP_STAT.position_reached, or `NotInitialized`, `Bus`, `UnexpectedReset`. */
  Result<bool> PositionReached() noexcept;
  /** @return XLATCH, the position latched at a switch event, or `NotInitialized`, `Bus`,
   * `UnexpectedReset`. */
  Result<int32_t> ReadLatchedPosition() noexcept;

  /**
   * @brief Release a stall stop: hold, check standstill (the check of `Enable()`), clear
   * `event_stop_sg`. The motor does not restart by itself.
   * @return `Ok`, `NotInitialized`, `Unsupported`, `Busy` (the motor still moves), `Bus` or
   * `UnexpectedReset`.
   */
  Result<void> ReleaseStall() noexcept;

  /// @}
  /// @name Chopper, StealthChop, CoolStep, StallGuard, DcStep
  /// @{

  /**
   * @brief Change the chopper (CHOPCONF); TOFF is written as 0 while the driver is disabled.
   * @param chopper The settings; `Validate()` rules apply.
   * @param verify Extra traffic: `ReadBack` reads CHOPCONF back.
   * @return `Ok`, `NotInitialized`, `InvalidArgument`, `Refused`, `Bus`, `VerifyFailed`,
   * `UnexpectedReset`.
   */
  Result<void> SetChopper(const Chopper& chopper, Verify verify = Verify::None) noexcept;

  /**
   * @brief Change StealthChop (PWMCONF and GCONF.en_pwm_mode). Switching it on needs standstill
   * (the check of `Enable()`) and writes IHOLD = IRUN first (page 32, 65).
   * @param stealth The settings.
   * @param policy Whether to wait for the chip's AT#1 phase and restore IHOLD.
   * @param verify Extra traffic to prove the writes.
   * @return `Ok`, `NotInitialized`, `Busy` (moving), `InvalidArgument`, `Refused` (IRUN below 8 or
   * below the lower limit for the declared coil resistance), `Bus`, `VerifyFailed`,
   * `UnexpectedReset`.
   */
  Result<void> SetStealthChop(const StealthChop& stealth,
                              TuningPolicy policy = TuningPolicy::HoldAtRunForAt1,
                              Verify verify = Verify::None) noexcept;

  /** @return PWM_SCALE and PWM_AUTO decoded (the tuning result), or `NotInitialized`, `Bus`,
   * `UnexpectedReset`. */
  Result<StealthChopState> ReadStealthChopState() noexcept;

  /**
   * @brief Set TPWMTHRS, TCOOLTHRS, THIGH and the hysteresis bit.
   * @param thresholds The thresholds; `TPWMTHRS >= TCOOLTHRS >= THIGH` where non-zero.
   * @param verify Extra traffic to prove the writes.
   * @return `Ok`, `NotInitialized`, `InvalidArgument`, `Bus`, `VerifyFailed`, `UnexpectedReset`.
   */
  Result<void> SetThresholds(const Thresholds& thresholds, Verify verify = Verify::None) noexcept;

  /**
   * @brief Set CoolStep (COOLCONF); needs TCOOLTHRS of 1 or more when `semin` is non-zero.
   * @param cool The settings.
   * @param verify Extra traffic to prove the write.
   * @return `Ok`, `NotInitialized`, `InvalidArgument`, `Bus`, `VerifyFailed`, `UnexpectedReset`.
   */
  Result<void> SetCoolStep(const CoolStep& cool, Verify verify = Verify::None) noexcept;

  /**
   * @brief Set StallGuard2 (SGT and SFILT in COOLCONF).
   * @param stall The settings.
   * @param verify Extra traffic to prove the write.
   * @return `Ok`, `NotInitialized`, `InvalidArgument`, `Bus`, `VerifyFailed`, `UnexpectedReset`.
   */
  Result<void> SetStallGuard(const StallGuard& stall, Verify verify = Verify::None) noexcept;

  /** @return DRV_STATUS.SG_RESULT (0 to 1023), or `NotInitialized`, `Bus`, `UnexpectedReset`. */
  Result<uint32_t> ReadStallGuard() noexcept;

  /**
   * @brief Set DcStep (VDCMIN and DCCTRL); needs the default table, `high_fullstep`,
   * `high_const_off`, TOFF 3 or more.
   * @param dcstep The settings.
   * @param verify Extra traffic to prove the writes.
   * @return `Ok`, `NotInitialized`, `InvalidArgument`, `Bus`, `VerifyFailed`, `UnexpectedReset`.
   */
  Result<void> SetDcStep(const DcStep& dcstep, Verify verify = Verify::None) noexcept;

  /** @return LOST_STEPS (StepDir mode with DcStep), or `NotInitialized`, `Unsupported` (motion
   * controller mode), `Bus`. */
  Result<uint32_t> ReadLostSteps() noexcept;

  /// @}
  /// @name Power stage
  /// @{

  /**
   * @brief Write DRV_CONF from declared values; needs the driver disabled.
   * @param bridge The settings; `declared` must be true; break-before-make must be at least 130
   * percent of `mosfet_switching_ns` when that is non-zero.
   * @param ack The acknowledgements: `Ack::LowOtLevel` for 120 degrees, or 136 degrees above 24 V.
   * @param verify Extra traffic to prove the write.
   * @return `Ok`, `NotInitialized`, `Busy` (driver enabled), `InvalidArgument`, `LimitExceeded`,
   * `Refused`, `Bus`, `VerifyFailed`, `UnexpectedReset`.
   */
  Result<void> SetBridge(const Bridge& bridge, AckSet ack = {},
                         Verify verify = Verify::None) noexcept;

  /**
   * @brief Write SHORT_CONF and the `diss2g`, `diss2vs` bits from declared values; needs the driver
   * disabled.
   * @param protection The settings; `declared` must be true.
   * @param ack `Ack::ShortProtectionOff` when a protection is disabled.
   * @param verify Extra traffic to prove the writes.
   * @return `Ok`, `NotInitialized`, `Busy`, `InvalidArgument`, `Refused`, `Bus`, `VerifyFailed`,
   * `UnexpectedReset`.
   */
  Result<void> SetProtection(const Protection& protection, AckSet ack = {},
                             Verify verify = Verify::None) noexcept;

  /// @}
  /// @name Flags, DIAG, switches, compare, direct mode
  /// @{

  /**
   * @brief Change the general flags (GCONF).
   * @param flags The flags.
   * @param verify Extra traffic to prove the write.
   * @return `Ok`, `NotInitialized`, `Bus`, `VerifyFailed`, `UnexpectedReset`.
   */
  Result<void> SetFlags(const Flags& flags, Verify verify = Verify::None) noexcept;

  /**
   * @brief Change the DIAG outputs (GCONF); bits of the other SD_MODE are refused.
   * @param diag The settings.
   * @param verify Extra traffic to prove the write.
   * @return `Ok`, `NotInitialized`, `InvalidArgument`, `Bus`, `VerifyFailed`, `UnexpectedReset`.
   */
  Result<void> SetDiag(const Diag& diag, Verify verify = Verify::None) noexcept;

  /**
   * @brief Change the reference switches and stop behaviour (SW_MODE).
   * @param switches The settings; a hard stop with StealthChop needs `Ack::HardStopWithStealthChop`
   * in the configuration.
   * @param verify Extra traffic: `ReadBack` reads SW_MODE back.
   * @return `Ok`, `NotInitialized`, `Unsupported`, `InvalidArgument`, `Refused`, `Bus`,
   * `VerifyFailed`, `UnexpectedReset`.
   */
  Result<void> SetSwitches(const Switches& switches, Verify verify = Verify::None) noexcept;

  /**
   * @brief Stop on stall (sg_stop); needs TCOOLTHRS of 1 or more and no soft stop.
   * @param enable `true` to stop on a stall.
   * @param verify Extra traffic to prove the write.
   * @return As `SetSwitches()`.
   */
  Result<void> SetStopOnStall(bool enable, Verify verify = Verify::None) noexcept;

  /**
   * @brief Arm the position compare output (X_COMPARE, DIAG1 pulse).
   * @param position The position that makes the chip pulse.
   * @return `Ok`, `NotInitialized`, `Unsupported`, `Bus`, `UnexpectedReset`.
   */
  Result<void> SetPositionCompare(int32_t position) noexcept;

  /**
   * @brief Enter direct mode: the coil currents come from XTARGET (page 110). Needs
   * `Ack::DirectMode` in the configuration and a stopped motor.
   * @return `Ok`, `NotInitialized`, `Unsupported`, `Refused` (no acknowledgement), `Busy`, `Bus`,
   * `UnexpectedReset`.
   */
  Result<void> EnterDirectMode() noexcept;

  /**
   * @brief Write the two coil currents in direct mode (XTARGET: A in bits 8..0, B in bits 24..16).
   * @param coil_a Phase A, -248 to 248 (255 with `Ack::VoltageModeNoCurrentLimit`).
   * @param coil_b Phase B, same range.
   * @return `Ok`, `NotInitialized`, `Refused` (not in direct mode), `LimitExceeded`, `Bus`,
   * `UnexpectedReset`.
   */
  Result<void> SetDirectCurrents(int32_t coil_a, int32_t coil_b) noexcept;

  /**
   * @brief Leave direct mode in the safe order: hold, clear direct_mode, XTARGET = XACTUAL.
   * @return `Ok`, `NotInitialized`, `Bus`, `UnexpectedReset`.
   */
  Result<void> ExitDirectMode() noexcept;

  /// @}
  /// @name Encoder and microstep table
  /// @{

  /**
   * @brief Write ENCMODE, ENC_CONST and ENC_DEVIATION.
   * @param encoder The settings; `declared` must be true.
   * @param verify Extra traffic: `ReadBack` reads ENCMODE back.
   * @return `Ok`, `NotInitialized`, `Unsupported` (StepDir mode), `InvalidArgument`, `Bus`,
   * `VerifyFailed`, `UnexpectedReset`.
   */
  Result<void> SetEncoder(const Encoder& encoder, Verify verify = Verify::None) noexcept;

  /** @return X_ENC, or `NotInitialized`, `Unsupported`, `Bus`, `UnexpectedReset`. */
  Result<int32_t> ReadEncoderPosition() noexcept;
  /**
   * @brief Set the encoder counter.
   * @param position The new X_ENC.
   * @return `Ok`, `NotInitialized`, `Unsupported`, `Bus`, `UnexpectedReset`.
   */
  Result<void> SetEncoderPosition(int32_t position) noexcept;
  /** @return ENC_LATCH, or `NotInitialized`, `Unsupported`, `Bus`, `UnexpectedReset`. */
  Result<int32_t> ReadEncoderLatch() noexcept;
  /** @return ENC_STATUS decoded, or `NotInitialized`, `Unsupported`, `Bus`, `UnexpectedReset`. */
  Result<EncoderStatus> ReadEncoderStatus() noexcept;
  /**
   * @brief Clear encoder events (write 1 to clear).
   * @param n_event Clear the N event.
   * @param deviation_warn Clear the deviation warning.
   * @return `Ok`, `NotInitialized`, `Unsupported`, `Bus`, `UnexpectedReset`.
   */
  Result<void> ClearEncoderEvents(bool n_event, bool deviation_warn) noexcept;

  /**
   * @brief Write a microstep table. Needs MSCNT = 0 (read first), a motor at rest and a table that
   * passes `CheckTable()`.
   * @param table The table.
   * @return `Ok`, `NotInitialized`, `Refused` (MSCNT is not 0 or the motor moves),
   * `InvalidArgument`, `Bus`, `UnexpectedReset`.
   */
  Result<void> SetMicrostepTable(const MicrostepTable& table) noexcept;

  /** @return MSCNT 0 to 1023, or `NotInitialized`, `Bus`, `UnexpectedReset`. */
  Result<uint32_t> ReadMicrostepCounter() noexcept;
  /** @return MSCURACT decoded, or `NotInitialized`, `Bus`, `UnexpectedReset`. */
  Result<MicrostepCurrents> ReadMicrostepCurrents() noexcept;

  /// @}
  /// @name Status and diagnostics
  /// @{

  /** @return GSTAT decoded; a set `drv_err` latches the fault, a set `reset` makes the call fail
   * with `UnexpectedReset`. */
  Result<GlobalStatus> ReadGlobalStatus() noexcept;
  /**
   * @brief Clear GSTAT bits (write 1 to clear); bit 0 of `mask` is `reset`, 1 `drv_err`, 2 `uv_cp`.
   * @param mask The bits to clear, 1 to 7.
   * @return `Ok`, `NotInitialized`, `InvalidArgument`, `Bus`, `UnexpectedReset`.
   */
  Result<void> ClearGlobalStatus(uint32_t mask) noexcept;
  /** @return DRV_STATUS decoded, or `NotInitialized`, `Bus`, `UnexpectedReset`. */
  Result<DriverStatus> ReadDriverStatus() noexcept;
  /** @return RAMP_STAT decoded, or `NotInitialized`, `Unsupported` (StepDir mode), `Bus`,
   * `UnexpectedReset`. */
  Result<RampStatus> ReadRampStatus() noexcept;
  /**
   * @brief Clear RAMP_STAT events: only the write-to-clear bits (mask 0x10CC) are ever written.
   * @param mask The bits to clear; any bit outside 0x10CC gives `InvalidArgument`.
   * @return `Ok`, `NotInitialized`, `Unsupported`, `InvalidArgument`, `Bus`, `UnexpectedReset`.
   */
  Result<void> ClearRampEvents(uint32_t mask) noexcept;
  /** @return IOIN decoded (pin levels and VERSION), or `NotInitialized`, `Bus`, `UnexpectedReset`.
   */
  Result<InputPins> ReadInputs() noexcept;
  /** @return OFFSET_READ decoded, or `NotInitialized`, `Bus`, `UnexpectedReset`. */
  Result<Offsets> ReadOffsets() noexcept;
  /** @return TSTEP, or `NotInitialized`, `Bus`, `UnexpectedReset`. */
  Result<uint32_t> ReadStepTime() noexcept;
  /** @return IFCNT (UART only), or `NotInitialized`, `Unsupported` (SPI), `Bus`, `Timeout`, `Crc`.
   */
  Result<uint32_t> ReadInterfaceCounter() noexcept;
  /** @return OTP_READ decoded, or `NotInitialized`, `Bus`, `UnexpectedReset`. */
  Result<OtpImage> ReadOtp() noexcept;
  /** @return FACTORY_CONF.FCLKTRIM (never written by the driver), or `NotInitialized`, `Bus`,
   * `UnexpectedReset`. */
  Result<uint32_t> ReadClockTrim() noexcept;
  /** @return IOIN.VERSION (0x30), or `NotInitialized`, `Bus`, `UnexpectedReset`. */
  Result<uint32_t> Identify() noexcept;
  /** @return One pipelined batch (GSTAT, DRV_STATUS, RAMP_STAT, XACTUAL, VACTUAL, TSTEP, MSCNT). */
  Result<Snapshot> ReadSnapshot() noexcept;

  /**
   * @brief After a DIAG0 interrupt: read GSTAT, DRV_STATUS, RAMP_STAT and ENC_STATUS and clear the
   * events that were set.
   * @return What was found, or `NotInitialized`, `Bus`, `UnexpectedReset` (the reset flag of
   * GSTAT).
   */
  Result<Events> Service() noexcept;

  /**
   * @brief Read one readable register once, with no decoding (bring-up).
   * @param address A readable address (write-only registers are refused).
   * @return The value, or `NotInitialized`, `Refused` (write-only or absent; nothing sent), `Bus`,
   * `UnexpectedReset`.
   */
  Result<uint32_t> ReadRegister(uint8_t address) noexcept;

  /** @return DIAG0 level, active meaning the pin is low (open drain); `Unsupported` without
   * `GpioGet()`. */
  Result<bool> Diag0Active() noexcept;
  /** @return DIAG1 level, active meaning the pin is low; `Unsupported` without `GpioGet()`. */
  Result<bool> Diag1Active() noexcept;

  /** @return The SPI status byte of the last transfer (bit 0 reset, 1 driver_error, 2 sg2, 3
   * standstill, 4 velocity_reached, 5 position_reached, 6 status_stop_l, 7 status_stop_r); one
   * transfer old. 0 for UART. No I/O. */
  [[nodiscard]] uint8_t GetLastStatusByte() const noexcept {
    return last_status_;
  }

  /// @}
  /// @name UART nodes
  /// @{

  /**
   * @brief Program another node address into the chip at `from_node` (UART only), and set the NAO
   * level.
   * @param from_node The address the chip answers to now.
   * @param new_node The address to program, 0 to 254 (254 only in the addressing procedure, page
   * 29).
   * @param nao_level The level of SDO_CFG0/NAO to the next chip (OUTPUT register).
   * @return `Ok`, `NotInitialized`, `Unsupported` (SPI), `InvalidArgument`, `Bus`, `Timeout`,
   * `Crc`.
   */
  Result<void> ProgramNode(uint8_t from_node, uint8_t new_node, bool nao_level) noexcept;

  /// @}
  /// @name Configuration
  /// @{

  /** @return The configuration in force. No I/O. */
  [[nodiscard]] const Config& GetConfig() const& noexcept {
    return config_;
  }
  /** @brief Not on a temporary driver: the reference would dangle (KB-CPP20-044). */
  void GetConfig() const&& = delete;

  /// @}

private:
  static constexpr bool kSpi = SpiPort<Port>;

  Result<void> initialize() noexcept;
  Result<void> checkLink(const Config& config) noexcept;
  Result<void> clearGstat() noexcept;
  Result<void> startUp(const Config& config) noexcept;
  Result<void> writeTable(const Config& config) noexcept;
  Result<void> writeAll(const Config& config) noexcept;
  Result<void> finishTuning() noexcept;
  Result<void> ready() const noexcept;
  Result<void> readyMotion() const noexcept;
  Result<void> readyController() const noexcept;
  Result<void> guard(const Config& next) const noexcept;
  Result<void> quiesce(Verify verify = Verify::None) noexcept;
  Result<void> checkIntact() noexcept;
  /// Notes a chip reset: not initialised any more, but the switch-off stays owed.
  void noteReset() noexcept;
  Result<void> readyOff() const noexcept;
  Result<void> readyStop() const noexcept;
  /// Direct mode: XTARGET holds the coil currents, so a stop writes 0 there.
  Result<void> zeroCoils(Verify verify) noexcept;
  /// Brings a ramp generator left running to a stop and records a latched stall stop (start-up).
  Result<void> stopRamp(Verify verify) noexcept;
  /// Puts the motor on hold when a switch change would release a latched stop event.
  Result<void> holdIfReleasing(const Switches& next) noexcept;
  [[nodiscard]] static constexpr Verify offVerify(Verify verify) noexcept;
  /// A verification the bus cannot do becomes the bus's own (a stop is never refused for it).
  [[nodiscard]] static constexpr Verify busVerify(Verify verify) noexcept;
  /// The writes that bring the ramp generator to a stop in velocity mode (VMAX 0), after XTARGET 0
  /// when the coil currents may be in XTARGET (direct mode, or the mode in doubt).
  Result<void> rampStop(bool immediate, bool zero_coils, Verify verify) noexcept;
  /// A sequence that must reach its last write ignores a reset report until then.
  [[nodiscard]] bool beginSequence() noexcept;
  Result<void> endSequence(bool watched, Result<void> result) noexcept;
  Result<void> fail(Error error, const char* message) noexcept;
  [[nodiscard]] uint32_t vmaxLimit() const noexcept;

  Error datagram(uint8_t cmd, uint32_t value, uint32_t& reply) noexcept;
  Error observe(uint8_t status) noexcept;
  Error uartRead(uint8_t reg, uint32_t& value) noexcept;
  Error uartWrite(uint8_t reg, uint32_t value) noexcept;
  Error uartExchange(const detail::UartFrame& tx, std::size_t tx_len, detail::UartFrame& rx,
                     std::size_t rx_len) noexcept;
  Error readMany(std::span<const uint8_t> addresses, std::span<uint32_t> out) noexcept;
  Result<uint32_t> readReg(uint8_t address) noexcept;
  Result<uint32_t> readChecked(uint8_t address) noexcept;
  Result<uint32_t> readControlled(uint8_t address) noexcept;
  Result<void> sendReg(uint8_t address, uint32_t value, Verify verify, uint32_t& word,
                       uint32_t& reply) noexcept;
  Result<void> writeReg(uint8_t address, uint32_t value, Verify verify) noexcept;
  Result<void> writeRegs(std::span<const detail::Write> writes, Verify verify) noexcept;
  Result<void> verifyWrite(uint8_t address, uint32_t word, Verify verify) noexcept;
  Result<void> checkVerify(Verify verify) const noexcept;
  Result<void> setPin(CtrlPin pin, bool active) noexcept;
  Result<void> ensureStopped() noexcept;
  [[nodiscard]] bool stuckLow(uint32_t value) const noexcept;
  /// `Bus` when `value` and the status byte are 0 and IOIN.VERSION is wrong too (SPI only).
  [[nodiscard]] Error lineCheck(uint32_t value) noexcept;
  Result<void> writeChopconf(Verify verify) noexcept;
  /// CHOPCONF with the bridge marked off (TOFF 0), without the intact check of `pushConfig()`.
  Result<void> writeOff(Verify verify) noexcept;
  /// `pushConfig()` checks the chip before a write that switches the bridge on, then sends.
  Result<void> pushConfig(const Config& next, std::span<const uint8_t> which,
                          Verify verify) noexcept;
  /// The registers of `which` from `next`, written and taken into `config_` on success.
  Result<void> sendConfig(const Config& next, std::span<const uint8_t> which,
                          Verify verify) noexcept;
  Result<void> setField(Config next, std::span<const uint8_t> which, Verify verify) noexcept;
  Result<void> clearEvents(const Events& ev) noexcept;
  Result<bool> pinActive(InputPin pin, bool push_pull) noexcept;
  void waitUartRecovery() noexcept;

  Port& port_;
  Config config_{};
  units::CurrentPlan plan_{};
  BeginReport report_{};
  std::array<uint32_t, 128> shadow_{};
  /// Transfer buffers: owned by the driver, never on the stack, so a port with DMA can reach them.
  std::array<uint8_t, (std::size_t{kDatagramBytes} * kMaxChain)> spi_tx_{};
  std::array<uint8_t, (std::size_t{kDatagramBytes} * kMaxChain)> spi_rx_{};
  detail::UartFrame uart_tx_{};
  detail::UartFrame uart_rx_{};
  uint32_t uart_counter_{0};
  uint8_t node_{0};
  uint8_t last_status_{0};
  bool initialized_{false};
  bool enabled_{false};
  bool fault_{false};
  bool watch_reset_{false};
  bool reset_seen_{
      false}; ///< A status byte carried the reset flag (a sequence reports it at its end).
  bool intact_checked_{false}; ///< `Enable()` has just checked the chip: no second check for TOFF.
  bool direct_mode_{false};
  bool direct_unsure_{false}; ///< A GCONF write for direct mode failed: the
                              ///< chip may be in either mode.
  bool started_{false};       ///< A start-up sent its first write: a switch-off is owed until done.
  bool off_pending_{false};   ///< A switch-off was attempted or lost and is not confirmed.
  bool ramp_written_{false};
  bool hold_at_run_{false};
};

/** @} */

} // namespace tmc5160

// codeql::misra_deviation_next_line(MD-CPP-009)
#include "../src/tmc5160.ipp" // IWYU pragma: export

#endif // TMC5160_HPP
