/**
 * @file mock_tmc5160.hpp
 * @brief A behavioural model of the TMC5160 / TMC5160A, written from the datasheet and independent
 * of the driver.
 *
 * Nothing here includes a driver header: register addresses, bit positions and formulas are
 * repeated from the datasheet (Rev. 1.18) so a driver mistake cannot hide in a shared constant. The
 * model has
 *
 *  - the register file with write masks, write-to-clear bits, write-only registers and reset
 * values,
 *  - the SPI front end (40-bit pipelined datagrams, the status byte one transfer old, a daisy
 * chain) and the UART front end (sync, node, CRC-8, IFCNT, SENDDELAY, 12-bit-time recovery),
 *  - a ramp generator integrated in physical units, current scaling, thresholds, CoolStep and
 * StallGuard scripted from the test, protection and reset injection, DIAG pins, an encoder counter
 * and the microstep table,
 *  - a `Knobs` structure with one switch for the ambiguities of docs/ambiguities.md that the
 * model can express, so a test shows the driver survives either reading.
 */
#ifndef TMC5160_MOCK_TMC5160_HPP
#define TMC5160_MOCK_TMC5160_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

namespace tmc5160_test {

/// One switch per ambiguity (see the ambiguity page of the docs). The defaults are the reading the
/// driver assumes.
struct Knobs {
  uint32_t fclk_hz = 12'000'000; ///< A20 and clock: the clock the chip really runs at.
  uint32_t rsense_mohm = 100;    ///< The sense resistor on the board.
  uint32_t vfs_mv = 325;         ///< A20: full-scale sense voltage the chip really has.
  bool status_lag = true;        ///< A3: the status byte shows the previous transfer.
  uint8_t wo_read_mode =
      0;                   ///< A4: reading a write-only register: 0 zeros, 1 last value, 2 garbage.
  bool echo_writes = true; ///< A5: the reply after a write mirrors the written data.
  bool lut_literal_reading =
      false; ///< A6: decode the microstep table the other way (unused by the driver).
  bool mscuract_swap = false;         ///< A7: CUR_A and CUR_B swapped.
  uint32_t otp_byte = 0;              ///< A8: OTP_READ content.
  bool bbm_hazard = false;            ///< A9: BBM written with a running motor raises drv_err.
  bool gstat_reset_value = true;      ///< A13: GSTAT.reset set after power-up.
  bool multistep_reset = false;       ///< A12: GCONF.multistep_filt resets to 1 (the p32 reading).
  bool ramp_stat_wc = true;           ///< A24: event bits clear on write 1.
  bool direct_mode_exit = true;       ///< A26: XTARGET keeps the coil value when direct mode ends.
  bool vdcmin_range = true;           ///< A27: VDCMIN is 23 bits wide.
  uint8_t version_byte = 0x30;        ///< A29: IOIN.VERSION.
  bool ifcnt_spi = false;             ///< A30: IFCNT counts in SPI mode too.
  bool node_254 = true;               ///< A31: node 254 accepted.
  uint32_t uart_reply_delay_bits = 0; ///< A32: extra reply delay in bit times.
  uint32_t uart_recovery_bits = 12;   ///< A33: idle bit times needed after a UART error.
  bool chain_order = false;           ///< A34: index 0 is the chip nearest MISO instead of MOSI.
  bool sd_mode = false;               ///< A42: the SD_MODE strap (true = STEP/DIR).
  bool enc_decimal_sign = true;       ///< A43: negative decimal constants follow the p107 rule.
  bool xcompare_pulse = true;         ///< A49: X_COMPARE gives a one-update DIAG pulse.
  bool at_a_version = true;           ///< A37: the A version's autotune.
  uint32_t baud = 115'200;            ///< The bit time the chip assumes for UART recovery.
};

/// Bytes of register addresses the model knows, repeated from the datasheet.
namespace m {
inline constexpr uint8_t GCONF = 0x00, GSTAT = 0x01, IFCNT = 0x02, SLAVECONF = 0x03, IOIN = 0x04,
                         X_COMPARE = 0x05, OTP_PROG = 0x06, OTP_READ = 0x07, FACTORY_CONF = 0x08,
                         SHORT_CONF = 0x09, DRV_CONF = 0x0A, GLOBAL_SCALER = 0x0B,
                         OFFSET_READ = 0x0C, IHOLD_IRUN = 0x10, TPOWERDOWN = 0x11, TSTEP = 0x12,
                         TPWMTHRS = 0x13, TCOOLTHRS = 0x14, THIGH = 0x15, RAMPMODE = 0x20,
                         XACTUAL = 0x21, VACTUAL = 0x22, VSTART = 0x23, A1 = 0x24, V1 = 0x25,
                         AMAX = 0x26, VMAX = 0x27, DMAX = 0x28, D1 = 0x2A, VSTOP = 0x2B,
                         TZEROWAIT = 0x2C, XTARGET = 0x2D, VDCMIN = 0x33, SW_MODE = 0x34,
                         RAMP_STAT = 0x35, XLATCH = 0x36, ENCMODE = 0x38, X_ENC = 0x39,
                         ENC_CONST = 0x3A, ENC_STATUS = 0x3B, ENC_LATCH = 0x3C,
                         ENC_DEVIATION = 0x3D, MSLUT0 = 0x60, MSLUTSEL = 0x68, MSLUTSTART = 0x69,
                         MSCNT = 0x6A, MSCURACT = 0x6B, CHOPCONF = 0x6C, COOLCONF = 0x6D,
                         DCCTRL = 0x6E, DRV_STATUS = 0x6F, PWMCONF = 0x70, PWM_SCALE = 0x71,
                         PWM_AUTO = 0x72, LOST_STEPS = 0x73;
} // namespace m

/// CRC-8 of the UART datagram, written bit by bit from page 116 (polynomial 0x07, LSB first).
inline uint8_t ModelCrc8(const uint8_t* data, std::size_t len) noexcept {
  uint8_t crc = 0;
  for (std::size_t i = 0; i < len; ++i) {
    uint8_t b = data[i];
    for (int bit = 0; bit < 8; ++bit) {
      if (((static_cast<uint32_t>(crc) >> 7U) ^ (b & 1U)) != 0U) {
        crc = static_cast<uint8_t>((static_cast<uint32_t>(crc) << 1U) ^ 0x07U);
      } else {
        crc = static_cast<uint8_t>(crc << 1);
      }
      b = static_cast<uint8_t>(b >> 1);
    }
  }
  return crc;
}

/// One chip.
class Chip {
public:
  Knobs k{};

