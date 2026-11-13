/**
 * @file tmc5160_units.hpp
 * @brief Pure integer conversions between datasheet register units and physical units, and the
 * current plan that ties the coil current to the declared motor and sense resistor.
 * @copyright Copyright (c) 2026 The driver-tmc5160 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Every function is `constexpr`, integer only, allocates nothing and does no I/O. All times,
 * velocities and accelerations scale with the clock frequency `fclk_hz` (section 12.1, page 81).
 * Products that can exceed 64 bits are split in two steps; the result is within one unit of the
 * exact quotient.
 */
#ifndef TMC5160_UNITS_HPP
#define TMC5160_UNITS_HPP

#include <array>
#include <cstdint>

#include "tmc5160_types.hpp"

namespace tmc5160::units {

/// 1/sqrt(2) in 16.16 fixed point (46341 / 65536).
inline constexpr uint64_t kInvSqrt2Q16 = 46'341;

/**
 * @brief The RMS coil current for a sense resistor, GLOBALSCALER and current scale (section 9
 * page 74): `GS/256 * CS/32 * VFS / RSENSE / sqrt(2)`, rounded down.
 * @param rsense_mohm The sense resistor in milliohm, 1 or more.
 * @param global_scaler GLOBALSCALER as a divisor numerator, 32 to 256 (register 0 means 256).
 * @param cs_plus_1 IRUN or IHOLD plus one, 1 to 32.
 * @return The current in mA; 0 for a zero sense resistor.
 */
[[nodiscard]] constexpr uint32_t RmsMa(uint32_t rsense_mohm, uint32_t global_scaler,
                                       uint32_t cs_plus_1) noexcept {
  if (rsense_mohm == 0U) {
    return 0U;
  }
  const uint64_t num = uint64_t{kVfsMv} * 1000U * global_scaler * cs_plus_1 * kInvSqrt2Q16;
  const uint64_t den = uint64_t{rsense_mohm} * 256U * 32U * 65'536U;
  return static_cast<uint32_t>(num / den);
}

/** @brief The registers a current request turns into (page 36, 38). */
struct CurrentPlan {
  uint32_t global_scaler{0};     ///< GLOBALSCALER as a numerator, 129 to 256.
  uint32_t global_scaler_reg{0}; ///< The register value: 0 stands for 256.
  uint32_t irun{0};              ///< IRUN 0 to 31.
  uint32_t ihold{0};             ///< IHOLD 0 to 31 (0 is 1/32 of full scale, not zero current).
  uint32_t run_ma{0};        ///< The nominal run current the registers give, in mA, rounded down.
  uint32_t hold_ma{0};       ///< The nominal hold current the registers give, in mA.
  uint32_t full_scale_ma{0}; ///< The current at IRUN 31 with this GLOBALSCALER.
};

namespace detail {
/// The largest `cs_plus_1` in 1..32 whose current is at most `target_ma`; 0 when even 1 is too
/// much.
[[nodiscard]] constexpr uint32_t ScaleFor(uint32_t rsense_mohm, uint32_t gs,
                                          uint32_t target_ma) noexcept {
  uint32_t best = 0U;
  for (uint32_t cs = 1U; cs <= 32U; ++cs) {
    if (RmsMa(rsense_mohm, gs, cs) <= target_ma) {
      best = cs;
    }
  }
  return best;
}

/// The current ceiling in mA, reduced by the full-scale tolerance when asked (page 123).
[[nodiscard]] constexpr uint32_t Ceiling(const Config& c) noexcept {
  uint32_t hard = c.motor.rated_rms_ma;
  if ((c.motor.max_rms_ma != 0U) && (c.motor.max_rms_ma < hard)) {
    hard = c.motor.max_rms_ma;
  }
  return c.include_tolerance ? ((hard * 100U) / (100U + kCurrentTolerancePercent)) : hard;
}
} // namespace detail

/**
 * @brief Turn a run and hold current request into register values, or refuse it.
 *
 * The run and hold current, rounded down, must stay at or under the ceiling `min(max_rms_ma,
 * rated_rms_ma)`, reduced by the 5 percent tolerance when `include_tolerance` is set (section 9
 * page 74, 28.2 page 123), and the sense resistor must be able to carry the run current.
 * GLOBALSCALER is chosen from 129 to 256 (page 36 recommends above 128).
 *
 * @param config The configuration (motor, board, tolerance, acknowledgements).
 * @param run_ma Requested run current, RMS mA.
 * @param hold_ma Requested hold current, RMS mA.
 * @return The plan, `LimitExceeded` (ceiling or sense resistor), `Refused` (hold above run without
 *         `Ack::HoldAboveRun`) or `InvalidArgument` (a zero request or a missing sense resistor).
 */
[[nodiscard]] constexpr Result<CurrentPlan> PlanCurrent(const Config& config, uint32_t run_ma,
                                                        uint32_t hold_ma) noexcept {
  if ((run_ma == 0U) || (config.board.rsense_mohm == 0U)) {
    return Result<CurrentPlan>::Failure(Error::InvalidArgument);
  }
  if ((hold_ma > run_ma) && !config.ack.Has(Ack::HoldAboveRun)) {
    return Result<CurrentPlan>::Failure(Error::Refused);
  }
  const uint32_t ceiling = detail::Ceiling(config);
  const uint32_t highest = (hold_ma > run_ma) ? hold_ma : run_ma;
  const uint32_t fs256 = RmsMa(config.board.rsense_mohm, 256U, 32U);
  if ((highest > ceiling) || (highest > fs256)) {
    return Result<CurrentPlan>::Failure(Error::LimitExceeded);
  }
  uint32_t gs = 129U;
  while ((gs < 256U) && (RmsMa(config.board.rsense_mohm, gs, 32U) < highest)) {
    ++gs;
  }
  const uint32_t cs_run = detail::ScaleFor(config.board.rsense_mohm, gs, run_ma);
  const uint32_t cs_hold = detail::ScaleFor(config.board.rsense_mohm, gs, hold_ma);
  if (cs_run == 0U) {
    return Result<CurrentPlan>::Failure(Error::LimitExceeded);
  }
  CurrentPlan p{};
  p.global_scaler = gs;
  p.global_scaler_reg = (gs == 256U) ? 0U : gs;
  p.irun = cs_run - 1U;
  p.ihold = (cs_hold == 0U) ? 0U : (cs_hold - 1U);
  p.run_ma = RmsMa(config.board.rsense_mohm, gs, cs_run);
  p.hold_ma = RmsMa(config.board.rsense_mohm, gs, p.ihold + 1U);
  p.full_scale_ma = RmsMa(config.board.rsense_mohm, gs, 32U);
  return Result<CurrentPlan>::Success(p);
}

/**
 * @brief The StealthChop lower current limit for automatic tuning (section 7.3.1 page 62):
 * `tBLANK * fPWM * VM / RCOIL`.
 * @param tbl_code CHOPCONF.TBL 0 to 3 (16, 24, 36, 54 clocks).
 * @param pwm_freq_code PWMCONF.pwm_freq 0 to 3 (2/1024, 2/683, 2/512, 2/410 of fCLK).
 * @param supply_mv Motor supply in mV.
 * @param coil_resistance_mohm Coil resistance in mOhm; 0 gives 0 (unknown).
 * @return The limit in mA.
 */
[[nodiscard]] constexpr uint32_t StealthChopLowerLimitMa(uint32_t tbl_code, uint32_t pwm_freq_code,
                                                         uint32_t supply_mv,
                                                         uint32_t coil_resistance_mohm) noexcept {
  constexpr std::array<uint32_t, 4> kBlank{16U, 24U, 36U, 54U};
  constexpr std::array<uint32_t, 4> kPeriod{1024U, 683U, 512U, 410U};
  if (coil_resistance_mohm == 0U) {
    return 0U;
  }
  const uint64_t num = uint64_t{supply_mv} * 1000U * kBlank[tbl_code & 3U] * 2U;
  return static_cast<uint32_t>(num /
                               (uint64_t{coil_resistance_mohm} * kPeriod[pwm_freq_code & 3U]));
}

/**
 * @brief How long StealthChop's first tuning phase AT#1 can take: the chip must stand still for
 * `2^20 + 2 * 2^18` clocks (page 58), plus 10 percent.
 * @param fclk_hz The clock frequency.
 * @return Microseconds.
 */
[[nodiscard]] constexpr uint32_t StealthChopAt1Us(uint32_t fclk_hz) noexcept {
  if (fclk_hz == 0U) {
    return 0U;
  }
  const uint64_t clocks = (uint64_t{1} << 20U) + (uint64_t{2} << 18U);
  return static_cast<uint32_t>(((clocks * 1'100'000U) / fclk_hz));
}

/**
 * @brief The StealthChop guard of design section 2.5: with automatic scaling the run current scale
 * IRUN must be 8 or more (page 62) and, when the coil resistance is declared, the nominal run
 * current must reach the lower limit (page 62).
 * @param config The configuration.
 * @param plan The current plan.
 * @return `Ok` or `Refused`.
 */
[[nodiscard]] constexpr Error StealthGuard(const Config& config, const CurrentPlan& plan) noexcept {
  if (!(config.stealth.enable && config.stealth.autoscale)) {
    return Error::Ok;
  }
  const uint32_t lower =
      StealthChopLowerLimitMa(config.chopper.tbl, config.stealth.freq, config.board.supply_mv,
                              config.motor.coil_resistance_mohm);
  return ((plan.irun >= 8U) && (plan.run_ma >= lower)) ? Error::Ok : Error::Refused;
}

/**
 * @brief A velocity in microsteps per second as a VMAX-style register value: `v * 2^24 / fCLK`.
 * @param hz Velocity in microsteps per second (256 per full step at the finest setting).
 * @param fclk_hz The clock frequency.
 * @return The register value, or `InvalidArgument` for a zero clock or a value above 32 bits.
 */
[[nodiscard]] constexpr Result<uint32_t> VelocityRegFromHz(uint32_t hz, uint32_t fclk_hz) noexcept {
  if (fclk_hz == 0U) {
    return Result<uint32_t>::Failure(Error::InvalidArgument);
  }
  const uint64_t reg = (uint64_t{hz} << 24U) / fclk_hz;
  return (reg > 0xFFFF'FFFFU) ? Result<uint32_t>::Failure(Error::InvalidArgument)
                              : Result<uint32_t>::Success(static_cast<uint32_t>(reg));
}

/**
 * @brief The velocity a register value stands for: `v_reg * fCLK / 2^24` (page 81).
 * @param velocity_reg The register value.
 * @param fclk_hz The clock frequency.
 * @return Microsteps per second.
 */
[[nodiscard]] constexpr uint32_t HzFromVelocityReg(uint32_t velocity_reg,
                                                   uint32_t fclk_hz) noexcept {
  return static_cast<uint32_t>((uint64_t{velocity_reg} * fclk_hz) >> 24U);
}

/**
 * @brief An acceleration in microsteps per second squared as a register value: `a * 2^41 / fCLK^2`.
 * @param hz_per_s The acceleration.
 * @param fclk_hz The clock frequency.
 * @return The register value (within 1 of exact), or `InvalidArgument` for a zero clock or an
 *         acceleration above 2^24 - 1.
 */
[[nodiscard]] constexpr Result<uint32_t> AccelRegFromHzPerS(uint32_t hz_per_s,
                                                            uint32_t fclk_hz) noexcept {
  if ((fclk_hz == 0U) || (hz_per_s > 0xFFFFFFU)) {
    return Result<uint32_t>::Failure(Error::InvalidArgument);
  }
  const uint64_t step1 = (uint64_t{hz_per_s} << 20U) / fclk_hz;
  return Result<uint32_t>::Success(static_cast<uint32_t>((step1 << 21U) / fclk_hz));
}

/**
 * @brief The acceleration a register value stands for: `a_reg * fCLK^2 / 2^41`.
 * @param accel_reg The register value, 0 to 65535.
 * @param fclk_hz The clock frequency.
 * @return Microsteps per second squared.
 */
[[nodiscard]] constexpr uint32_t HzPerSFromAccelReg(uint32_t accel_reg, uint32_t fclk_hz) noexcept {
  const uint64_t q = (uint64_t{accel_reg} * fclk_hz) >> 20U;
  return static_cast<uint32_t>((q * fclk_hz) >> 21U);
}

/**
 * @brief TSTEP for a velocity at 256 microsteps: `2^24 / v` (page 38, A23).
 * @param velocity_reg VACTUAL-style velocity.
 * @return TSTEP, `0xFFFFF` for velocity 0 and capped at 20 bits.
 */
[[nodiscard]] constexpr uint32_t StepTimeFromVelocity(uint32_t velocity_reg) noexcept {
  if (velocity_reg == 0U) {
    return 0xFFFFFU;
  }
  const uint32_t t = (1U << 24U) / velocity_reg;
  return (t > 0xFFFFFU) ? 0xFFFFFU : t;
}

/**
 * @brief The velocity a TSTEP value stands for: `2^24 / TSTEP`.
 * @param tstep TSTEP; 0 gives 0.
 * @return The VACTUAL-style velocity.
 */
[[nodiscard]] constexpr uint32_t VelocityFromStepTime(uint32_t tstep) noexcept {
  return (tstep == 0U) ? 0U : ((1U << 24U) / tstep);
}

/**
 * @brief Microsteps a ramp from rest to `v` takes with acceleration `a`: `v^2 / a / 2^8` (page 81).
 * @param velocity_reg Final velocity register value.
 * @param accel_reg Acceleration register value, 1 or more; 0 gives 0.
 * @return The number of microsteps.
 */
[[nodiscard]] constexpr uint32_t RampSteps(uint32_t velocity_reg, uint32_t accel_reg) noexcept {
  return (accel_reg == 0U)
             ? 0U
             : static_cast<uint32_t>((uint64_t{velocity_reg} * velocity_reg) / accel_reg / 256U);
}

/**
 * @brief Time of a ramp from rest to `v` with acceleration `a`, in microseconds: the velocity
 * changes by `a / 256` per update tick of 512 clocks (section 12.1).
 * @param velocity_reg Final velocity register value.
 * @param accel_reg Acceleration register value; 0 gives 0.
 * @param fclk_hz The clock frequency.
 * @return The time in microseconds.
 */
[[nodiscard]] constexpr uint64_t RampTimeUs(uint32_t velocity_reg, uint32_t accel_reg,
                                            uint32_t fclk_hz) noexcept {
  if ((accel_reg == 0U) || (fclk_hz == 0U)) {
    return 0U;
  }
  return (uint64_t{velocity_reg} * 256U * 512U * 1'000'000U) / (uint64_t{accel_reg} * fclk_hz);
}

/**
 * @brief Chopper off time: `24 + 32 * TOFF` clocks (page 51).
 * @param toff TOFF 1 to 15.
 * @return Clocks.
 */
[[nodiscard]] constexpr uint32_t ToffClocks(uint32_t toff) noexcept {
  return 24U + (32U * (toff & 15U));
}

/**
 * @brief Chopper off time in nanoseconds.
 * @param toff TOFF.
 * @param fclk_hz The clock frequency, 1 or more.
 * @return Nanoseconds.
 */
[[nodiscard]] constexpr uint32_t OffTimeNs(uint32_t toff, uint32_t fclk_hz) noexcept {
  return (fclk_hz == 0U)
             ? 0U
             : static_cast<uint32_t>((uint64_t{ToffClocks(toff)} * 1'000'000'000U) / fclk_hz);
}

/**
 * @brief TPOWERDOWN delay: 2^18 clocks per count (page 38).
 * @param tpowerdown The register value 0 to 255.
 * @param fclk_hz The clock frequency.
 * @return Microseconds.
 */
[[nodiscard]] constexpr uint64_t PowerDownUs(uint32_t tpowerdown, uint32_t fclk_hz) noexcept {
  return (fclk_hz == 0U) ? 0U : ((uint64_t{tpowerdown & 255U} << 18U) * 1'000'000U) / fclk_hz;
}

/**
 * @brief IHOLDDELAY per current step: 2^18 clocks per count (page 38).
 * @param hold_delay The register value 0 to 15 (0 is treated as 1 by the chip).
 * @param fclk_hz The clock frequency.
 * @return Microseconds.
 */
[[nodiscard]] constexpr uint64_t HoldDelayUs(uint32_t hold_delay, uint32_t fclk_hz) noexcept {
  return (fclk_hz == 0U) ? 0U : ((uint64_t{hold_delay & 15U} << 18U) * 1'000'000U) / fclk_hz;
}

/**
 * @brief TZEROWAIT: 512 clocks per count (page 41).
 * @param zero_wait The register value 0 to 65535.
 * @param fclk_hz The clock frequency.
 * @return Microseconds.
 */
[[nodiscard]] constexpr uint64_t ZeroWaitUs(uint32_t zero_wait, uint32_t fclk_hz) noexcept {
  return (fclk_hz == 0U) ? 0U : (uint64_t{zero_wait & 0xFFFFU} * 512U * 1'000'000U) / fclk_hz;
}

/**
 * @brief The velocity VDCMIN stands for: the same scale as VMAX (page 42).
 * @param vdcmin The register value.
 * @param fclk_hz The clock frequency.
 * @return Microsteps per second.
 */
[[nodiscard]] constexpr uint32_t DcStepVelocity(uint32_t vdcmin, uint32_t fclk_hz) noexcept {
  return HzFromVelocityReg(vdcmin, fclk_hz);
}

/**
 * @brief The ENC_CONST value for an encoder resolution (section 20 page 107): the factor that turns
 * `counts` encoder counts per revolution into `microsteps` microsteps per revolution.
 *
 * Binary: `microsteps / counts` in 16.16, negative values as two's complement. Decimal: the
 * integer part and the fraction in 1/10000; a negative value follows the page 107 rule (the
 * fraction is subtracted from the integer part).
 *
 * @param microsteps Microsteps per motor revolution, 1 or more.
 * @param counts Encoder counts per revolution (4 x the pulses), 1 or more.
 * @param decimal `true` for the decimal register format.
 * @param negative `true` when the encoder counts against the motor.
 * @return The scale, or `InvalidArgument` for a zero argument or a ratio above 32767.
 */
[[nodiscard]] constexpr Result<EncoderScale> EncoderScaleFor(uint32_t microsteps, uint32_t counts,
                                                             bool decimal, bool negative) noexcept {
  if ((microsteps == 0U) || (counts == 0U) || ((microsteps / counts) > 32'767U)) {
    return Result<EncoderScale>::Failure(Error::InvalidArgument);
  }
  const uint64_t unit = decimal ? 10'000U : 65'536U;
  const uint64_t scaled = (uint64_t{microsteps} * unit) / counts;
  EncoderScale s{};
  s.decimal = decimal;
  s.integer = static_cast<int16_t>(scaled / unit);
  s.fraction = static_cast<uint16_t>(scaled % unit);
  s.negative = negative;
  return Result<EncoderScale>::Success(s);
}

/**
 * @brief The 32-bit ENC_CONST register word for a scale: high 16 bits integer, low 16 bits
 * fraction. A negative binary constant is the two's complement of the 16.16 value; a negative
 * decimal constant has the integer part decremented by one and the fraction `10000 - fraction`
 * (page 107).
 * @param scale The scale.
 * @return The register word.
 */
[[nodiscard]] constexpr uint32_t EncConstWord(const EncoderScale& scale) noexcept {
  if (!scale.negative) {
    return (static_cast<uint32_t>(static_cast<uint16_t>(scale.integer)) << 16U) | scale.fraction;
  }
  if (scale.decimal) {
    const uint32_t frac = (scale.fraction == 0U) ? 0U : (10'000U - scale.fraction);
    const int32_t whole = (scale.fraction == 0U) ? -static_cast<int32_t>(scale.integer)
                                                 : -static_cast<int32_t>(scale.integer) - 1;
    return (static_cast<uint32_t>(static_cast<uint16_t>(static_cast<int16_t>(whole))) << 16U) |
           frac;
  }
  const uint32_t magnitude =
      (static_cast<uint32_t>(static_cast<uint16_t>(scale.integer)) << 16U) | scale.fraction;
  return 0U - magnitude;
}

} // namespace tmc5160::units

#endif // TMC5160_UNITS_HPP
