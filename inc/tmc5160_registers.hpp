/**
 * @file tmc5160_registers.hpp
 * @brief TMC5160 register map: addresses, access rules, reserved bits and reset values.
 * @copyright Copyright (c) 2026 The driver-tmc5160 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Datasheet chapter 6 (Rev. 1.18), pages 31 to 56. Addresses are seven bits. Most registers are
 * write-only (the datasheet marks them W, section 1.3 page 7 asks the master to keep a shadow), so
 * the table below records, for every address, whether the chip lets the driver read it, whether the
 * driver may write it, the bits that exist (every other bit is written as 0) and the bits that
 * clear on a write of 1. The driver refuses a read of a write-only address and a write of a
 * read-only one.
 */
#ifndef TMC5160_REGISTERS_HPP
#define TMC5160_REGISTERS_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "tmc5160_detail.hpp"

/** @brief The TMC5160 register map. */
namespace tmc5160::reg {

/**
 * @defgroup tmc5160_registers Register map
 * @brief Addresses, fields and reset values.
 * @{
 */

inline constexpr uint8_t kGconf = 0x00;    ///< GCONF (RW, 18 bits), page 32.
inline constexpr uint8_t kGstat = 0x01;    ///< GSTAT (R, write 1 to clear, 3 bits), page 33.
inline constexpr uint8_t kIfcnt = 0x02;    ///< IFCNT (R, 8 bits): UART write counter, page 33.
inline constexpr uint8_t kNodeconf = 0x03; ///< NODECONF (W, 12 bits): UART node, page 34.
inline constexpr uint8_t kIoin = 0x04;     ///< IOIN (R): input pins and VERSION, page 34.
inline constexpr uint8_t kOutput = 0x04;   ///< OUTPUT (W, 1 bit): NAO level in UART mode, page 34.
inline constexpr uint8_t kXCompare = 0x05; ///< X_COMPARE (W, 32 bits), page 34.
inline constexpr uint8_t kOtpProg = 0x06;  ///< OTP_PROG (W): never written by the driver.
inline constexpr uint8_t kOtpRead = 0x07;  ///< OTP_READ (R, 8 bits), page 35.
inline constexpr uint8_t kFactoryConf = 0x08;  ///< FACTORY_CONF (RW): never written by the driver.
inline constexpr uint8_t kShortConf = 0x09;    ///< SHORT_CONF (W, 19 bits), page 35.
inline constexpr uint8_t kDrvConf = 0x0A;      ///< DRV_CONF (W, 22 bits), page 35.
inline constexpr uint8_t kGlobalScaler = 0x0B; ///< GLOBALSCALER (W, 8 bits), page 36.
inline constexpr uint8_t kOffsetRead = 0x0C;   ///< OFFSET_READ (R, 16 bits), page 36.
inline constexpr uint8_t kIholdIrun = 0x10;    ///< IHOLD_IRUN (W, 20 bits), page 38.
inline constexpr uint8_t kTpowerdown = 0x11;   ///< TPOWERDOWN (W, 8 bits), page 38.
inline constexpr uint8_t kTstep = 0x12;        ///< TSTEP (R, 20 bits), page 38.
inline constexpr uint8_t kTpwmthrs = 0x13;     ///< TPWMTHRS (W, 20 bits), page 39.
inline constexpr uint8_t kTcoolthrs = 0x14;    ///< TCOOLTHRS (W, 20 bits), page 39.
inline constexpr uint8_t kThigh = 0x15;        ///< THIGH (W, 20 bits), page 39.
inline constexpr uint8_t kRampmode = 0x20;     ///< RAMPMODE (RW, 2 bits), page 40.
inline constexpr uint8_t kXactual = 0x21;      ///< XACTUAL (RW, 32 bits), page 40.
inline constexpr uint8_t kVactual = 0x22;      ///< VACTUAL (R, 24 bits), page 40.
inline constexpr uint8_t kVstart = 0x23;       ///< VSTART (W, 18 bits), page 40.
inline constexpr uint8_t kA1 = 0x24;           ///< A1 (W, 16 bits), page 40.
inline constexpr uint8_t kV1 = 0x25;           ///< V1 (W, 20 bits), page 40.
inline constexpr uint8_t kAmax = 0x26;         ///< AMAX (W, 16 bits), page 40.
inline constexpr uint8_t kVmax = 0x27;         ///< VMAX (W, 23 bits), page 40.
inline constexpr uint8_t kDmax = 0x28;         ///< DMAX (W, 16 bits), page 40.
inline constexpr uint8_t kD1 = 0x2A;           ///< D1 (W, 16 bits), page 40.
inline constexpr uint8_t kVstop = 0x2B;        ///< VSTOP (W, 18 bits), page 41.
inline constexpr uint8_t kTzerowait = 0x2C;    ///< TZEROWAIT (W, 16 bits), page 41.
inline constexpr uint8_t kXtarget = 0x2D;      ///< XTARGET (RW, 32 bits), page 41.
inline constexpr uint8_t kVdcmin = 0x33;       ///< VDCMIN (W, 23 bits), page 42.
inline constexpr uint8_t kSwMode = 0x34;       ///< SW_MODE (RW, 12 bits), page 42.
inline constexpr uint8_t kRampStat = 0x35;     ///< RAMP_STAT (R, write 1 to clear some, 14 bits).
inline constexpr uint8_t kXlatch = 0x36;       ///< XLATCH (R, 32 bits), page 42.
inline constexpr uint8_t kEncmode = 0x38;      ///< ENCMODE (RW, 11 bits), page 45.
inline constexpr uint8_t kXEnc = 0x39;         ///< X_ENC (RW, 32 bits), page 45.
inline constexpr uint8_t kEncConst = 0x3A;     ///< ENC_CONST (W, 32 bits), page 45.
inline constexpr uint8_t kEncStatus = 0x3B;    ///< ENC_STATUS (R, write 1 to clear, 2 bits).
inline constexpr uint8_t kEncLatch = 0x3C;     ///< ENC_LATCH (R, 32 bits), page 45.
inline constexpr uint8_t kEncDeviation = 0x3D; ///< ENC_DEVIATION (W, 20 bits), page 45.
inline constexpr uint8_t kMslut0 = 0x60;       ///< MSLUT[0] (W); MSLUT[n] is at kMslut0 + n, n < 8.
inline constexpr uint8_t kMslutsel = 0x68;     ///< MSLUTSEL (W, 32 bits), page 47.
inline constexpr uint8_t kMslutstart = 0x69;   ///< MSLUTSTART (W, 24 bits), page 47.
inline constexpr uint8_t kMscnt = 0x6A;        ///< MSCNT (R, 10 bits), page 47.
inline constexpr uint8_t kMscuract = 0x6B;     ///< MSCURACT (R, 25 bits), page 47.
inline constexpr uint8_t kChopconf = 0x6C;     ///< CHOPCONF (RW, 32 bits), page 48.
inline constexpr uint8_t kCoolconf = 0x6D;     ///< COOLCONF (W, 25 bits), page 48.
inline constexpr uint8_t kDcctrl = 0x6E;       ///< DCCTRL (W, 24 bits), page 48.
inline constexpr uint8_t kDrvStatus = 0x6F;    ///< DRV_STATUS (R, 32 bits), page 48.
inline constexpr uint8_t kPwmconf = 0x70;      ///< PWMCONF (W, 32 bits), page 48.
inline constexpr uint8_t kPwmScale = 0x71;     ///< PWM_SCALE (R, 17 bits), page 48.
inline constexpr uint8_t kPwmAuto = 0x72;      ///< PWM_AUTO (R, 16 bits), page 48.
inline constexpr uint8_t kLostSteps = 0x73;    ///< LOST_STEPS (R, 20 bits), page 49.
inline constexpr uint8_t kLastRegister = 0x73; ///< Highest implemented address.

/// Write flag of the first byte of an SPI datagram (section 4.1, page 23).
inline constexpr uint8_t kWriteFlag = 0x80;

/** @brief How the driver may touch an address. */
enum class Access : uint8_t {
  ReadOnly = 0,  ///< The chip only reads back; a write is refused.
  WriteOnly = 1, ///< The chip never reads it back; the driver keeps the shadow.
  ReadWrite = 2, ///< Read and written; the chip may change bits on its own.
  Clear = 3,     ///< Read; a write of 1 clears the bits in `w1c`.
  Forbidden = 4, ///< Exists on the chip, never written by the driver (OTP_PROG, FACTORY_CONF).
};

/** @brief What the driver knows about one register. */
struct RegInfo {
  uint8_t address; ///< Seven-bit address.
  Access access;   ///< How it may be touched.
  uint32_t mask;   ///< Bits that exist; the driver writes every other bit as 0.
  uint32_t w1c;    ///< Bits a write of 1 clears (Clear access only).
  uint32_t reset;  ///< Value after a power-up, section 6 page 31.
  bool is_output;  ///< `true` for OUTPUT, the write side of address 0x04.
};

/// Default sine table words, MSLUT[0] to MSLUT[7] (page 106).
inline constexpr std::array<uint32_t, 8> kDefaultMslut{0xAAAAB554U, 0x4A9554AAU, 0x24492929U,
                                                       0x10104222U, 0xFBFFFFFFU, 0xB5BB777DU,
                                                       0x49295556U, 0x00404222U};
/// Reset value of MSLUTSEL (page 47).
inline constexpr uint32_t kDefaultMslutsel = 0xFFFF8056U;
/// Reset value of MSLUTSTART: START_SIN 0, START_SIN90 247 (page 47).
inline constexpr uint32_t kDefaultMslutstart = 0x00F70000U;
/// Reset value of CHOPCONF (page 48).
inline constexpr uint32_t kChopconfReset = 0x10410150U;
/// Reset value of PWMCONF (page 48).
inline constexpr uint32_t kPwmconfReset = 0xC40C001EU;
/// Reset value of ENC_CONST, 1.0 in 16.16 (page 45).
inline constexpr uint32_t kEncConstReset = 0x00010000U;
/// The chip version IOIN reports in bits 31 to 24 (page 34).
inline constexpr uint32_t kVersion = 0x30U;

/// The table: one row per address and direction, in address order.
inline constexpr std::array<RegInfo, 63> kTable{{
    {.address = kGconf,
     .access = Access::ReadWrite,
     .mask = 0x0003FFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kGstat,
     .access = Access::Clear,
     .mask = 0x7U,
     .w1c = 0x7U,
     .reset = 0x1U,
     .is_output = false},
    {.address = kIfcnt,
     .access = Access::ReadOnly,
     .mask = 0xFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kNodeconf,
     .access = Access::WriteOnly,
     .mask = 0xFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kIoin,
     .access = Access::ReadOnly,
     .mask = 0xFF0000FFU,
     .w1c = 0U,
     .reset = 0x30000000U,
     .is_output = false},
    {.address = kOutput,
     .access = Access::WriteOnly,
     .mask = 0x1U,
     .w1c = 0U,
     .reset = 0x1U,
     .is_output = true},
    {.address = kXCompare,
     .access = Access::WriteOnly,
     .mask = 0xFFFFFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kOtpProg,
     .access = Access::Forbidden,
     .mask = 0xFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kOtpRead,
     .access = Access::ReadOnly,
     .mask = 0xFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kFactoryConf,
     .access = Access::Forbidden,
     .mask = 0x1FU,
     .w1c = 0U,
     .reset = 0x0CU,
     .is_output = false},
    {.address = kShortConf,
     .access = Access::WriteOnly,
     .mask = 0x00070F0FU,
     .w1c = 0U,
     .reset = 0x00010606U,
     .is_output = false},
    {.address = kDrvConf,
     .access = Access::WriteOnly,
     .mask = 0x003F0F1FU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kGlobalScaler,
     .access = Access::WriteOnly,
     .mask = 0xFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kOffsetRead,
     .access = Access::ReadOnly,
     .mask = 0xFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kIholdIrun,
     .access = Access::WriteOnly,
     .mask = 0x000F1F1FU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kTpowerdown,
     .access = Access::WriteOnly,
     .mask = 0xFFU,
     .w1c = 0U,
     .reset = 10U,
     .is_output = false},
    {.address = kTstep,
     .access = Access::ReadOnly,
     .mask = 0xFFFFFU,
     .w1c = 0U,
     .reset = 0xFFFFFU,
     .is_output = false},
    {.address = kTpwmthrs,
     .access = Access::WriteOnly,
     .mask = 0xFFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kTcoolthrs,
     .access = Access::WriteOnly,
     .mask = 0xFFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kThigh,
     .access = Access::WriteOnly,
     .mask = 0xFFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kRampmode,
     .access = Access::ReadWrite,
     .mask = 0x3U,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kXactual,
     .access = Access::ReadWrite,
     .mask = 0xFFFFFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kVactual,
     .access = Access::ReadOnly,
     .mask = 0xFFFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kVstart,
     .access = Access::WriteOnly,
     .mask = 0x3FFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kA1,
     .access = Access::WriteOnly,
     .mask = 0xFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kV1,
     .access = Access::WriteOnly,
     .mask = 0xFFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kAmax,
     .access = Access::WriteOnly,
     .mask = 0xFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kVmax,
     .access = Access::WriteOnly,
     .mask = 0x7FFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kDmax,
     .access = Access::WriteOnly,
     .mask = 0xFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kD1,
     .access = Access::WriteOnly,
     .mask = 0xFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kVstop,
     .access = Access::WriteOnly,
     .mask = 0x3FFFFU,
     .w1c = 0U,
     .reset = 1U,
     .is_output = false},
    {.address = kTzerowait,
     .access = Access::WriteOnly,
     .mask = 0xFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kXtarget,
     .access = Access::ReadWrite,
     .mask = 0xFFFFFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kVdcmin,
     .access = Access::WriteOnly,
     .mask = 0x7FFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kSwMode,
     .access = Access::ReadWrite,
     .mask = 0xFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kRampStat,
     .access = Access::Clear,
     .mask = 0x3FFFU,
     .w1c = 0x10CCU,
     .reset = 0U,
     .is_output = false},
    {.address = kXlatch,
     .access = Access::ReadOnly,
     .mask = 0xFFFFFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kEncmode,
     .access = Access::ReadWrite,
     .mask = 0x7FFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kXEnc,
     .access = Access::ReadWrite,
     .mask = 0xFFFFFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kEncConst,
     .access = Access::WriteOnly,
     .mask = 0xFFFFFFFFU,
     .w1c = 0U,
     .reset = kEncConstReset,
     .is_output = false},
    {.address = kEncStatus,
     .access = Access::Clear,
     .mask = 0x3U,
     .w1c = 0x3U,
     .reset = 0U,
     .is_output = false},
    {.address = kEncLatch,
     .access = Access::ReadOnly,
     .mask = 0xFFFFFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kEncDeviation,
     .access = Access::WriteOnly,
     .mask = 0xFFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = 0x60U,
     .access = Access::WriteOnly,
     .mask = 0xFFFFFFFFU,
     .w1c = 0U,
     .reset = 0xAAAAB554U,
     .is_output = false},
    {.address = 0x61U,
     .access = Access::WriteOnly,
     .mask = 0xFFFFFFFFU,
     .w1c = 0U,
     .reset = 0x4A9554AAU,
     .is_output = false},
    {.address = 0x62U,
     .access = Access::WriteOnly,
     .mask = 0xFFFFFFFFU,
     .w1c = 0U,
     .reset = 0x24492929U,
     .is_output = false},
    {.address = 0x63U,
     .access = Access::WriteOnly,
     .mask = 0xFFFFFFFFU,
     .w1c = 0U,
     .reset = 0x10104222U,
     .is_output = false},
    {.address = 0x64U,
     .access = Access::WriteOnly,
     .mask = 0xFFFFFFFFU,
     .w1c = 0U,
     .reset = 0xFBFFFFFFU,
     .is_output = false},
    {.address = 0x65U,
     .access = Access::WriteOnly,
     .mask = 0xFFFFFFFFU,
     .w1c = 0U,
     .reset = 0xB5BB777DU,
     .is_output = false},
    {.address = 0x66U,
     .access = Access::WriteOnly,
     .mask = 0xFFFFFFFFU,
     .w1c = 0U,
     .reset = 0x49295556U,
     .is_output = false},
    {.address = 0x67U,
     .access = Access::WriteOnly,
     .mask = 0xFFFFFFFFU,
     .w1c = 0U,
     .reset = 0x00404222U,
     .is_output = false},
    {.address = kMslutsel,
     .access = Access::WriteOnly,
     .mask = 0xFFFFFFFFU,
     .w1c = 0U,
     .reset = kDefaultMslutsel,
     .is_output = false},
    {.address = kMslutstart,
     .access = Access::WriteOnly,
     .mask = 0x00FF00FFU,
     .w1c = 0U,
     .reset = kDefaultMslutstart,
     .is_output = false},
    {.address = kMscnt,
     .access = Access::ReadOnly,
     .mask = 0x3FFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kMscuract,
     .access = Access::ReadOnly,
     .mask = 0x01FF01FFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kChopconf,
     .access = Access::ReadWrite,
     .mask = 0xFFFDDFFFU,
     .w1c = 0U,
     .reset = kChopconfReset,
     .is_output = false},
    {.address = kCoolconf,
     .access = Access::WriteOnly,
     .mask = 0x017FEF6FU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kDcctrl,
     .access = Access::WriteOnly,
     .mask = 0x00FF03FFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kDrvStatus,
     .access = Access::ReadOnly,
     .mask = 0xFF1F03FFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kPwmconf,
     .access = Access::WriteOnly,
     .mask = 0xFF3FFFFFU,
     .w1c = 0U,
     .reset = kPwmconfReset,
     .is_output = false},
    {.address = kPwmScale,
     .access = Access::ReadOnly,
     .mask = 0x01FF00FFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kPwmAuto,
     .access = Access::ReadOnly,
     .mask = 0x00FF00FFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
    {.address = kLostSteps,
     .access = Access::ReadOnly,
     .mask = 0xFFFFFU,
     .w1c = 0U,
     .reset = 0U,
     .is_output = false},
}};

/**
 * @brief The row that lets the driver read `address`.
 * @param address The register address.
 * @return The row, or `nullptr` for a write-only or absent address.
 */
[[nodiscard]] constexpr const RegInfo* FindReadable(uint8_t address) noexcept {
  for (const RegInfo& r : kTable) {
    if ((r.address == address) && !r.is_output && (r.access != Access::WriteOnly) &&
        ((r.access != Access::Forbidden) || (address == kFactoryConf))) {
      return &r;
    }
  }
  return nullptr;
}

/**
 * @brief The row that lets the driver write `address`.
 * @param address The register address.
 * @return The row, or `nullptr` (read-only, forbidden or absent).
 */
[[nodiscard]] constexpr const RegInfo* FindWritable(uint8_t address) noexcept {
  for (const RegInfo& r : kTable) {
    const bool writes = (r.access == Access::WriteOnly) || (r.access == Access::ReadWrite) ||
                        (r.access == Access::Clear);
    if ((r.address == address) && writes) {
      return &r;
    }
  }
  return nullptr;
}

/**
 * @brief The reset value of a register.
 * @param address The register address.
 * @return The reset value (of the writable row when there are two); 0 if absent.
 */
[[nodiscard]] constexpr uint32_t ResetValue(uint8_t address) noexcept {
  for (const RegInfo& r : kTable) {
    if (r.address == address) {
      return r.reset;
    }
  }
  return 0U;
}

/** @name GCONF bits (page 32) */
/// @{
inline constexpr uint32_t kGconfRecalibrate = 1U << 0;        ///< Recalibrate zero crossing.
inline constexpr uint32_t kGconfFastStandstill = 1U << 1;     ///< Standstill after 2^18 clocks.
inline constexpr uint32_t kGconfEnPwmMode = 1U << 2;          ///< StealthChop enable.
inline constexpr uint32_t kGconfMultistepFilt = 1U << 3;      ///< Step pulse filtering.
inline constexpr uint32_t kGconfShaft = 1U << 4;              ///< Invert direction.
inline constexpr uint32_t kGconfDiag0Error = 1U << 5;         ///< DIAG0 on driver error.
inline constexpr uint32_t kGconfDiag0Otpw = 1U << 6;          ///< DIAG0 on pre-warning.
inline constexpr uint32_t kGconfDiag0Stall = 1U << 7;         ///< DIAG0 on stall (or STEP output).
inline constexpr uint32_t kGconfDiag1Stall = 1U << 8;         ///< DIAG1 on stall (or DIR output).
inline constexpr uint32_t kGconfDiag1Index = 1U << 9;         ///< DIAG1 on microstep index 0.
inline constexpr uint32_t kGconfDiag1OnState = 1U << 10;      ///< DIAG1 on chopper on-state.
inline constexpr uint32_t kGconfDiag1StepsSkipped = 1U << 11; ///< DIAG1 toggles on skipped steps.
inline constexpr uint32_t kGconfDiag0PushPull = 1U << 12;     ///< DIAG0 push-pull.
inline constexpr uint32_t kGconfDiag1PushPull = 1U << 13;     ///< DIAG1 push-pull.
inline constexpr uint32_t kGconfSmallHysteresis = 1U << 14;   ///< 1/32 hysteresis.
inline constexpr uint32_t kGconfStopEnable = 1U << 15;        ///< ENCA_DCIN stops the sequencer.
inline constexpr uint32_t kGconfDirectMode = 1U << 16;        ///< Coil currents from XTARGET.
inline constexpr uint32_t kGconfTestMode = 1U << 17;          ///< Test mode: never set.
/// @}

/** @name GSTAT bits (page 33) */
/// @{
inline constexpr uint32_t kGstatReset = 1U << 0;  ///< The chip was reset.
inline constexpr uint32_t kGstatDrvErr = 1U << 1; ///< The driver shut down.
inline constexpr uint32_t kGstatUvCp = 1U << 2;   ///< Charge pump undervoltage.
/// @}

/** @name IOIN bits (page 34) */
/// @{
inline constexpr uint32_t kIoinSdMode = 1U << 6; ///< Level of the SD_MODE strap.
/// @}

/** @name SW_MODE bits (page 43) */
/// @{
inline constexpr uint32_t kSwStopLeft = 1U << 0;           ///< Stop at the left switch.
inline constexpr uint32_t kSwStopRight = 1U << 1;          ///< Stop at the right switch.
inline constexpr uint32_t kSwPolLeft = 1U << 2;            ///< Left switch active low.
inline constexpr uint32_t kSwPolRight = 1U << 3;           ///< Right switch active low.
inline constexpr uint32_t kSwSwap = 1U << 4;               ///< Swap left and right.
inline constexpr uint32_t kSwLatchLeftActive = 1U << 5;    ///< Latch on active left edge.
inline constexpr uint32_t kSwLatchLeftInactive = 1U << 6;  ///< Latch on inactive left edge.
inline constexpr uint32_t kSwLatchRightActive = 1U << 7;   ///< Latch on active right edge.
inline constexpr uint32_t kSwLatchRightInactive = 1U << 8; ///< Latch on inactive right edge.
inline constexpr uint32_t kSwLatchEncoder = 1U << 9;       ///< Latch X_ENC too.
inline constexpr uint32_t kSwStopStall = 1U << 10;         ///< Stop on stall (sg_stop).
inline constexpr uint32_t kSwSoftStop = 1U << 11;          ///< Soft stop (en_softstop).
/// @}

/** @name RAMP_STAT bits (page 44) */
/// @{
inline constexpr uint32_t kRampStopLeftActive = 1U << 0;  ///< Left switch active.
inline constexpr uint32_t kRampStopRightActive = 1U << 1; ///< Right switch active.
inline constexpr uint32_t kRampLatchLeft = 1U << 2;       ///< Left latch ready (write 1 clears).
inline constexpr uint32_t kRampLatchRight = 1U << 3;      ///< Right latch ready (write 1 clears).
inline constexpr uint32_t kRampEventStopLeft = 1U << 4;   ///< Left stop event.
inline constexpr uint32_t kRampEventStopRight = 1U << 5;  ///< Right stop event.
inline constexpr uint32_t kRampEventStopStall = 1U << 6;  ///< Stall stop event (write 1 clears).
inline constexpr uint32_t kRampEventPosReached = 1U
                                                 << 7; ///< Target reached event (write 1 clears).
inline constexpr uint32_t kRampVelocityReached = 1U << 8; ///< VACTUAL equals VMAX.
inline constexpr uint32_t kRampPositionReached = 1U << 9; ///< XACTUAL equals XTARGET.
inline constexpr uint32_t kRampVzero = 1U << 10;          ///< VACTUAL is 0.
inline constexpr uint32_t kRampZeroWaitActive = 1U << 11; ///< TZEROWAIT is running.
inline constexpr uint32_t kRampSecondMove = 1U << 12;     ///< Ramp reversed (write 1 clears).
inline constexpr uint32_t kRampStatusSg = 1U << 13;       ///< Stall input active.
/// @}

/** @name DRV_STATUS bits (page 56) */
/// @{
inline constexpr uint32_t kDrvS2vsA = 1U << 12;      ///< Short to supply, phase A.
inline constexpr uint32_t kDrvS2vsB = 1U << 13;      ///< Short to supply, phase B.
inline constexpr uint32_t kDrvStealth = 1U << 14;    ///< StealthChop active.
inline constexpr uint32_t kDrvFsActive = 1U << 15;   ///< Full-step active.
inline constexpr uint32_t kDrvStallGuard = 1U << 24; ///< Stall detected.
inline constexpr uint32_t kDrvOt = 1U << 25;         ///< Overtemperature shutdown.
inline constexpr uint32_t kDrvOtpw = 1U << 26;       ///< Overtemperature pre-warning.
inline constexpr uint32_t kDrvS2gA = 1U << 27;       ///< Short to ground, phase A.
inline constexpr uint32_t kDrvS2gB = 1U << 28;       ///< Short to ground, phase B.
inline constexpr uint32_t kDrvOlA = 1U << 29;        ///< Open load, phase A.
inline constexpr uint32_t kDrvOlB = 1U << 30;        ///< Open load, phase B.
inline constexpr uint32_t kDrvStst = 1U << 31;       ///< Standstill.
/// @}

/** @name Field helpers */
/// @{
using Toff = detail::Field<uint32_t, 0, 4>;         ///< CHOPCONF TOFF.
using Hstrt = detail::Field<uint32_t, 4, 3>;        ///< CHOPCONF HSTRT / TFD[2:0].
using Hend = detail::Field<uint32_t, 7, 4>;         ///< CHOPCONF HEND / OFFSET.
using Fd3 = detail::Field<uint32_t, 11, 1>;         ///< CHOPCONF fd3.
using Disfdcc = detail::Field<uint32_t, 12, 1>;     ///< CHOPCONF disfdcc.
using Chm = detail::Field<uint32_t, 14, 1>;         ///< CHOPCONF chm.
using Tbl = detail::Field<uint32_t, 15, 2>;         ///< CHOPCONF TBL.
using Vhighfs = detail::Field<uint32_t, 18, 1>;     ///< CHOPCONF vhighfs.
using Vhighchm = detail::Field<uint32_t, 19, 1>;    ///< CHOPCONF vhighchm.
using Tpfd = detail::Field<uint32_t, 20, 4>;        ///< CHOPCONF TPFD.
using Mres = detail::Field<uint32_t, 24, 4>;        ///< CHOPCONF MRES.
using Intpol = detail::Field<uint32_t, 28, 1>;      ///< CHOPCONF intpol.
using Dedge = detail::Field<uint32_t, 29, 1>;       ///< CHOPCONF dedge.
using Diss2g = detail::Field<uint32_t, 30, 1>;      ///< CHOPCONF diss2g.
using Diss2vs = detail::Field<uint32_t, 31, 1>;     ///< CHOPCONF diss2vs.
using Ihold = detail::Field<uint32_t, 0, 5>;        ///< IHOLD_IRUN IHOLD.
using Irun = detail::Field<uint32_t, 8, 5>;         ///< IHOLD_IRUN IRUN.
using IholdDelay = detail::Field<uint32_t, 16, 4>;  ///< IHOLD_IRUN IHOLDDELAY.
using PwmOfs = detail::Field<uint32_t, 0, 8>;       ///< PWMCONF PWM_OFS.
using PwmGrad = detail::Field<uint32_t, 8, 8>;      ///< PWMCONF PWM_GRAD.
using PwmFreq = detail::Field<uint32_t, 16, 2>;     ///< PWMCONF pwm_freq.
using Freewheel = detail::Field<uint32_t, 20, 2>;   ///< PWMCONF freewheel.
using PwmReg = detail::Field<uint32_t, 24, 4>;      ///< PWMCONF PWM_REG.
using PwmLim = detail::Field<uint32_t, 28, 4>;      ///< PWMCONF PWM_LIM.
using Semin = detail::Field<uint32_t, 0, 4>;        ///< COOLCONF SEMIN.
using Seup = detail::Field<uint32_t, 5, 2>;         ///< COOLCONF SEUP.
using Semax = detail::Field<uint32_t, 8, 4>;        ///< COOLCONF SEMAX.
using Sedn = detail::Field<uint32_t, 13, 2>;        ///< COOLCONF SEDN.
using Seimin = detail::Field<uint32_t, 15, 1>;      ///< COOLCONF SEIMIN.
using Sgt = detail::Field<uint32_t, 16, 7>;         ///< COOLCONF SGT.
using Sfilt = detail::Field<uint32_t, 24, 1>;       ///< COOLCONF SFILT.
using S2vsLevel = detail::Field<uint32_t, 0, 4>;    ///< SHORT_CONF S2VS_LEVEL.
using S2gLevel = detail::Field<uint32_t, 8, 4>;     ///< SHORT_CONF S2G_LEVEL.
using ShortFilter = detail::Field<uint32_t, 16, 2>; ///< SHORT_CONF SHORTFILTER.
using ShortDelay = detail::Field<uint32_t, 18, 1>;  ///< SHORT_CONF shortdelay.
using Bbmtime = detail::Field<uint32_t, 0, 5>;      ///< DRV_CONF BBMTIME.
using Bbmclks = detail::Field<uint32_t, 8, 4>;      ///< DRV_CONF BBMCLKS.
using Otselect = detail::Field<uint32_t, 16, 2>;    ///< DRV_CONF OTSELECT.
using Drvstrength = detail::Field<uint32_t, 18, 2>; ///< DRV_CONF DRVSTRENGTH.
using FiltIsense = detail::Field<uint32_t, 20, 2>;  ///< DRV_CONF FILT_ISENSE.
using NodeAddr = detail::Field<uint32_t, 0, 8>;     ///< NODECONF NODEADDR.
using SendDelay = detail::Field<uint32_t, 8, 4>;    ///< NODECONF SENDDELAY.
using DcTime = detail::Field<uint32_t, 0, 10>;      ///< DCCTRL DC_TIME.
using DcSg = detail::Field<uint32_t, 16, 8>;        ///< DCCTRL DC_SG.
using StartSin = detail::Field<uint32_t, 0, 8>;     ///< MSLUTSTART START_SIN.
using StartSin90 = detail::Field<uint32_t, 16, 8>;  ///< MSLUTSTART START_SIN90.
/// @}

/// @}

} // namespace tmc5160::reg

#endif // TMC5160_REGISTERS_HPP