  // ---- what the tests observe ----
  uint32_t reset_count = 0;         ///< Power-ups so far, the first included.
  uint32_t writes = 0;              ///< Register writes accepted.
  uint32_t reserved_bit_writes = 0; ///< Writes with bits set outside the register's defined bits.
  uint32_t forbidden_writes = 0;    ///< OTP_PROG, FACTORY_CONF or the test_mode bit written.
  uint32_t wo_reads = 0;            ///< Reads of write-only registers.
  uint32_t ramp_writes_moving = 0;  ///< Ramp registers written with the motor moving.
  uint32_t datagrams = 0;           ///< SPI datagrams or UART frames decoded.
  uint32_t crc_errors = 0;          ///< UART frames dropped for CRC or sync.
  uint32_t dropped_in_recovery = 0; ///< UART frames dropped while the line had to idle.
  uint32_t ifcnt = 0;               ///< The interface counter.
  int32_t min_x = 0;                ///< Lowest XACTUAL since `ResetWatch()`.
  int32_t max_x = 0;                ///< Highest XACTUAL since `ResetWatch()`.
  // Blank reads, a live chip with no flag set (the stuck-line tie-break): with `blank_reads`,
  // every SPI register but IOIN reads 0 and the status byte reads 0, except register
  // `blank_keep` (0x80: the status byte; 0x100: none), which reads `blank_value`.
  uint32_t blank_keep = 0x100U;
  uint32_t blank_value = 0U;
  bool nao = false; ///< OUTPUT register: the level on NAO.
  bool blank_reads = false;
  double peak_coil_ma =
      0.0;            ///< Highest coil current RMS the model has computed with the driver enabled.
  double min_v = 0.0; ///< Lowest velocity (Hz, signed) since `ResetWatch()`.
  double max_v = 0.0; ///< Highest velocity since `ResetWatch()`.
  double stale_target_us = 0.0; ///< Time moving in position mode towards an older move's target.
  double first_v = 0.0;         ///< Speed at the first motion after `ResetWatch()` (Hz).
  std::array<uint8_t, 8> write_log{}; ///< Register addresses written since `ClearWriteLog()`.
  std::size_t write_log_n = 8U;       ///< Entries in `write_log` (8: not logging).
  void ClearWriteLog() noexcept {
    write_log.fill(0U);
    write_log_n = 0U;
  }
  bool at1_failed = false; ///< StealthChop autotune AT#1 began with IHOLD different from IRUN.
  bool at1_done = false;   ///< AT#1 completed.

  Chip() {
    PowerOn();
  }

  /// Power-up or brown-out: every register returns to its reset value, GSTAT.reset is set.
  void PowerOn() noexcept {
    r_.fill(0U);
    last_written_.fill(0U);
    ++reset_count;
    r_[m::GSTAT] = k.gstat_reset_value ? 1U : 0U;
    r_[m::TPOWERDOWN] = 10U;
    r_[m::TZEROWAIT] = 0U;
    r_[m::CHOPCONF] = 0x10410150U;
    r_[m::PWMCONF] = 0xC40C001EU;
    r_[m::GCONF] = k.multistep_reset ? (1U << 3) : 0U;
    uint32_t words[8] = {0xAAAAB554U, 0x4A9554AAU, 0x24492929U, 0x10104222U,
                         0xFBFFFFFFU, 0xB5BB777DU, 0x49295556U, 0x00404222U};
    for (uint32_t i = 0; i < 8U; ++i) {
      r_[m::MSLUT0 + i] = words[i];
    }
    r_[m::MSLUTSEL] = 0xFFFF8056U;
    r_[m::MSLUTSTART] = 0x00F70000U;
    xactual_ = 0;
    xtarget_ = 0;
    v_hz_ = 0.0;
    pos_frac_ = 0.0;
    status_pending_ = 0;
    reply_pending_ = 0;
    at1_us_ = 0.0;
    zw_left_ = 0.0;
    still_us_ = 1e9; // a chip at power-up has been still for a long time
    at1_failed = false;
    at1_done = false;
    stall_flag_ = false;
    ot_ = false;
    otpw_ = false;
    s2g_ = false;
    uart_recover_us_ = 0.0;
    ifcnt = 0;
    pos_reached_ = false;
    compare_pulse_ = false;
    x_enc_ = 0;
    enc_status_ = 0;
  }

  // ---- environment inputs ----
  void SetDrvEnn(bool enabled) noexcept {
    drv_enn_low_ = enabled;
  }
  [[nodiscard]] bool DrvEnnActive() const noexcept {
    return drv_enn_low_;
  }
  void SetReferenceSwitches(bool left, bool right) noexcept {
    ref_left_ = left;
    ref_right_ = right;
  }
  void InjectStall(bool stalled) noexcept {
    stall_flag_ = stalled;
  }
  void InjectOvertemp(bool shutdown) noexcept {
    otpw_ = true;
    ot_ = shutdown;
    if (shutdown) {
      r_[m::GSTAT] |= 2U;
    }
  }
  void ClearTemperature() noexcept {
    otpw_ = false;
    ot_ = false;
  }
  void InjectShort() noexcept {
    s2g_ = true;
    r_[m::GSTAT] |= 2U;
  }
  void ClearShort() noexcept {
    s2g_ = false;
  }
  void InjectChargePumpUv() noexcept {
    r_[m::GSTAT] |= 4U;
  }
  void SetSgResult(uint32_t v) noexcept {
    sg_result_ = v & 0x3FFU;
  }
  void SetOffsets(int a, int b) noexcept {
    offset_a_ = a;
    offset_b_ = b;
  }
  void AddEncoderCounts(int32_t counts) noexcept {
    const auto cst = static_cast<int32_t>(r_[m::ENC_CONST]);
    double factor = 0.0;
    if ((r_[m::ENCMODE] & (1U << 10)) != 0U) { // decimal: hi16 signed integer, lo16 in 1/10000
      const int32_t hi = (cst / 65536) - (((cst % 65536) < 0) ? 1 : 0); // arithmetic shift by 16
      const auto lo = static_cast<int32_t>(static_cast<uint32_t>(cst) & 0xFFFFU);
      const double frac = static_cast<double>(lo) / 10000.0;
      factor = static_cast<double>(hi) + frac;
      if (!k.enc_decimal_sign && (hi < 0)) {
        factor = -(static_cast<double>(-hi) + frac);
      }
    } else {
      factor = static_cast<double>(cst) / 65536.0;
    }
    x_enc_ += static_cast<int32_t>(std::floor(factor * static_cast<double>(counts)));
  }

  // ---- derived values for invariants ----
  [[nodiscard]] uint32_t Reg(uint8_t a) const noexcept {
    return r_[a & 0x7FU];
  }
  /// The latched RAMP_STAT event bits, as the chip holds them.
  [[nodiscard]] uint32_t RampEvents() const noexcept {
    return ramp_events_;
  }
  [[nodiscard]] int32_t Xtarget() const noexcept {
    return xtarget_;
  }
  [[nodiscard]] int32_t Xactual() const noexcept {
    return xactual_;
  }
  [[nodiscard]] double VelocityHz() const noexcept {
    return v_hz_;
  }
  [[nodiscard]] bool DriverOn() const noexcept {
    return drv_enn_low_ && ((r_[m::CHOPCONF] & 0xFU) != 0U);
  }
  [[nodiscard]] bool Standstill() const noexcept {
    return v_hz_ == 0.0;
  }
  // ---- watch: what the motor did between two looks of a test ----
  /// Start a watch at the present position and velocity.
  void ResetWatch() noexcept {
    min_x = xactual_;
    max_x = xactual_;
    min_v = v_hz_;
    max_v = v_hz_;
    first_v = std::fabs(v_hz_);
    stale_target_us = 0.0;
  }
  /// RAMP_STAT.t_zerowait_active as the chip holds it now (page 44).
  [[nodiscard]] bool ZeroWaitActive() const noexcept {
    return zw_left_ > 0.0;
  }
  /// TZEROWAIT time left, microseconds.
  [[nodiscard]] double ZeroWaitLeftUs() const noexcept {
    return zw_left_ * 1e6;
  }

  /// Coil RMS current in mA for the given run or hold scale, from the formula of page 74.
  [[nodiscard]] double CoilMa(uint32_t cs_field) const noexcept {
    const uint32_t gs =
        (r_[m::GLOBAL_SCALER] & 0xFFU) == 0U ? 256U : (r_[m::GLOBAL_SCALER] & 0xFFU);
    const double irms =
        (static_cast<double>(gs) / 256.0) * (static_cast<double>(cs_field + 1U) / 32.0) *
        (static_cast<double>(k.vfs_mv) / static_cast<double>(k.rsense_mohm)) / std::numbers::sqrt2;
    return irms * 1000.0;
  }
  [[nodiscard]] double RunMa() const noexcept {
    return CoilMa((r_[m::IHOLD_IRUN] >> 8) & 0x1FU);
  }
  [[nodiscard]] double HoldMa() const noexcept {
    return CoilMa(r_[m::IHOLD_IRUN] & 0x1FU);
  }

  /// The microstep table decoded from MSLUT, MSLUTSEL and MSLUTSTART (page 47).
  [[nodiscard]] int TableEntry(uint32_t i) const noexcept {
    const uint32_t sel = r_[m::MSLUTSEL];
    if (i >= 256U) {
      return static_cast<int>((r_[m::MSLUTSTART] >> 16) & 0xFFU); // START_SIN90
    }
    const uint32_t x1 = (sel >> 8) & 0xFFU;
    const uint32_t x2 = (sel >> 16) & 0xFFU;
    const uint32_t x3 = (sel >> 24) & 0xFFU;
    const uint32_t wcode[4] = {sel & 3U, (sel >> 2) & 3U, (sel >> 4) & 3U, (sel >> 6) & 3U};
    int value = static_cast<int>(r_[m::MSLUTSTART] & 0xFFU);
    // The verified reading (A6): bit n of the table is the increment into entry n; the literal
    // reading
    // (`lut_literal_reading`) takes bit n as the step from entry n to n + 1.
    for (uint32_t n = k.lut_literal_reading ? 0U : 1U; n < (k.lut_literal_reading ? i : i + 1U);
         ++n) {
      uint32_t seg = 3U;
      if (n < x1) {
        seg = 0U;
      } else if (n < x2) {
        seg = 1U;
      } else if (n < x3) {
        seg = 2U;
      } else {
        seg = 3U; // past the third boundary: the last segment
      }
      const int bit = static_cast<int>((r_[m::MSLUT0 + (n >> 5)] >> (n & 31U)) & 1U);
      value += static_cast<int>(wcode[seg]) - 1 + bit;
    }
    return value;
  }

  // ---- time ----
  void Elapse(uint32_t us) noexcept {
    auto left = static_cast<double>(us);
    while (left > 0.0) {
      // A step ends where a TZEROWAIT pause ends, so the next move starts on time (page 83 c).
      // Only while the pause really runs (position mode, not direct mode), and never below 1 us.
      const bool pausing = (zw_left_ > 0.0) && ((r_[m::RAMPMODE] & 3U) == 0U) && !directMode();
      const double pause_us = pausing ? std::max(zw_left_ * 1e6, 1.0) : 250.0;
      const auto dt = std::min({left, 250.0, pause_us});
      step(dt * 1e-6);
      watch(dt);
      left -= dt;
    }
    if (uart_recover_us_ > 0.0) {
      uart_recover_us_ = uart_recover_us_ > static_cast<double>(us)
                             ? uart_recover_us_ - static_cast<double>(us)
                             : 0.0;
    }
  }

  // ---- register access ----
  uint32_t Read(uint8_t address) noexcept {
    const uint8_t a = address & 0x7FU;
    switch (a) {
    case m::GCONF:
      return r_[a];
    case m::GSTAT:
      return r_[a] & 7U;
    case m::IFCNT:
      return (k.ifcnt_spi || uart_mode_) ? (ifcnt & 0xFFU) : 0U;
    case m::IOIN: {
      uint32_t v = (static_cast<uint32_t>(k.version_byte) << 24);
      v |= (ref_left_ ? 1U : 0U) | (ref_right_ ? 2U : 0U);
      v |= drv_enn_low_ ? 0U : (1U << 4); // DRV_ENN is the package pin: high = off
      v |= k.sd_mode ? (1U << 6) : 0U;
      return v;
    }
    case m::OTP_READ:
      return k.otp_byte;
    case m::FACTORY_CONF:
      return 0x0AU;
    case m::OFFSET_READ:
      return ((static_cast<uint32_t>(offset_a_) & 0xFFU) << 8U) |
             (static_cast<uint32_t>(offset_b_) & 0xFFU);
    case m::TSTEP: {
      const double v = std::fabs(v_hz_);
      if (v < 1.0) {
        return 0xFFFFFU;
      }
      const double t = static_cast<double>(k.fclk_hz) / v;
      return t > 1048575.0 ? 0xFFFFFU : static_cast<uint32_t>(t);
    }
    case m::RAMPMODE:
      return r_[a] & 3U;
    case m::XACTUAL:
      return static_cast<uint32_t>(xactual_);
    case m::VACTUAL:
      return static_cast<uint32_t>(vactualRaw()) & 0xFFFFFFU;
    case m::XTARGET:
      return static_cast<uint32_t>(xtarget_);
    case m::SW_MODE:
      return r_[a];
    case m::RAMP_STAT:
      return rampStat();
    case m::XLATCH:
      return static_cast<uint32_t>(xlatch_);
    case m::ENCMODE:
      return r_[a];
    case m::X_ENC:
      return static_cast<uint32_t>(x_enc_);
    case m::ENC_STATUS:
      return enc_status_;
    case m::ENC_LATCH:
      return static_cast<uint32_t>(enc_latch_);
    case m::MSCNT:
      return static_cast<uint32_t>(xactual_) & 1023U;
    case m::MSCURACT:
      return msCurAct();
    case m::DRV_STATUS:
      return drvStatus();
    case m::PWM_SCALE:
      return at1_done ? 0x00300040U : 0U;
    case m::PWM_AUTO:
      return at1_done ? 0x00300030U : 0U;
    case m::LOST_STEPS:
      return 0U;
    default:
      break;
    }
    if (isReadable(a)) {
      return r_[a];
    }
    ++wo_reads;
    if (k.wo_read_mode == 1U) {
      return last_written_[a];
    }
    return k.wo_read_mode == 2U ? (0xDEADBE00U | a) : 0U;
  }

  void Write(uint8_t address, uint32_t v) noexcept {
    const uint8_t a = address & 0x7FU;
    const uint32_t mask = writeMask(a);
    ++writes;
    if (write_log_n < write_log.size()) {
      write_log[write_log_n] = a; // the order of the writes, for the tests that pin it
      ++write_log_n;
    }
    if ((mask == 0U) && (a != m::OTP_PROG) && (a != m::FACTORY_CONF)) {
      return; // a read-only or unknown register: the chip ignores the write
    }
    if (((v & ~mask) != 0U) && (a != m::GSTAT) && (a != m::RAMP_STAT) && (a != m::ENC_STATUS)) {
      ++reserved_bit_writes;
    }
    if (a == m::OTP_PROG) {
      ++forbidden_writes;
      return;
    }
    if (a == m::FACTORY_CONF) {
      ++forbidden_writes;
      return;
    }
    if ((a == m::GCONF) && ((v & (1U << 17)) != 0U)) {
      ++forbidden_writes; // test_mode
    }
    const uint32_t old = r_[a];
    switch (a) {
    case m::GSTAT:
      r_[a] &= ~(v & 7U);
      r_[a] |= (s2g_ || ot_) ? 2U : 0U; // the cause is still there: the error stays
      return;
    case m::RAMP_STAT:
      if (k.ramp_stat_wc) {
        ramp_events_ &= ~(v & 0x10CCU);
      }
      return;
    case m::ENC_STATUS:
      enc_status_ &= ~(v & 3U);
      return;
    case m::XACTUAL:
      xactual_ = static_cast<int32_t>(v);
      pos_frac_ = 0.0;
      return;
    case m::XTARGET:
      xtarget_ = static_cast<int32_t>(v);
      target_stale_ = false;
      direct_target_ = v;
      pos_reached_ = false;
      return;
    case m::X_ENC:
      x_enc_ = static_cast<int32_t>(v);
      return;
    case m::X_COMPARE:
      r_[a] = v;
      return;
    case m::SLAVECONF:
      if (!k.node_254 && ((v & 0xFFU) == 254U)) {
        return; // A31, the other reading: 254 is not a valid address
      }
      r_[a] = v & mask;
      return;
    case m::IOIN: // address 0x04 written: the OUTPUT register, UART only
      if (uart_mode_) {
        nao = (v & 1U) != 0U;
      }
      return;
    default:
      break;
    }
    r_[a] = v & mask;
    last_written_[a] = v & mask;
    if (a == m::RAMPMODE) {
      zw_left_ = 0.0; // a new mode ends the pause
      // XTARGET is ignored outside position mode: from here on it is an older move's target
      target_stale_ = target_stale_ || ((v & 3U) != 0U);
    }
    if ((a == m::GCONF) && ((old & (1U << 16)) != 0U) && ((r_[a] & (1U << 16)) == 0U) &&
        !k.direct_mode_exit) {
      xtarget_ = xactual_; // A26, the other reading: the chip resets XTARGET itself
    }
    const bool on_the_fly =
        (a == m::RAMPMODE) || (a == m::VMAX) || (a == m::AMAX) || (a == m::DMAX);
    if ((a >= m::VSTART) && (a <= m::TZEROWAIT) && !on_the_fly && (v_hz_ != 0.0) &&
        (old != r_[a])) {
      ++ramp_writes_moving;
    }
    if ((a == m::DRV_CONF) && k.bbm_hazard && DriverOn() && (v_hz_ != 0.0) && (old != r_[a])) {
      r_[m::GSTAT] |= 2U;
    }
    if ((a == m::IHOLD_IRUN) && !at1_done && !at1_failed && stealthOn() && DriverOn() &&
        ((r_[a] & 0x1FU) != ((r_[a] >> 8) & 0x1FU))) {
      at1_failed = true;
    }
    if ((a == m::CHOPCONF) || (a == m::GCONF) || (a == m::IHOLD_IRUN) || (a == m::GLOBAL_SCALER)) {
      notePeak();
    }
  }

  // ---- SPI front end ----
  /// One datagram: `in` is 5 bytes; `out` receives 5 bytes (status, previous reply data).
  void SpiDatagram(const uint8_t* in, uint8_t* out) noexcept {
    uart_mode_ = false;
    out[0] = blankStatus(k.status_lag ? status_pending_ : StatusByte());
    out[1] = static_cast<uint8_t>(reply_pending_ >> 24);
    out[2] = static_cast<uint8_t>(reply_pending_ >> 16);
    out[3] = static_cast<uint8_t>(reply_pending_ >> 8);
    out[4] = static_cast<uint8_t>(reply_pending_);
    ++datagrams;
    const bool write = (in[0] & 0x80U) != 0U;
    const uint8_t address = in[0] & 0x7FU;
    const uint32_t data = (static_cast<uint32_t>(in[1]) << 24) |
                          (static_cast<uint32_t>(in[2]) << 16) |
                          (static_cast<uint32_t>(in[3]) << 8) | in[4];
    if (write) {
      Write(address, data);
      if (k.echo_writes) {
        reply_pending_ = data;
      }
    } else {
      reply_pending_ = blankWord(address, Read(address));
    }
    status_pending_ = StatusByte();
  }

  /// UART: handle one frame. Returns the reply length (0 for none).
  std::size_t UartFrame(const uint8_t* tx, std::size_t len, uint8_t* rx) noexcept {
    uart_mode_ = true;
    if (uart_recover_us_ > 0.0) {
      ++dropped_in_recovery;
      uart_recover_us_ = RecoveryUs();
      return 0;
    }
    const bool write = len == 8U;
    if (((len != 8U) && (len != 4U)) || ((tx[0] & 0x0FU) != 0x05U) ||
        (ModelCrc8(tx, len - 1U) != tx[len - 1U])) {
      ++crc_errors;
      uart_recover_us_ = RecoveryUs();
      return 0;
    }
    const uint32_t node = tx[1];
    if (node != Node()) {
      return 0;
    }
    ++datagrams;
    if (write) {
      if ((tx[2] & 0x80U) == 0U) {
        return 0;
      }
      const uint32_t data = (static_cast<uint32_t>(tx[3]) << 24) |
                            (static_cast<uint32_t>(tx[4]) << 16) |
                            (static_cast<uint32_t>(tx[5]) << 8) | tx[6];
      Write(tx[2] & 0x7FU, data);
      ifcnt = (ifcnt + 1U) & 0xFFU;
      return 0;
    }
    if ((tx[2] & 0x80U) != 0U) {
      return 0;
    }
    const uint32_t v = Read(tx[2] & 0x7FU);
    rx[0] = 0x05;
    rx[1] = 0xFF;
    rx[2] = tx[2] & 0x7FU;
    rx[3] = static_cast<uint8_t>(v >> 24);
    rx[4] = static_cast<uint8_t>(v >> 16);
    rx[5] = static_cast<uint8_t>(v >> 8);
    rx[6] = static_cast<uint8_t>(v);
    rx[7] = ModelCrc8(rx, 7);
    return 8;
  }
  [[nodiscard]] uint32_t Node() const noexcept {
    return r_[m::SLAVECONF] & 0xFFU;
  }
  /// Time from the end of a read request to the start of the reply, in microseconds (page 27, A32).
  [[nodiscard]] double ReplyDelayUs() const noexcept {
    return static_cast<double>((SendDelay() * 8U) + k.uart_reply_delay_bits) * 1e6 /
           static_cast<double>(k.baud);
  }
  [[nodiscard]] uint32_t SendDelay() const noexcept {
    return (r_[m::SLAVECONF] >> 8) & 0xFU;
  }

  /// Pin levels, electrical: open drain pulls low when active.
  [[nodiscard]] bool Diag0Level() const noexcept {
    const uint32_t g = r_[m::GCONF];
    const bool active = ((((g >> 5) & 1U) != 0U) && ((r_[m::GSTAT] & 2U) != 0U)) ||
                        ((((g >> 6) & 1U) != 0U) && otpw_) ||
                        ((((g >> 7) & 1U) != 0U) && stall_flag_);
    const bool push_pull = ((g >> 12) & 1U) != 0U;
    return push_pull ? active : !active;
  }
  [[nodiscard]] bool Diag1Level() const noexcept {
    const uint32_t g = r_[m::GCONF];
    const bool active = ((((g >> 8) & 1U) != 0U) && stall_flag_) || compare_pulse_;
    const bool push_pull = ((g >> 13) & 1U) != 0U;
    return push_pull ? active : !active;
  }
  [[nodiscard]] uint8_t StatusByte() const noexcept {
    uint32_t s = 0;
    s |= (r_[m::GSTAT] & 1U) != 0U ? 1U : 0U;
    s |= (r_[m::GSTAT] & 2U) != 0U ? 2U : 0U;
    s |= stall_flag_ ? 4U : 0U;
    // stst: 2^20 clock cycles after the last step pulse (page 56)
    s |= ((v_hz_ == 0.0) && (still_us_ >= stillLimitUs())) ? 8U : 0U;
    s |= (rampStat() & 0x100U) != 0U ? 16U : 0U;
    s |= positionReached() ? 32U : 0U;
    s |= ref_left_ ? 64U : 0U;
    s |= ref_right_ ? 128U : 0U;
    return static_cast<uint8_t>(s);
  }
  /// UART recovery time the chip needs, in microseconds.
  [[nodiscard]] double RecoveryUs() const noexcept {
    return static_cast<double>(k.uart_recovery_bits) * 1e6 / static_cast<double>(k.baud);
  }

private:
  static bool isReadable(uint8_t a) noexcept {
    switch (a) {
    case m::GCONF:
    case m::GSTAT:
    case m::IFCNT:
    case m::IOIN:
    case m::OTP_READ:
    case m::FACTORY_CONF:
    case m::OFFSET_READ:
    case m::TSTEP:
    case m::RAMPMODE:
    case m::XACTUAL:
    case m::VACTUAL:
    case m::XTARGET:
    case m::SW_MODE:
    case m::RAMP_STAT:
    case m::XLATCH:
    case m::ENCMODE:
    case m::X_ENC:
    case m::ENC_STATUS:
    case m::ENC_LATCH:
    case m::MSCNT:
    case m::MSCURACT:
    case m::CHOPCONF:
    case m::DRV_STATUS:
    case m::PWM_SCALE:
    case m::PWM_AUTO:
    case m::LOST_STEPS:
      return true;
    default:
      return false;
    }
  }

  [[nodiscard]] uint32_t writeMask(uint8_t a) const noexcept {
    switch (a) {
    case m::GCONF:
      return 0x3FFFFU;
    case m::GSTAT:
      return 7U;
    case m::SLAVECONF:
      return 0xF00U | 0xFFU;
    case m::IOIN:
      return 1U;
    case m::X_COMPARE:
      return 0xFFFFFFFFU;
    case m::SHORT_CONF:
      return 0x00070F0FU | 0x00030000U;
    case m::DRV_CONF:
      return 0x003F0F1FU;
    case m::GLOBAL_SCALER:
      return 0xFFU;
    case m::IHOLD_IRUN:
      return 0x000F1F1FU;
    case m::TPOWERDOWN:
      return 0xFFU;
    case m::TPWMTHRS:
    case m::TCOOLTHRS:
    case m::THIGH:
      return 0xFFFFFU;
    case m::RAMPMODE:
      return 3U;
    case m::XACTUAL:
    case m::XTARGET:
    case m::X_ENC:
    case m::ENC_CONST:
      return 0xFFFFFFFFU;
    case m::VSTART:
    case m::VSTOP:
      return 0x3FFFFU;
    case m::A1:
    case m::AMAX:
    case m::DMAX:
    case m::D1:
    case m::TZEROWAIT:
      return 0xFFFFU;
    case m::V1:
      return 0xFFFFFU;
    case m::VMAX:
      return 0x7FFFFFU;
    case m::VDCMIN:
      return k.vdcmin_range ? 0x7FFFFFU : 0x3FFFFFU;
    case m::SW_MODE:
      return 0xFFFU;
    case m::RAMP_STAT:
      return 0x10CCU;
    case m::ENCMODE:
      return 0x7FFU;
    case m::ENC_STATUS:
      return 3U;
    case m::ENC_DEVIATION:
      return 0xFFFFFU;
    case m::MSLUTSEL:
      return 0xFFFFFFFFU;
    case m::MSLUTSTART:
      return 0x00FF00FFU;
    case m::CHOPCONF:
    case m::PWMCONF:
      return 0xFFFFFFFFU;
    case m::COOLCONF:
      return 0x017FEF6FU;
    case m::DCCTRL:
      return 0x00FF03FFU;
    default:
      break;
    }
    if ((a >= m::MSLUT0) && (a <= (m::MSLUT0 + 7U))) {
      return 0xFFFFFFFFU;
    }
    return 0U;
  }

  [[nodiscard]] bool stealthOn() const noexcept {
    return (r_[m::GCONF] & 4U) != 0U;
  }

  void notePeak() noexcept {
    if (!DriverOn()) {
      return;
    }
    const double run = RunMa();
    const double hold = HoldMa();
    peak_coil_ma = std::max({peak_coil_ma, run, hold});
  }

  [[nodiscard]] int32_t vactualRaw() const noexcept {
    const double units = v_hz_ * 16777216.0 / static_cast<double>(k.fclk_hz);
    return static_cast<int32_t>(std::llround(units));
  }

  [[nodiscard]] uint32_t rampStat() const noexcept {
    uint32_t v = ramp_events_ & 0x10FCU;
    v |= ref_left_ ? 1U : 0U;
    v |= ref_right_ ? 2U : 0U;
    v |= ((v_hz_ != 0.0) && (std::fabs(v_hz_) >= targetHz()) && (targetHz() > 0.0)) ? 0x100U : 0U;
    v |= positionReached() ? 0x200U : 0U;
    v |= v_hz_ == 0.0 ? 0x400U : 0U;
    v |= zw_left_ > 0.0 ? 0x800U : 0U; // t_zerowait_active (page 44)
    v |= stall_flag_ ? 0x2000U : 0U;
    return v;
  }

  [[nodiscard]] uint32_t drvStatus() const noexcept {
    uint32_t v = sg_result_;
    v |= ((r_[m::IHOLD_IRUN] >> 8) & 0x1FU) << 16;
    v |= stall_flag_ ? (1U << 24) : 0U;
    v |= ot_ ? (1U << 25) : 0U;
    v |= otpw_ ? (1U << 26) : 0U;
    v |= s2g_ ? (1U << 27) : 0U;
    v |= v_hz_ == 0.0 ? (1U << 31) : 0U;
    return v;
  }

  [[nodiscard]] uint32_t msCurAct() const noexcept {
    const uint32_t i = static_cast<uint32_t>(xactual_) & 1023U;
    const uint32_t q = i >> 8;
    const uint32_t n = i & 255U;
    int a = 0;
    int b = 0;
    const int sin = TableEntry(n);
    const int cos = TableEntry(256U - n);
    switch (q) {
    case 0:
      a = sin;
      b = cos;
      break;
    case 1:
      a = cos;
      b = -sin;
      break;
    case 2:
      a = -sin;
      b = -cos;
      break;
    default:
      a = -cos;
      b = sin;
      break;
    }
    if (k.mscuract_swap) {
      const int t = a;
      a = b;
      b = t;
    }
    return ((static_cast<uint32_t>(a) & 0x1FFU) << 16) | (static_cast<uint32_t>(b) & 0x1FFU);
  }

  [[nodiscard]] double vmaxHz() const noexcept {
    return static_cast<double>(r_[m::VMAX]) * static_cast<double>(k.fclk_hz) / 16777216.0;
  }
  [[nodiscard]] double targetHz() const noexcept {
    const uint32_t mode = r_[m::RAMPMODE] & 3U;
    if (mode == 3U) {
      return std::fabs(v_hz_);
    }
    return vmaxHz();
  }
  [[nodiscard]] double unitHz(uint32_t reg) const noexcept {
    return static_cast<double>(reg) * static_cast<double>(k.fclk_hz) / 16777216.0;
  }
  [[nodiscard]] double unitAcc(uint32_t reg) const noexcept {
    const auto f = static_cast<double>(k.fclk_hz);
    return static_cast<double>(reg) * f * f / 2199023255552.0; // 2^41
  }

  [[nodiscard]] double stillLimitUs() const noexcept {
    return 1048576.0 * 1e6 / static_cast<double>(k.fclk_hz);
  }

  void step(double dt) noexcept {
    notePeak();
    if (v_hz_ == 0.0) {
      still_us_ += dt * 1e6;
    } else {
      still_us_ = 0.0;
    }
    // StealthChop autotune window (page 58): at standstill, IHOLD = IRUN, for AT#1.
    if (DriverOn() && stealthOn() && !at1_done && !at1_failed) {
      if (v_hz_ == 0.0) {
        at1_us_ += dt * 1e6;
        const double clocks = at1_us_ * static_cast<double>(k.fclk_hz) / 1e6;
        const double need = (1048576.0 + 2.0 * 262144.0) * (k.at_a_version ? 1.0 : 2.0);
        if (clocks >= need) {
          at1_done = true;
        }
      }
    }
    compare_pulse_ = false;
    ramp_events_ &= ~0x30U; // the switch stop events follow the condition (page 53)
    if (directMode()) {
      return;
    }
    const uint32_t mode = r_[m::RAMPMODE] & 3U;
    if ((mode == 0U) && (zw_left_ > 0.0)) { // TZEROWAIT: standstill before the next move (page 83)
      zw_left_ = std::max(zw_left_ - dt, 0.0);
      v_hz_ = 0.0;
      return;
    }
    const double vmax = vmaxHz();
    const double v1 = unitHz(r_[m::V1]);
    const double vstart = unitHz(r_[m::VSTART]);
    const double vstop = unitHz(r_[m::VSTOP]);
    double target = 0.0;
    double speed = std::fabs(v_hz_);
    const bool was_moving = speed > 0.0;
    bool homing_in = false; // position mode, moving towards XTARGET with a speed allowed
    bool braking = false;   // position mode, slowing for the target or a reversal (DMAX, D1, VSTOP)
    double to_go = 0.0;     // position mode: distance left to XTARGET
    double dir = (mode == 2U) ? -1.0 : 1.0;
    if (mode == 3U) {
      integrate(dt);
      return;
    }
    if ((mode == 1U) || (mode == 2U)) {
      target = vmax;
      if ((speed > 0.0) && ((v_hz_ < 0.0) != (dir < 0.0))) {
        // moving against the mode's direction: down through 0 at AMAX first (page 83)
        target = 0.0;
        dir = (v_hz_ < 0.0) ? -1.0 : 1.0;
      }
    } else {
      const double dist =
          static_cast<double>(static_cast<int64_t>(xtarget_) - static_cast<int64_t>(xactual_)) -
          pos_frac_;
      dir = dist >= 0.0 ? 1.0 : -1.0;
      // Speed up only if the ramp could still stop in time after one more step of acceleration;
      // otherwise a ramp near its target would speed up and slow down by turns (A1 above D1).
      const double up = ((v1 > 0.0) && (speed < v1)) ? unitAcc(r_[m::A1]) : unitAcc(r_[m::AMAX]);
      const double next = speed + (up * dt);
      const double stop_dist = stopDistance(next, v1) + ((next - speed) * dt);
      // XACTUAL is a whole microstep: at the target (or within half a step) at VSTOP the ramp ends
      if (((xactual_ == xtarget_) || (std::fabs(dist) < 0.5)) && (speed <= (vstop + 1.0))) {
        arrive(was_moving);
        return;
      }
      braking = std::fabs(dist) <= (stop_dist + (speed * dt));
      target = braking ? 0.0 : vmax;
      if ((speed > 0.0) && ((v_hz_ < 0.0) != (dir < 0.0))) {
        braking = true;
        target = 0.0; // the target is on the other side: stop first, in the moving direction
        dir = (v_hz_ < 0.0) ? -1.0 : 1.0;
      } else {
        homing_in = vmax > 0.0;
        to_go = std::fabs(dist);
      }
    }
    // reference switches and stall
    if (stopByInputs(dir)) {
      v_hz_ = 0.0;
      integrate(dt);
      return;
    }
    if ((speed == 0.0) && (target > 0.0)) {
      speed = vstart > 0.0 ? vstart : 0.0;
      v_hz_ = dir * speed;
    }
    // Velocity mode always uses AMAX to accelerate and to decelerate (page 83,
    // section 12.2.3); A1, D1 and DMAX belong to the positioning mode.
    const bool velocity_mode = mode != 0U;
    double accel = unitAcc(r_[m::AMAX]);
    double decel = unitAcc(r_[m::AMAX]);
    if (!velocity_mode) {
      // DMAX and D1 only for the target or a reversal; a lower VMAX (VMAX 0 included) is followed
      // with A1 and AMAX (page 84, joystick control).
      const bool low = (v1 > 0.0) && (speed <= v1);
      accel = ((v1 > 0.0) && (speed < v1)) ? unitAcc(r_[m::A1]) : unitAcc(r_[m::AMAX]);
      if (braking) {
        decel = low ? unitAcc(r_[m::D1]) : unitAcc(r_[m::DMAX]);
      } else {
        decel = low ? unitAcc(r_[m::A1]) : unitAcc(r_[m::AMAX]);
      }
    }
    if (speed < target) {
      if ((r_[m::AMAX] == 0U) && (mode != 0U)) {
        // no acceleration: the chip does not move in velocity mode with AMAX 0
      } else {
        speed = std::min(speed + (accel * dt), target);
      }
    } else if (speed > target) {
      speed = std::max(speed - (decel * dt), target);
    } else {
      // speed equals the target: nothing to do
    }
    if (braking && (speed <= vstop)) {
      // VSTOP is the end of a position ramp's braking only (pages 83, 84): it goes on at VSTOP to
      // its target (page 82), or stops there before a reversal. Other ramps run down to 0 at
      // their acceleration.
      speed = homing_in ? std::max(vstop, 1.0) : 0.0;
    }
    if (homing_in && (speed <= (vstop + 1.0)) && ((speed * dt) >= to_go)) {
      arrive(true); // reaches XTARGET within this step
      return;
    }
    v_hz_ = dir * speed;
    integrate(dt);
    if ((mode == 0U) && was_moving && (speed == 0.0)) {
      // the ramp ended in standstill: the pause starts (page 83, option c)
      zw_left_ =
          static_cast<double>(r_[m::TZEROWAIT] & 0xFFFFU) * 512.0 / static_cast<double>(k.fclk_hz);
    }
  }

  /// A position ramp at its target: standstill, position_reached and, after a move, the
  /// TZEROWAIT pause before the next move (pages 41, 83).
  void arrive(bool moved) noexcept {
    xactual_ = xtarget_;
    pos_frac_ = 0.0;
    v_hz_ = 0.0;
    pos_reached_ = true;
    ramp_events_ |= 0x80U;
    if (moved) {
      zw_left_ =
          static_cast<double>(r_[m::TZEROWAIT] & 0xFFFFU) * 512.0 / static_cast<double>(k.fclk_hz);
    }
  }

  void watch(double dt_us) noexcept {
    min_x = std::min(min_x, xactual_);
    max_x = std::max(max_x, xactual_);
    min_v = std::min(min_v, v_hz_);
    max_v = std::max(max_v, v_hz_);
    if (first_v == 0.0) {
      first_v = std::fabs(v_hz_);
    }
    const bool position = (r_[m::RAMPMODE] & 3U) == 0U;
    if (position && target_stale_ && (v_hz_ != 0.0) && !directMode()) {
      stale_target_us += dt_us;
    }
  }

  [[nodiscard]] uint32_t blankWord(uint8_t address, uint32_t value) const noexcept {
    if (!blank_reads || ((address & 0x7FU) == m::IOIN)) {
      return value;
    }
    return ((address & 0x7FU) == blank_keep) ? blank_value : 0U;
  }
  [[nodiscard]] uint8_t blankStatus(uint8_t status) const noexcept {
    if (!blank_reads) {
      return status;
    }
    return (blank_keep == 0x80U) ? static_cast<uint8_t>(blank_value) : uint8_t{0};
  }

  /// The distance a position ramp needs to stop from `speed`: DMAX down to V1, then D1 (page 40).
  [[nodiscard]] double stopDistance(double speed, double v1) const noexcept {
    const double dmax = unitAcc(r_[m::DMAX]);
    const double d1 = unitAcc(r_[m::D1]);
    if ((v1 <= 0.0) || (d1 <= 0.0)) {
      return (dmax > 0.0) ? ((speed * speed) / (2.0 * dmax)) : 0.0;
    }
    if (speed <= v1) {
      return (speed * speed) / (2.0 * d1);
    }
    const double upper = (dmax > 0.0) ? (((speed * speed) - (v1 * v1)) / (2.0 * dmax)) : 0.0;
    return upper + ((v1 * v1) / (2.0 * d1));
  }

  /// position_reached: XACTUAL equals XTARGET (page 44).
  [[nodiscard]] bool positionReached() const noexcept {
    return pos_reached_ || (xactual_ == xtarget_);
  }

  [[nodiscard]] bool directMode() const noexcept {
    return (r_[m::GCONF] & (1U << 16)) != 0U;
  }

  /// Reference switches and the StallGuard stop (pages 43, 53, 90): the motor holds at zero while
  /// the stop applies.
  bool stopByInputs(double dir) noexcept {
    const uint32_t sw = r_[m::SW_MODE];
    const bool left_active = ((sw & 4U) != 0U) ? !ref_left_ : ref_left_;
    const bool right_active = ((sw & 8U) != 0U) ? !ref_right_ : ref_right_;
    if (((sw & 1U) != 0U) && left_active && (dir < 0.0)) {
      ramp_events_ |= 0x10U;
      return true;
    }
    if (((sw & 2U) != 0U) && right_active && (dir > 0.0)) {
      ramp_events_ |= 0x20U;
      return true;
    }
    if ((sw & (1U << 10)) != 0U) {
      if (stall_flag_ && (v_hz_ != 0.0)) {
        ramp_events_ |= 0x40U;
      }
      return (ramp_events_ & 0x40U) != 0U; // no restart before the event is cleared
    }
    return false;
  }

  void integrate(double dt) noexcept {
    pos_frac_ += v_hz_ * dt;
    const double whole = std::trunc(pos_frac_);
    if (whole != 0.0) {
      xactual_ = static_cast<int32_t>(static_cast<int64_t>(xactual_) + static_cast<int64_t>(whole));
      pos_frac_ -= whole;
    }
    const auto xa = static_cast<uint32_t>(xactual_);
    if (k.xcompare_pulse && (xa == r_[m::X_COMPARE]) && (r_[m::X_COMPARE] != 0U)) {
      compare_pulse_ = true;
    }
  }

  std::array<uint32_t, 128> r_{};
  std::array<uint32_t, 128> last_written_{};
  int32_t xactual_ = 0;
  int32_t xtarget_ = 0;
  uint32_t direct_target_ = 0;
  double v_hz_ = 0.0;
  double pos_frac_ = 0.0;
  bool drv_enn_low_ = true; ///< DRV_ENN is low (enabled) unless the board drives it high.
  bool uart_mode_ = false;
  bool ref_left_ = false;
  bool ref_right_ = false;
  bool stall_flag_ = false;
  bool ot_ = false;
  bool otpw_ = false;
  bool s2g_ = false;
  bool pos_reached_ = false;
  bool compare_pulse_ = false;
  bool target_stale_ = false; ///< XTARGET was written before the last switch to velocity mode.
  uint32_t ramp_events_ = 0;
  uint32_t sg_result_ = 0;
  int offset_a_ = 0;
  int offset_b_ = 0;
  int32_t x_enc_ = 0;
  uint32_t enc_status_ = 0;
  int32_t enc_latch_ = 0;
  int32_t xlatch_ = 0;
  uint8_t status_pending_ = 0;
  uint32_t reply_pending_ = 0;
  double at1_us_ = 0.0;
  double zw_left_ = 0.0;  ///< TZEROWAIT time left, seconds.
  double still_us_ = 1e9; ///< Time since the last step pulse, microseconds.
  double uart_recover_us_ = 0.0;
};

} // namespace tmc5160_test

#endif // TMC5160_MOCK_TMC5160_HPP
