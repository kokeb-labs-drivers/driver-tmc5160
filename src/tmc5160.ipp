/**
 * @file tmc5160.ipp
 * @brief TMC5160 driver implementation, included by `tmc5160.hpp`.
 * @copyright Copyright (c) 2026 The driver-tmc5160 authors. SPDX-License-Identifier: MIT
 *
 * Section numbers and pages refer to the TMC5160 / TMC5160A datasheet, Rev. 1.18.
 */
#ifndef TMC5160_IPP
#define TMC5160_IPP

namespace tmc5160 {

namespace detail {

/// A bare `Error` as a `Result<void>`.
[[nodiscard]] inline Result<void> ToResult(Error e) noexcept {
  return (e == Error::Ok) ? Result<void>::Success() : Result<void>::Failure(e);
}

/// The address list a setter writes: at most `kMaxWrites` registers.
inline constexpr std::size_t kMaxWrites = 24;

/// IFCNT wraps at 8 bits (page 33).
inline constexpr uint32_t kCounterMask = 0xFFU;

/// Bits of GSTAT and of the write-to-clear registers (pages 33, 44, 45).
inline constexpr uint32_t kGstatAll = 0x7U;
inline constexpr uint32_t kRampClearMask = 0x10CCU;
inline constexpr uint32_t kEncClearMask = 0x3U;
static_assert(reg::FindReadable(reg::kGstat)->w1c == kGstatAll,
              "GSTAT clear mask follows the table");
static_assert(reg::FindReadable(reg::kRampStat)->w1c == kRampClearMask,
              "RAMP_STAT clear mask follows the table");
static_assert(reg::FindReadable(reg::kEncStatus)->w1c == kEncClearMask,
              "ENC_STATUS clear mask follows the table");

/// The largest coil current in direct mode (page 110).
inline constexpr int32_t kDirectMax = 248;
inline constexpr int32_t kDirectMaxVoltage = 255;

/// Absolute value of a 32-bit integer as 64 bits (no overflow for the minimum).
[[nodiscard]] constexpr int64_t Abs64(int32_t v) noexcept {
  return (v < 0) ? -static_cast<int64_t>(v) : static_cast<int64_t>(v);
}

} // namespace detail

// -------------------------------------------------------------------------------------------------
// Bus layer: SPI datagrams (section 4) and UART datagrams (section 5)
// -------------------------------------------------------------------------------------------------

template <AnyPort Port>
void TMC5160<Port>::noteReset() noexcept {
  // Every write-only register is lost (page 119). The driver is no longer
  // initialised, but the DRV_ENN pin it raised is still raised: the switch-off
  // stays possible (started_).
  initialized_ = false;
  watch_reset_ = false;
  enabled_ = false;
  // The output is made safe at once: a status byte has no integrity check, so the pin is dropped
  // before anything else is believed (the chip's TOFF is rewritten by the next start-up).
  static_cast<void>(setPin(CtrlPin::DrvEnn, false));
}

template <AnyPort Port>
bool TMC5160<Port>::beginSequence() noexcept {
  const bool watched = watch_reset_;
  watch_reset_ = false;
  reset_seen_ = false;
  return watched;
}

template <AnyPort Port>
Result<void> TMC5160<Port>::endSequence(bool watched, Result<void> result) noexcept {
  if (watched && reset_seen_) {
    noteReset();
    detail::Log(port_, LogLevel::Error, "tmc5160: the chip reset");
    return result ? Result<void>::Failure(Error::UnexpectedReset) : result;
  }
  if (watched && initialized_) {
    watch_reset_ = true;
  }
  return result;
}

template <AnyPort Port>
Error TMC5160<Port>::observe(uint8_t status) noexcept {
  last_status_ = status;
  const uint32_t s = status;
  reset_seen_ = reset_seen_ || ((s & 1U) != 0U);
  fault_ = fault_ || ((s & 2U) != 0U); // driver_error mirrors GSTAT.drv_err (page 24)
  if (watch_reset_ &&
      ((s & 1U) != 0U)) { // reset flag: every write-only register is lost (page 119)
    noteReset();
    detail::Log(port_, LogLevel::Error, "tmc5160: the chip reset");
    return Error::UnexpectedReset;
  }
  return Error::Ok;
}

template <AnyPort Port>
Error TMC5160<Port>::datagram(uint8_t cmd, uint32_t value, uint32_t& reply) noexcept {
  if constexpr (kSpi) {
    const std::size_t chain = config_.spi.chain_length;
    const std::size_t slot = kDatagramBytes * (chain - 1U - config_.spi.chain_index);
    auto& tx = spi_tx_;
    auto& rx = spi_rx_;
    tx.fill(0U); // other chips' slots: a read of GCONF
    rx.fill(0U);
    tx[slot] = cmd;
    tx[slot + 1U] = static_cast<uint8_t>(value >> 24U);
    tx[slot + 2U] = static_cast<uint8_t>(value >> 16U);
    tx[slot + 3U] = static_cast<uint8_t>(value >> 8U);
    tx[slot + 4U] = static_cast<uint8_t>(value);
    if (!port_.Transfer(tx.data(), rx.data(), kDatagramBytes * chain)) {
      return Error::Bus;
    }
    reply = (static_cast<uint32_t>(rx[slot + 1U]) << 24U) |
            (static_cast<uint32_t>(rx[slot + 2U]) << 16U) |
            (static_cast<uint32_t>(rx[slot + 3U]) << 8U) | static_cast<uint32_t>(rx[slot + 4U]);
    return observe(rx[slot]);
  } else {
    static_cast<void>(cmd);
    static_cast<void>(value);
    static_cast<void>(reply);
    return Error::Unsupported;
  }
}

template <AnyPort Port>
void TMC5160<Port>::waitUartRecovery() noexcept {
  if constexpr (!kSpi) {
    // After an error the line must be idle for 12 bit times (page 26).
    const uint32_t us =
        static_cast<uint32_t>((uint64_t{kUartRecoveryBits} * 1'000'000U) / config_.uart.baud) + 1U;
    detail::DelayUs(port_, us);
  }
}

template <AnyPort Port>
Error TMC5160<Port>::uartExchange(const detail::UartFrame& tx, std::size_t tx_len,
                                  detail::UartFrame& rx, std::size_t rx_len) noexcept {
  if constexpr (!kSpi) {
    // The reply starts after SENDDELAY * 8 bit times (page 27); no timeout figure is given (A32).
    const uint32_t delay_bits =
        ((config_.uart.send_delay < 1U) ? 1U : config_.uart.send_delay) * 8U;
    const uint32_t bits = delay_bits + kUartReplyMarginBits + (10U * detail::kUartFrameLen);
    const uint32_t timeout_us =
        static_cast<uint32_t>((uint64_t{bits} * 1'000'000U) / config_.uart.baud) + 1U;
    switch (port_.Exchange(tx.data(), tx_len, rx.data(), rx_len, timeout_us)) {
    case BusStatus::Ok:
      return Error::Ok;
    case BusStatus::Timeout:
      return Error::Timeout;
    case BusStatus::Fault:
      return Error::Bus;
    default:
      break;
    }
    return Error::Bus;
  } else {
    static_cast<void>(tx);
    static_cast<void>(tx_len);
    static_cast<void>(rx);
    static_cast<void>(rx_len);
    return Error::Unsupported;
  }
}

template <AnyPort Port>
Error TMC5160<Port>::uartRead(uint8_t reg, uint32_t& value) noexcept {
  if constexpr (!kSpi) {
    Error last = Error::Bus;
    for (uint32_t attempt = 0U; attempt <= kUartReadRetries; ++attempt) {
      uart_tx_ = detail::UartReadFrame(node_, reg);
      uart_rx_.fill(0U);
      last = uartExchange(uart_tx_, detail::kUartRequestLen, uart_rx_, detail::kUartFrameLen);
      if (last == Error::Ok) {
        const uint32_t parsed = detail::ParseUartReply(uart_rx_, reg, value);
        last = (parsed == 0U) ? Error::Ok : ((parsed == 2U) ? Error::Crc : Error::Bus);
      }
      if (last == Error::Ok) {
        return Error::Ok;
      }
      waitUartRecovery(); // a read has no side effect: retry (A33)
    }
    return last;
  } else {
    static_cast<void>(reg);
    static_cast<void>(value);
    return Error::Unsupported;
  }
}

template <AnyPort Port>
Error TMC5160<Port>::uartWrite(uint8_t reg, uint32_t value) noexcept {
  if constexpr (!kSpi) {
    uart_tx_ = detail::UartWriteFrame(node_, reg, value);
    const Error e = uartExchange(uart_tx_, detail::kUartFrameLen, uart_rx_, 0U);
    if (e != Error::Ok) {
      waitUartRecovery(); // a write is never retried: its effect is unknown
      return e;
    }
    if (node_ == config_.uart.node) {
      uart_counter_ =
          (uart_counter_ + 1U) & detail::kCounterMask; // each accepted write counts (page 26)
    }
    return Error::Ok;
  } else {
    static_cast<void>(reg);
    static_cast<void>(value);
    return Error::Unsupported;
  }
}

template <AnyPort Port>
Error TMC5160<Port>::readMany(std::span<const uint8_t> addresses,
                              std::span<uint32_t> out) noexcept {
  if constexpr (kSpi) {
    uint32_t reply = 0U;
    for (std::size_t i = 0U; i < addresses.size();
         ++i) { // the data of read i arrives in datagram i + 1
      const Error e = datagram(addresses[i], 0U, reply);
      if (e != Error::Ok) {
        return e;
      }
      if (i > 0U) {
        out[i - 1U] = reply;
      }
    }
    const Error last =
        datagram(reg::kGconf, 0U, reply); // the pipeline flush: a harmless read of GCONF
    out[addresses.size() - 1U] = reply;
    return last;
  } else {
    for (std::size_t i = 0U; i < addresses.size(); ++i) {
      const Error e = uartRead(static_cast<uint8_t>(addresses[i] & 0x7FU), out[i]);
      if (e != Error::Ok) {
        return e;
      }
    }
    return Error::Ok;
  }
}

template <AnyPort Port>
Result<uint32_t> TMC5160<Port>::readReg(uint8_t address) noexcept {
  if (reg::FindReadable(address) == nullptr) {
    return Result<uint32_t>::Failure(
        Error::Refused); // a write-only register has no defined read (A4)
  }
  const std::array<uint8_t, 1> list{address};
  std::array<uint32_t, 1> out{};
  const Error e = readMany(list, out);
  return (e == Error::Ok) ? Result<uint32_t>::Success(out[0]) : Result<uint32_t>::Failure(e);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::checkVerify(Verify verify) const noexcept {
  const bool wrong_bus =
      ((verify == Verify::Echo) && !kSpi) || ((verify == Verify::Counter) && kSpi);
  return detail::ToResult(wrong_bus ? Error::Unsupported : Error::Ok);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::verifyWrite(uint8_t address, uint32_t word, Verify verify) noexcept {
  uint32_t reply = 0U;
  if (verify ==
      Verify::Echo) { // the reply of the next datagram mirrors the data just written (page 24, A5)
    const Error e = datagram(reg::kGconf, 0U, reply);
    return detail::ToResult((e != Error::Ok) ? e
                                             : ((reply == word) ? Error::Ok : Error::VerifyFailed));
  }
  if (verify == Verify::Counter) { // IFCNT counted this write (page 33)
    const Error e = uartRead(reg::kIfcnt, reply);
    const bool match = (reply & detail::kCounterMask) == uart_counter_;
    return detail::ToResult((e != Error::Ok) ? e : (match ? Error::Ok : Error::VerifyFailed));
  }
  const reg::RegInfo* info = reg::FindReadable(address);
  const reg::RegInfo* writable = reg::FindWritable(address);
  if ((verify != Verify::ReadBack) || (info == nullptr) || (writable == nullptr)) {
    return Result<void>::Success(); // a write-only register cannot be read back
  }
  const auto got = readReg(address);
  if (!got) {
    return Result<void>::Failure(got.error);
  }
  return detail::ToResult((((got.value ^ word) & writable->mask) == 0U) ? Error::Ok
                                                                        : Error::VerifyFailed);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::sendReg(uint8_t address, uint32_t value, Verify verify, uint32_t& word,
                                    uint32_t& reply) noexcept {
  const reg::RegInfo* info = reg::FindWritable(address);
  if (info == nullptr) {
    return Result<void>::Failure(Error::Refused); // read-only, forbidden or absent: nothing is sent
  }
  if (kSpi && ((address == reg::kNodeconf) || info->is_output)) {
    return Result<void>::Failure(
        Error::Refused); // NODECONF and OUTPUT exist in UART mode only (page 34)
  }
  if (auto r = checkVerify(verify); !r) {
    return r;
  }
  word = value & ((info->access == reg::Access::Clear) ? info->w1c : info->mask);
  if (address == reg::kGconf) {
    word &= ~reg::kGconfTestMode; // the analog test output is never set
  }
  reply = 0U;
  const Error e = kSpi ? datagram(static_cast<uint8_t>(reg::kWriteFlag | address), word, reply)
                       : uartWrite(address, word);
  if (e != Error::Ok) {
    return Result<void>::Failure(e);
  }
  if (info->access != reg::Access::Clear) {
    shadow_[address & 0x7FU] = word; // the write-through shadow of a write-only register (page 7)
  }
  return Result<void>::Success();
}

template <AnyPort Port>
Result<void> TMC5160<Port>::writeReg(uint8_t address, uint32_t value, Verify verify) noexcept {
  uint32_t word = 0U;
  uint32_t reply = 0U;
  if (auto r = sendReg(address, value, verify, word, reply); !r) {
    return r;
  }
  return verifyWrite(address, word, verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::writeRegs(std::span<const detail::Write> writes,
                                      Verify verify) noexcept {
  if (auto r = checkVerify(verify); !r) {
    return r;
  }
  if (kSpi && (verify == Verify::Echo) && !writes.empty()) {
    // The reply of every datagram mirrors the data of the write before it (page 24): each write
    // but the last is proven by the next one, the last by one more transfer.
    uint32_t previous = 0U;
    for (std::size_t i = 0U; i < writes.size(); ++i) {
      uint32_t word = 0U;
      uint32_t reply = 0U;
      if (auto r = sendReg(writes[i].address, writes[i].value, verify, word, reply); !r) {
        return r;
      }
      if ((i > 0U) && (reply != previous)) {
        return Result<void>::Failure(Error::VerifyFailed);
      }
      previous = word;
    }
    return verifyWrite(writes[writes.size() - 1U].address, previous, verify);
  }
  for (std::size_t i = 0U; i < writes.size(); ++i) {
    const bool last = (i + 1U) == writes.size();
    const bool check = last || (verify == Verify::ReadBack);
    if (auto r = writeReg(writes[i].address, writes[i].value, check ? verify : Verify::None); !r) {
      return r;
    }
  }
  return Result<void>::Success();
}

template <AnyPort Port>
Result<void> TMC5160<Port>::pushConfig(const Config& next, std::span<const uint8_t> which,
                                       Verify verify) noexcept {
  if (which.size() > detail::kMaxWrites) {
    return Result<void>::Failure(Error::InvalidArgument); // nothing is truncated silently
  }
  if (enabled_ && !intact_checked_) {
    // Writing TOFF switches the bridge on: after a chip reset or a lost write it would run at
    // the reset current registers, which can exceed the motor's rating (page 119).
    for (const uint8_t address : which) {
      if (address == reg::kChopconf) {
        if (auto r = checkIntact(); !r) {
          return r;
        }
      }
    }
  }
  return sendConfig(next, which, verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::sendConfig(const Config& next, std::span<const uint8_t> which,
                                       Verify verify) noexcept {
  if (which.size() > detail::kMaxWrites) {
    return Result<void>::Failure(Error::InvalidArgument); // nothing is truncated silently
  }
  std::array<detail::Write, detail::kMaxWrites> writes{};
  const detail::WordContext x{next, plan_, direct_mode_, enabled_, hold_at_run_};
  const std::size_t count = which.size();
  for (std::size_t i = 0U; i < count; ++i) {
    writes[i] = detail::Write{.address = which[i], .value = detail::WordFor(x, which[i])};
  }
  auto r = writeRegs(std::span<const detail::Write>{writes}.first(count), verify);
  if (r) {
    config_ = next;
  }
  return r;
}

template <AnyPort Port>
Result<void> TMC5160<Port>::guard(const Config& next) const noexcept {
  const Error e = Validate(next);
  return detail::ToResult((e != Error::Ok) ? e : CheckAcks(next));
}

template <AnyPort Port>
Result<void> TMC5160<Port>::ready() const noexcept {
  return detail::ToResult(initialized_ ? Error::Ok : Error::NotInitialized);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::readyOff() const noexcept {
  // A switch-off, a stop and `Deinitialize()` are never refused because the
  // driver reports a reset: they need only a started driver (one whose start-up
  // sent its first write).
  return detail::ToResult(started_ ? Error::Ok : Error::NotInitialized);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::readyStop() const noexcept {
  if (auto r = readyOff(); !r) {
    return r;
  }
  return detail::ToResult((config_.mode == Mode::MotionController) ? Error::Ok
                                                                   : Error::Unsupported);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::readyController() const noexcept {
  if (auto r = ready(); !r) {
    return r;
  }
  return detail::ToResult((config_.mode == Mode::MotionController) ? Error::Ok
                                                                   : Error::Unsupported);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::readyMotion() const noexcept {
  if (auto r = readyController(); !r) {
    return r;
  }
  if (fault_) {
    return Result<void>::Failure(Error::DeviceFault);
  }
  return detail::ToResult((direct_mode_ || direct_unsure_) ? Error::Refused : Error::Ok);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::setPin(CtrlPin pin, bool active) noexcept {
  if constexpr (detail::HasGpioSet<Port>) {
    return detail::ToResult(port_.GpioSet(pin, active ? GpioSignal::ACTIVE : GpioSignal::INACTIVE)
                                ? Error::Ok
                                : Error::Bus);
  } else {
    static_cast<void>(pin);
    static_cast<void>(active);
    return Result<void>::Success();
  }
}

template <AnyPort Port>
Result<uint32_t> TMC5160<Port>::readChecked(uint8_t address) noexcept {
  if (auto r = ready(); !r) {
    return Result<uint32_t>::Failure(r.error);
  }
  return readReg(address);
}

template <AnyPort Port>
Result<uint32_t> TMC5160<Port>::readControlled(uint8_t address) noexcept {
  if (auto r = readyController(); !r) {
    return Result<uint32_t>::Failure(r.error);
  }
  return readReg(address);
}

template <AnyPort Port>
bool TMC5160<Port>::stuckLow(uint32_t value) const noexcept {
  if constexpr (kSpi) {
    // A MISO line stuck low reads all zeros, status byte included; a running
    // chip at rest still reports its standstill flag in the status byte (page 24).
    return (value == 0U) && (last_status_ == 0U);
  } else {
    static_cast<void>(value);
    return false;
  }
}

template <AnyPort Port>
Error TMC5160<Port>::lineCheck(uint32_t value) noexcept {
  if (!stuckLow(value)) {
    return Error::Ok;
  }
  // All zeros with an all-zero status byte: a line stuck low, or a live chip that has no flag set
  // at this moment (a ramp that runs before `stst` is set, pages 24, 56). IOIN.VERSION decides.
  const auto ioin = readReg(reg::kIoin);
  if (!ioin) {
    return ioin.error;
  }
  return (detail::DecodeInputs(ioin.value).version == reg::kVersion) ? Error::Ok : Error::Bus;
}

template <AnyPort Port>
Result<void> TMC5160<Port>::ensureStopped() noexcept {
  // One RAMP_STAT read decides: vzero (VACTUAL is 0) and t_zerowait_active come from the same
  // word, so a ramp that ended in position mode and pauses for TZEROWAIT before it runs back to
  // its target (pages 44, 83 c) cannot slip between two reads.
  const auto ramp = readReg(reg::kRampStat);
  if (!ramp) {
    return Result<void>::Failure(ramp.error);
  }
  if (const Error e = lineCheck(ramp.value); e != Error::Ok) {
    return Result<void>::Failure(e);
  }
  if (((ramp.value & reg::kRampZeroWaitActive) != 0U) || ((ramp.value & reg::kRampVzero) == 0U)) {
    return Result<void>::Failure(Error::Busy);
  }
  // Position mode with a speed: vzero is also set for an instant where the ramp turns back to a
  // target it passed (TZEROWAIT 0, or the end of a pause with VSTART 0); only position_reached
  // tells that the ramp is done (pages 44, 83 c).
  const bool position = shadow_[reg::kRampmode] == static_cast<uint32_t>(RampMode::Position);
  if (position && (shadow_[reg::kVmax] != 0U) && ((ramp.value & reg::kRampPositionReached) == 0U)) {
    return Result<void>::Failure(Error::Busy);
  }
  return Result<void>::Success();
}

template <AnyPort Port>
Result<void> TMC5160<Port>::writeChopconf(Verify verify) noexcept {
  const std::array<uint8_t, 1> which{reg::kChopconf};
  return pushConfig(config_, which, verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::writeOff(Verify verify) noexcept {
  // The switch-off write (TOFF 0, `enabled_` already false): it never needs the intact check of
  // a switch-on, and `checkIntact()` itself switches off through it, so it goes straight to the
  // bus (no call path back into `pushConfig()`, MISRA C++:2023 Rule 8.2.10).
  const std::array<uint8_t, 1> which{reg::kChopconf};
  return sendConfig(config_, which, verify);
}

template <AnyPort Port>
constexpr Verify TMC5160<Port>::offVerify(Verify verify) noexcept {
  // A UART write has no reply: the interface counter is the only proof that TOFF = 0 arrived. A
  // switching-off write is never refused for the verification the caller named: a mode the bus
  // cannot do becomes the bus's own (the bridge must go off whatever the argument says).
  if (kSpi) {
    return (verify == Verify::Counter) ? Verify::Echo : verify;
  }
  return ((verify == Verify::None) || (verify == Verify::Echo)) ? Verify::Counter : verify;
}

template <AnyPort Port>
constexpr Verify TMC5160<Port>::busVerify(Verify verify) noexcept {
  if (kSpi) {
    return (verify == Verify::Counter) ? Verify::Echo : verify;
  }
  return (verify == Verify::Echo) ? Verify::Counter : verify;
}

template <AnyPort Port>
Result<void> TMC5160<Port>::rampStop(bool immediate, bool zero_coils, Verify verify) noexcept {
  // VMAX 0 and velocity mode first: a chip in position mode whose mode was only in doubt must not
  // be sent to XTARGET 0 at the old speed. The coil currents of direct mode (page 110) are zeroed
  // by the last write.
  std::array<detail::Write, 4> w{};
  std::size_t n = 0U;
  if (immediate) {
    w[n++] = detail::Write{.address = reg::kVstart, .value = 0U};
  }
  w[n++] = detail::Write{.address = reg::kVmax, .value = 0U};
  w[n++] = detail::Write{.address = reg::kRampmode,
                         .value = static_cast<uint32_t>(RampMode::VelocityPositive)};
  if (zero_coils) {
    w[n++] = detail::Write{.address = reg::kXtarget, .value = 0U};
  }
  return writeRegs(std::span<const detail::Write>{w}.first(n), verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::quiesce(Verify verify) noexcept {
  const bool watched = beginSequence();
  const Result<void> pin =
      setPin(CtrlPin::DrvEnn, false); // DRV_ENN first: the hardware switch (page 106)
  enabled_ = false;
  off_pending_ = true;
  const Result<void> off = writeOff(offVerify(verify));
  off_pending_ = !off || !pin;
  // The ramp generator keeps running with the bridge off: a later `Enable()` must not find it
  // moving (pages 83, 106). Best effort after the switch-off; the caller learns of a failure.
  Result<void> stop = Result<void>::Success();
  if (started_ && (config_.mode == Mode::MotionController)) {
    stop = rampStop(true, direct_mode_ || direct_unsure_, busVerify(verify));
  }
  return endSequence(watched, pin ? (off ? stop : off) : pin);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::checkIntact() noexcept {
  // A chip that browned out has lost every write-only register: its reset current scale can exceed
  // the motor's rating (page 119). The reset flag in GSTAT is read before the bridge is switched
  // on.
  const auto status = ReadGlobalStatus();
  if (!status) {
    return Result<void>::Failure(status.error);
  }
  if (status.value.drv_err) {
    return Result<void>::Failure(Error::DeviceFault);
  }
  if constexpr (kSpi) {
    // SPI has no counter: IOIN.VERSION shows a chip that answers (a stuck line reads all zeros).
    if (const auto pins = ReadInputs(); !pins) {
      return Result<void>::Failure(pins.error);
    }
  }
  if constexpr (!kSpi) {
    // A UART write has no reply: IFCNT shows whether the chip accepted every write the driver sent
    // (page 33). A lost current or chopper write would leave the reset values in the chip.
    uint32_t count = 0U;
    if (const Error e = uartRead(reg::kIfcnt, count); e != Error::Ok) {
      return Result<void>::Failure(e);
    }
    if ((count & detail::kCounterMask) != uart_counter_) {
      static_cast<void>(
          quiesce()); // the chip's registers are not what the driver sent: switch the bridge off
      return fail(Error::VerifyFailed, "tmc5160: the chip missed a write; start again");
    }
  }
  return Result<void>::Success();
}

// -------------------------------------------------------------------------------------------------
// Lifecycle (section 25, page 119: after a reboot set TOFF = 0, rewrite all registers, stop the
// ramp)
// -------------------------------------------------------------------------------------------------

template <AnyPort Port>
Result<void> TMC5160<Port>::fail(Error error, const char* message) noexcept {
  detail::Log(port_, LogLevel::Error, message);
  initialized_ = false;
  watch_reset_ = false;
  return Result<void>::Failure(error);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::EnsureInitialized() noexcept {
  if (initialized_) {
    return Result<void>::Success();
  }
  const auto r = initialize();
  if (r) {
    detail::Log(port_, LogLevel::Info, "tmc5160: initialised");
  }
  return r;
}

template <AnyPort Port>
Result<void> TMC5160<Port>::initialize() noexcept {
  const Config config = config_;
  if (auto r = guard(config); !r) {
    return fail(r.error, "tmc5160: invalid configuration");
  }
  const auto plan = units::PlanCurrent(config, config.current.run_ma, config.current.hold_ma);
  if (!plan) {
    return fail(plan.error, "tmc5160: the current plan fails a guard");
  }
  if (units::StealthGuard(config, plan.value) != Error::Ok) {
    return fail(Error::Refused, "tmc5160: StealthChop needs a higher run current");
  }
  if constexpr (detail::HasSckHz<Port>) {
    const uint32_t limit =
        config.clock.external ? (((config.clock.hz / 2U) / 10U) * 9U) : kMaxSckInternalHz;
    if (port_.SckHz() > limit) {
      return fail(Error::Unsupported, "tmc5160: the SPI clock is above the chip's limit");
    }
  }
  if (!port_.EnsureInitialized()) {
    return fail(Error::Bus, "tmc5160: the port could not bring the bus up");
  }
  shadow_.fill(0U);
  for (const reg::RegInfo& row : reg::kTable) {
    shadow_[row.address & 0x7FU] = row.reset;
  }
  plan_ = plan.value;
  ramp_written_ = false;
  node_ = config.uart.node;
  enabled_ = false;
  off_pending_ = false;
  fault_ = false;
  watch_reset_ = false;
  direct_mode_ = false;
  direct_unsure_ =
      true; // a chip that stayed powered may still be in direct mode until GCONF is written
  hold_at_run_ = false;
  last_status_ = 0U;
  waitUartRecovery();
  return startUp(config);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::checkLink(const Config& config) noexcept {
  const auto ioin = readReg(reg::kIoin);
  if (!ioin) {
    return fail(ioin.error, "tmc5160: IOIN could not be read");
  }
  const InputPins pins = detail::DecodeInputs(ioin.value);
  if (pins.version != reg::kVersion) {
    return fail(Error::DeviceIdMismatch, "tmc5160: IOIN.VERSION is not 0x30");
  }
  if (pins.sd_mode != (config.mode == Mode::StepDir)) {
    return fail(Error::ModeMismatch, "tmc5160: the SD_MODE strap does not match the configuration");
  }
  report_ = BeginReport{};
  report_.version = pins.version;
  report_.sd_mode = pins.sd_mode;
  return Result<void>::Success();
}

template <AnyPort Port>
Result<void> TMC5160<Port>::clearGstat() noexcept {
  const auto before = readReg(reg::kGstat);
  if (!before) {
    return fail(before.error, "tmc5160: GSTAT could not be read");
  }
  report_.uv_cp_seen = detail::DecodeGlobal(before.value).uv_cp;
  if (auto r = writeReg(reg::kGstat, detail::kGstatAll, Verify::None); !r) {
    return fail(r.error, "tmc5160: GSTAT could not be cleared");
  }
  const auto after = readReg(reg::kGstat);
  if (!after) {
    return fail(after.error, "tmc5160: GSTAT could not be read back");
  }
  return detail::DecodeGlobal(after.value).drv_err
             ? fail(Error::DeviceFault, "tmc5160: the driver reports an error")
             : Result<void>::Success();
}

template <AnyPort Port>
Result<void> TMC5160<Port>::startUp(const Config& config) noexcept {
  config_ = config;
  config_.verify = Verify::None; // the quiesce write is the first write; UART counts it
  started_ = true;               // from here on a switch-off and a stop are owed to the chip
  if (auto r = setPin(CtrlPin::DrvEnn, false); !r) {
    return fail(r.error, "tmc5160: DRV_ENN could not be driven");
  }
  if constexpr (!kSpi) { // the counter the first write is checked against (read: nothing is
                         // energised)
    const auto count = readReg(reg::kIfcnt);
    if (!count) {
      return fail(count.error, "tmc5160: IFCNT could not be read");
    }
    uart_counter_ = count.value & detail::kCounterMask;
  }
  // TOFF = 0 before anything else (page 119); on UART the counter shows that it arrived.
  if (auto r = writeChopconf(offVerify(Verify::None)); !r) {
    return fail(r.error, "tmc5160: the first write failed");
  }
  config_ = config;
  if (auto r = checkLink(config); !r) {
    return r;
  }
  if (auto r = clearGstat(); !r) {
    return r;
  }
  if (auto r = writeAll(config); !r) {
    return fail(r.error, "tmc5160: a configuration write failed");
  }
  report_.advisories = Advise(config);
  report_.current_run_ma = plan_.run_ma;
  report_.current_hold_ma = plan_.hold_ma;
  fault_ = false;
  initialized_ = true;
  watch_reset_ = true;
  if (config.begin_enabled) {
    if (const auto on = Enable(Verify::None); !on) {
      // A start-up that fails leaves the bridge off and the driver not started.
      static_cast<void>(quiesce());
      return fail(on.error, "tmc5160: the bridge could not be enabled at start-up");
    }
  }
  return Result<void>::Success();
}

template <AnyPort Port>
Result<void> TMC5160<Port>::writeTable(const Config& config) noexcept {
  const auto mscnt = readReg(reg::kMscnt);
  if (!mscnt) {
    return Result<void>::Failure(mscnt.error);
  }
  if (mscnt.value != 0U) { // the table is only rewritten at MSCNT = 0 (page 47)
    return detail::ToResult(config.custom_table ? Error::Refused : Error::Ok);
  }
  static constexpr std::array<uint8_t, 10> kTableRegs{
      0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, reg::kMslutsel, reg::kMslutstart};
  return pushConfig(config, kTableRegs, config.verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::stopRamp(Verify verify) noexcept {
  // A chip that stayed powered while the MCU restarted may still be running the last session's
  // ramp (page 119). Position mode with XTARGET = XACTUAL ends it with the deceleration ramp
  // (page 83); RAMPMODE = Hold would keep the speed. A latched stall stop is left as it is.
  const auto ramp = readReg(reg::kRampStat);
  if (!ramp) {
    return Result<void>::Failure(ramp.error);
  }
  report_.stall_latched = (ramp.value & reg::kRampEventStopStall) != 0U;
  const auto now = readReg(reg::kXactual);
  if (!now) {
    return Result<void>::Failure(now.error);
  }
  // XTARGET first: in velocity mode it is ignored, and position mode then never sees the stale
  // target of the last session (page 83).
  const std::array<detail::Write, 2> w{
      detail::Write{.address = reg::kXtarget, .value = now.value},
      detail::Write{.address = reg::kRampmode, .value = static_cast<uint32_t>(RampMode::Position)}};
  return writeRegs(w, verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::holdIfReleasing(const Switches& next) noexcept {
  // Switching a stop off clears its event and the motor continues towards the old target (pages
  // 43, 44, 90). When a latched event belongs to a stop that is being switched off, the ramp is
  // ended at the present position first.
  const Switches& was = config_.switches;
  uint32_t watch = 0U;
  watch |= (was.stop_on_stall && !next.stop_on_stall) ? reg::kRampEventStopStall : 0U;
  watch |= (was.stop_left && !next.stop_left) ? (1U << 4U) : 0U;
  watch |= (was.stop_right && !next.stop_right) ? (1U << 5U) : 0U;
  if (watch == 0U) {
    return Result<void>::Success();
  }
  const auto ramp = readReg(reg::kRampStat);
  if (!ramp) {
    return Result<void>::Failure(ramp.error);
  }
  if ((ramp.value & watch) == 0U) {
    return Result<void>::Success();
  }
  return stopRamp(Verify::None);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::writeAll(const Config& config) noexcept {
  const bool motion = config.mode == Mode::MotionController;
  if (motion) {
    if (auto r = stopRamp(config.verify); !r) {
      return r;
    }
  }
  hold_at_run_ = config.stealth.enable; // IHOLD = IRUN while StealthChop tunes (page 32)
  static constexpr std::array<uint8_t, 7> kBase{
      reg::kGlobalScaler, reg::kIholdIrun, reg::kTpowerdown, reg::kGconf,
      reg::kTpwmthrs,     reg::kTcoolthrs, reg::kThigh};
  static constexpr std::array<uint8_t, 4> kChopperSide{reg::kPwmconf, reg::kCoolconf, reg::kDcctrl,
                                                       reg::kVdcmin};
  static constexpr std::array<uint8_t, 9> kRamp{reg::kVstart, reg::kA1,    reg::kV1,
                                                reg::kAmax,   reg::kVmax,  reg::kDmax,
                                                reg::kD1,     reg::kVstop, reg::kTzerowait};
  static constexpr std::array<uint8_t, 3> kEncoder{reg::kEncmode, reg::kEncConst,
                                                   reg::kEncDeviation};
  const Verify v = config.verify;
  Result<void> r = Result<void>::Success();
  const auto step = [&r, &config, this, v](std::span<const uint8_t> which) noexcept {
    if (r) {
      r = pushConfig(config, which, v);
    }
  };
  const std::array<uint8_t, 1> drv{reg::kDrvConf};
  const std::array<uint8_t, 1> shortc{reg::kShortConf};
  const std::array<uint8_t, 1> swmode{reg::kSwMode};
  if (config.bridge.declared) {
    step(drv);
  }
  if (config.protection.declared) {
    step(shortc);
  }
  step(kBase);
  if (r) {
    direct_unsure_ = false; // GCONF was written without the direct-mode bit
  }
  step(kChopperSide);
  if (r) {
    r = writeTable(config);
  }
  if (motion) {
    step(kRamp);
    step(swmode);
    if (config.encoder.declared) {
      step(kEncoder);
    }
    ramp_written_ = static_cast<bool>(r);
  }
  const std::array<uint8_t, 1> chop{reg::kChopconf};
  step(chop); // TOFF 0: the driver stays off until `Enable()`
  return r;
}

template <AnyPort Port>
Result<void> TMC5160<Port>::Deinitialize() noexcept {
  if (!started_) {
    return Result<void>::Success(); // nothing was ever sent: nothing to switch off
  }
  const auto r = quiesce();
  initialized_ = false;
  watch_reset_ = false;
  started_ = !r; // only a completed switch-off ends the duty
  return r;
}

// -------------------------------------------------------------------------------------------------
// Enable, faults, current
// -------------------------------------------------------------------------------------------------

template <AnyPort Port>
Result<void> TMC5160<Port>::finishTuning() noexcept {
  if (!(config_.stealth.enable && hold_at_run_)) {
    return Result<void>::Success();
  }
  detail::DelayUs(port_, units::StealthChopAt1Us(
                             config_.clock.hz)); // AT#1 needs standstill at IHOLD = IRUN (page 58)
  hold_at_run_ = false;
  const std::array<uint8_t, 1> which{reg::kIholdIrun};
  return pushConfig(config_, which, Verify::None);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::Enable(Verify verify) noexcept {
  if (auto r = ready(); !r) {
    return r;
  }
  if (fault_) {
    return Result<void>::Failure(Error::DeviceFault);
  }
  if (auto r = checkIntact(); !r) {
    return r;
  }
  if (config_.mode == Mode::MotionController) {
    // A ramp that is still running (a start-up found it so, or the bridge was switched off while
    // the ramp ran) must not be energised: `Busy` until the motor stands still (pages 83, 119).
    if (auto r = ensureStopped(); !r) {
      return r;
    }
  }
  if constexpr (kSpi) {
    // GLOBALSCALER and IHOLD_IRUN set the coil current and cannot be read back:
    // a datagram that was corrupted at start-up would show only now. They are
    // sent again, with their echo checked, before the bridge is switched on (an
    // SPI write has no other proof).
    if (config_.verify == Verify::None) {
      static constexpr std::array<uint8_t, 2> kCurrent{reg::kGlobalScaler, reg::kIholdIrun};
      if (auto r = pushConfig(config_, kCurrent, Verify::Echo); !r) {
        return r;
      }
    }
  }
  if (auto r = setPin(CtrlPin::DrvEnn, true); !r) {
    return r;
  }
  enabled_ = true;
  off_pending_ = false;
  intact_checked_ = true; // the check above is this write's check
  const auto written = writeChopconf(verify);
  intact_checked_ = false;
  if (!written) {
    enabled_ = false;
    const auto off = setPin(CtrlPin::DrvEnn, false);
    off_pending_ = !off; // the chopper write failed: TOFF may be on only if DRV_ENN stays raised
    return off ? written : off;
  }
  return finishTuning();
}

template <AnyPort Port>
Result<void> TMC5160<Port>::Disable(Verify verify) noexcept {
  if (auto r = readyOff(); !r) {
    return r;
  }
  return quiesce(verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::ClearFaults() noexcept {
  if (auto r = ready(); !r) {
    return r;
  }
  const bool was_enabled = enabled_;
  const auto pin = setPin(CtrlPin::DrvEnn, false); // the hardware switch first (page 106)
  enabled_ = false;
  // An unseen reset is reported before anything is written or cleared.
  if (const auto seen = ReadGlobalStatus(); !seen) {
    return Result<void>::Failure(seen.error);
  }
  const auto off = quiesce();
  if (!off || !pin) {
    return pin ? off : pin;
  }
  const auto status = readReg(reg::kDrvStatus);
  if (!status) {
    return Result<void>::Failure(status.error);
  }
  const DriverStatus d = detail::DecodeDriver(status.value);
  if (d.over_temp || d.over_temp_warning) { // the cause is still there (page 79)
    return Result<void>::Failure(Error::Refused);
  }
  if (auto r = writeReg(reg::kGstat, detail::kGstatAll, Verify::None); !r) {
    return r;
  }
  const auto after = readReg(reg::kGstat);
  if (!after) {
    return Result<void>::Failure(after.error);
  }
  if (detail::DecodeGlobal(after.value).drv_err) {
    return Result<void>::Failure(Error::DeviceFault);
  }
  fault_ = false;
  return was_enabled ? Enable(Verify::None) : Result<void>::Success();
}

template <AnyPort Port>
Result<units::CurrentPlan> TMC5160<Port>::SetCurrent(uint32_t run_ma, uint32_t hold_ma,
                                                     Verify verify) noexcept {
  if (auto r = ready(); !r) {
    return Result<units::CurrentPlan>::Failure(r.error);
  }
  const auto plan = units::PlanCurrent(config_, run_ma, hold_ma);
  if (!plan) {
    return plan;
  }
  Config next = config_;
  next.current = Current{run_ma, hold_ma};
  if (units::StealthGuard(next, plan.value) != Error::Ok) {
    return Result<units::CurrentPlan>::Failure(Error::Refused);
  }
  const units::CurrentPlan old = plan_;
  const bool old_hold = hold_at_run_;
  // A write that fails between the two registers leaves one of them new and one old. Write first
  // the register whose new value alone gives the smaller current, so no half-way state exceeds both
  // the old and the new current (page 36: current = GLOBALSCALER x (CS + 1)).
  const uint64_t cs_old = uint64_t{(old.irun > old.ihold) ? old.irun : old.ihold} + 1U;
  const uint64_t cs_new =
      uint64_t{(plan.value.irun > plan.value.ihold) ? plan.value.irun : plan.value.ihold} + 1U;
  const bool scaler_first =
      (uint64_t{plan.value.global_scaler} * cs_old) <= (uint64_t{old.global_scaler} * cs_new);
  const std::array<uint8_t, 2> scaler_first_order{reg::kGlobalScaler, reg::kIholdIrun};
  const std::array<uint8_t, 2> current_first_order{reg::kIholdIrun, reg::kGlobalScaler};
  plan_ = plan.value;
  hold_at_run_ = false;
  if (auto r = pushConfig(next, scaler_first ? scaler_first_order : current_first_order, verify);
      !r) {
    plan_ = old;
    hold_at_run_ = old_hold;
    return Result<units::CurrentPlan>::Failure(r.error);
  }
  return Result<units::CurrentPlan>::Success(plan_);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::SetStandstill(const Standstill& standstill, Verify verify) noexcept {
  if (auto r = ready(); !r) {
    return r;
  }
  Config next = config_;
  next.standstill = standstill;
  if (auto r = guard(next); !r) {
    return r;
  }
  const std::array<uint8_t, 3> which{reg::kIholdIrun, reg::kTpowerdown, reg::kGconf};
  return pushConfig(next, which, verify);
}

// -------------------------------------------------------------------------------------------------
// Motion (section 6.3, page 40; section 12)
// -------------------------------------------------------------------------------------------------

template <AnyPort Port>
Result<void> TMC5160<Port>::SetRamp(const RampProfile& ramp, Verify verify) noexcept {
  if (auto r = readyController(); !r) {
    return r;
  }
  Config next = config_;
  next.ramp = ramp;
  if (auto r = guard(next); !r) {
    return r;
  }
  const std::array<uint8_t, 9> which{reg::kVstart, reg::kA1,    reg::kV1,
                                     reg::kAmax,   reg::kVmax,  reg::kDmax,
                                     reg::kD1,     reg::kVstop, reg::kTzerowait};
  const auto r = pushConfig(next, which, verify);
  ramp_written_ = ramp_written_ || static_cast<bool>(r);
  return r;
}

template <AnyPort Port>
uint32_t TMC5160<Port>::vmaxLimit() const noexcept {
  return (config_.max_velocity_reg == 0U) ? kVmaxLimit : config_.max_velocity_reg;
}

template <AnyPort Port>
Result<void> TMC5160<Port>::MoveTo(int32_t position, Verify verify) noexcept {
  if (auto r = readyMotion(); !r) {
    return r;
  }
  if (!ramp_written_) {
    return Result<void>::Failure(Error::Refused); // all nine ramp registers first (page 40)
  }
  if (config_.ramp.vmax == 0U) {
    // VMAX 0 (`SetMaxVelocity(0)`) would accept the target and never move: a caller waiting for
    // the arrival would wait forever.
    return Result<void>::Failure(Error::Refused);
  }
  // XTARGET, position mode, then the speed: a stop leaves velocity mode with VMAX 0, where XTARGET
  // is ignored; position mode with VMAX still 0 does not move, so the motor never starts in the
  // velocity direction or towards the target of an older move (pages 83, 84).
  std::array<detail::Write, 4> w{};
  std::size_t n = 0U;
  w[n++] = detail::Write{.address = reg::kXtarget, .value = static_cast<uint32_t>(position)};
  w[n++] =
      detail::Write{.address = reg::kRampmode, .value = static_cast<uint32_t>(RampMode::Position)};
  if (shadow_[reg::kVstart] != config_.ramp.vstart) { // a stop left them changed
    w[n++] = detail::Write{.address = reg::kVstart, .value = config_.ramp.vstart};
  }
  if (shadow_[reg::kVmax] != config_.ramp.vmax) {
    w[n++] = detail::Write{.address = reg::kVmax, .value = config_.ramp.vmax};
  }
  return writeRegs(std::span<const detail::Write>{w}.first(n), verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::MoveBy(int32_t delta, Verify verify) noexcept {
  if (auto r = readyMotion(); !r) {
    return r;
  }
  if ((!ramp_written_) || (config_.ramp.vmax == 0U)) {
    return Result<void>::Failure(Error::Refused); // as MoveTo, before the read
  }
  const auto now = ReadPosition();
  if (!now) {
    return Result<void>::Failure(now.error);
  }
  const int64_t target = static_cast<int64_t>(now.value) + static_cast<int64_t>(delta);
  if ((target < INT32_MIN) || (target > INT32_MAX)) {
    return Result<void>::Failure(Error::LimitExceeded); // known only after the read
  }
  return MoveTo(static_cast<int32_t>(target), verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::RunVelocity(int32_t velocity, Verify verify) noexcept {
  if (auto r = readyMotion(); !r) {
    return r;
  }
  if (detail::Abs64(velocity) > static_cast<int64_t>(vmaxLimit())) {
    return Result<void>::Failure(Error::LimitExceeded);
  }
  if (shadow_[reg::kAmax] == 0U) {
    return Result<void>::Failure(Error::InvalidArgument); // velocity mode needs AMAX (page 40)
  }
  const uint32_t mode = static_cast<uint32_t>((velocity < 0) ? RampMode::VelocityNegative
                                                             : RampMode::VelocityPositive);
  // In the same velocity mode the speed goes first (the mode write changes nothing). From another
  // velocity mode the mode goes first, then VSTART and VMAX: the new speed never reaches the move
  // the other way, and a start from rest begins at VSTART. From position mode the mode goes first
  // without a read: a position ramp at VACTUAL 0 may be pausing or turning back (page 83 c), and a
  // read would leave it one more exchange to run back. From hold mode VACTUAL tells the direction;
  // at rest in hold mode the speed goes first too (hold keeps 0, and the old VMAX must not apply).
  const detail::Write mode_write{.address = reg::kRampmode, .value = mode};
  const detail::Write vstart_write{.address = reg::kVstart, .value = config_.ramp.vstart};
  const detail::Write vmax_write{.address = reg::kVmax,
                                 .value = static_cast<uint32_t>(detail::Abs64(velocity))};
  const std::array<detail::Write, 3> speed_first{vmax_write, vstart_write, mode_write};
  const std::array<detail::Write, 3> mode_first{mode_write, vstart_write, vmax_write};
  const std::array<detail::Write, 3> from_position{mode_write, vmax_write, vstart_write};
  const uint32_t was = shadow_[reg::kRampmode];
  if (was == mode) {
    return writeRegs(speed_first, verify);
  }
  if (was == static_cast<uint32_t>(RampMode::Position)) {
    return writeRegs(from_position, verify);
  }
  if (was == static_cast<uint32_t>(RampMode::Hold)) {
    const auto now = readReg(reg::kVactual);
    if (!now) {
      return Result<void>::Failure(now.error);
    }
    const int32_t v = detail::SignExtend(now.value, 24);
    const bool same_way = (v == 0) || ((v < 0) == (velocity < 0));
    return writeRegs(same_way ? speed_first : mode_first, verify);
  }
  return writeRegs(mode_first, verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::SetMaxVelocity(uint32_t vmax, Verify verify) noexcept {
  if (auto r = readyMotion(); !r) {
    return r;
  }
  if (vmax > vmaxLimit()) {
    return Result<void>::Failure(Error::LimitExceeded);
  }
  // VMAX at least VSTART (page 40); VMAX 0 stops a position move only with VSTART 0 (page 84), so
  // VSTART 0 goes first (a later move writes it again).
  // VSTART as the chip has it, and as the next move writes it again (MoveTo restores the profile).
  const uint32_t vstart = shadow_[reg::kVstart];
  const uint32_t floor = (vstart > config_.ramp.vstart) ? vstart : config_.ramp.vstart;
  if ((vmax != 0U) && (vmax < floor)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  const std::array<detail::Write, 2> w{detail::Write{.address = reg::kVstart, .value = 0U},
                                       detail::Write{.address = reg::kVmax, .value = vmax}};
  const bool zero_vstart = (vmax == 0U) && (vstart != 0U);
  const auto r =
      writeRegs(std::span<const detail::Write>{w}.subspan(zero_vstart ? 0U : 1U), verify);
  if (r) {
    config_.ramp.vmax = vmax;
  }
  return r;
}

template <AnyPort Port>
Result<void> TMC5160<Port>::zeroCoils(Verify verify) noexcept {
  const std::array<detail::Write, 2> w{
      detail::Write{.address = reg::kXtarget, .value = 0U},
      detail::Write{.address = reg::kRampmode, .value = static_cast<uint32_t>(RampMode::Hold)}};
  return writeRegs(w, verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::Hold(Verify verify) noexcept {
  if (auto r = readyStop(); !r) {
    return r;
  }
  const bool watched = beginSequence();
  if (direct_mode_ || direct_unsure_) { // XTARGET holds the coil currents (page 110)
    return endSequence(watched, zeroCoils(busVerify(verify)));
  }
  const std::array<detail::Write, 1> w{
      detail::Write{.address = reg::kRampmode, .value = static_cast<uint32_t>(RampMode::Hold)}};
  return endSequence(watched, writeRegs(w, busVerify(verify)));
}

template <AnyPort Port>
Result<void> TMC5160<Port>::Stop(StopKind kind, Verify verify) noexcept {
  if (!detail::IsEnumerator(kind)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (kind == StopKind::Emergency) {
    if (auto r = readyOff(); !r) {
      return r;
    }
    return quiesce(verify);
  }
  if (auto r = readyStop(); !r) {
    return r;
  }
  const Verify v = busVerify(verify);
  const bool watched = beginSequence();
  const bool coils = direct_mode_ || direct_unsure_;
  if (coils || (kind != StopKind::HoldTarget)) {
    // Velocity mode with VMAX 0 stops the motor by the AMAX ramp; in doubt about direct mode the
    // coil currents go to zero first (a velocity of 0 and a current of 0 are safe in either).
    return endSequence(watched, rampStop(kind == StopKind::Immediate, coils, v));
  }
  const auto now = readReg(reg::kXactual);
  if (!now) {
    return endSequence(watched, Result<void>::Failure(now.error));
  }
  // XTARGET before position mode: the chip never heads for the stale target of an older move.
  const std::array<detail::Write, 2> w{
      detail::Write{.address = reg::kXtarget, .value = now.value},
      detail::Write{.address = reg::kRampmode, .value = static_cast<uint32_t>(RampMode::Position)}};
  return endSequence(watched, writeRegs(w, v));
}

template <AnyPort Port>
Result<void> TMC5160<Port>::SetHomePosition(int32_t position) noexcept {
  if (auto r = readyController(); !r) {
    return r;
  }
  if (auto r = ensureStopped(); !r) {
    return r;
  }
  const uint32_t word = static_cast<uint32_t>(position);
  const std::array<detail::Write, 3> w{
      detail::Write{.address = reg::kRampmode, .value = static_cast<uint32_t>(RampMode::Hold)},
      detail::Write{.address = reg::kXactual, .value = word},
      detail::Write{.address = reg::kXtarget, .value = word}};
  return writeRegs(w, Verify::None);
}

template <AnyPort Port>
Result<int32_t> TMC5160<Port>::ReadPosition() noexcept {
  const auto r = readControlled(reg::kXactual);
  return r ? Result<int32_t>::Success(static_cast<int32_t>(r.value))
           : Result<int32_t>::Failure(r.error);
}

template <AnyPort Port>
Result<int32_t> TMC5160<Port>::ReadVelocity() noexcept {
  const auto r = readControlled(reg::kVactual);
  return r ? Result<int32_t>::Success(detail::SignExtend(r.value, 24))
           : Result<int32_t>::Failure(r.error);
}

template <AnyPort Port>
Result<int32_t> TMC5160<Port>::ReadTarget() noexcept {
  const auto r = readControlled(reg::kXtarget);
  return r ? Result<int32_t>::Success(static_cast<int32_t>(r.value))
           : Result<int32_t>::Failure(r.error);
}

template <AnyPort Port>
Result<bool> TMC5160<Port>::PositionReached() noexcept {
  const auto r = readControlled(reg::kRampStat);
  return r ? Result<bool>::Success(detail::DecodeRamp(r.value).position_reached)
           : Result<bool>::Failure(r.error);
}

template <AnyPort Port>
Result<int32_t> TMC5160<Port>::ReadLatchedPosition() noexcept {
  const auto r = readControlled(reg::kXlatch);
  return r ? Result<int32_t>::Success(static_cast<int32_t>(r.value))
           : Result<int32_t>::Failure(r.error);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::ReleaseStall() noexcept {
  if (auto r = Hold(Verify::None); !r) {
    return r;
  }
  if (auto r = ensureStopped(); !r) {
    return r;
  }
  return writeReg(reg::kRampStat, reg::kRampEventStopStall,
                  Verify::None); // the motor never restarts by itself (page 90)
}

// -------------------------------------------------------------------------------------------------
// Chopper, StealthChop, CoolStep, StallGuard, DcStep
// -------------------------------------------------------------------------------------------------

template <AnyPort Port>
Result<void> TMC5160<Port>::setField(Config next, std::span<const uint8_t> which,
                                     Verify verify) noexcept {
  if (auto r = ready(); !r) {
    return r;
  }
  if (auto r = guard(next); !r) {
    return r;
  }
  next.ack = config_.ack; // an acknowledgement given to one call does not stay
  return pushConfig(next, which, verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::SetChopper(const Chopper& chopper, Verify verify) noexcept {
  Config next = config_;
  next.chopper = chopper;
  const std::array<uint8_t, 1> which{reg::kChopconf};
  return setField(next, which, verify); // `pushConfig()` checks the chip first while enabled
}

template <AnyPort Port>
Result<void> TMC5160<Port>::SetStealthChop(const StealthChop& stealth, TuningPolicy policy,
                                           Verify verify) noexcept {
  if (auto r = ready(); !r) {
    return r;
  }
  Config next = config_;
  next.stealth = stealth;
  const bool newly_on = stealth.enable && !config_.stealth.enable;
  if (auto r = guard(next); !r) {
    return r;
  }
  if (units::StealthGuard(next, plan_) != Error::Ok) {
    return Result<void>::Failure(Error::Refused);
  }
  if (newly_on && (config_.mode == Mode::MotionController)) {
    if (auto r = ensureStopped(); !r) { // en_pwm_mode only at standstill (page 32)
      return r;
    }
  }
  const bool old_hold = hold_at_run_;
  hold_at_run_ = stealth.enable && (newly_on || hold_at_run_);
  const std::array<uint8_t, 3> which{reg::kIholdIrun, reg::kPwmconf, reg::kGconf};
  if (auto r = pushConfig(next, which, verify); !r) {
    hold_at_run_ = old_hold;
    return r;
  }
  if (newly_on && enabled_ && (policy == TuningPolicy::HoldAtRunForAt1)) {
    return finishTuning();
  }
  return Result<void>::Success();
}

template <AnyPort Port>
Result<StealthChopState> TMC5160<Port>::ReadStealthChopState() noexcept {
  if (auto r = ready(); !r) {
    return Result<StealthChopState>::Failure(r.error);
  }
  const std::array<uint8_t, 2> list{reg::kPwmScale, reg::kPwmAuto};
  std::array<uint32_t, 2> out{};
  const Error e = readMany(list, out);
  return (e == Error::Ok)
             ? Result<StealthChopState>::Success(detail::DecodeStealthChop(out[0], out[1]))
             : Result<StealthChopState>::Failure(e);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::SetThresholds(const Thresholds& thresholds, Verify verify) noexcept {
  Config next = config_;
  next.thresholds = thresholds;
  const std::array<uint8_t, 4> which{reg::kTpwmthrs, reg::kTcoolthrs, reg::kThigh, reg::kGconf};
  return setField(next, which, verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::SetCoolStep(const CoolStep& cool, Verify verify) noexcept {
  Config next = config_;
  next.cool = cool;
  const std::array<uint8_t, 1> which{reg::kCoolconf};
  return setField(next, which, verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::SetStallGuard(const StallGuard& stall, Verify verify) noexcept {
  Config next = config_;
  next.stall = stall;
  const std::array<uint8_t, 1> which{reg::kCoolconf};
  return setField(next, which, verify);
}

template <AnyPort Port>
Result<uint32_t> TMC5160<Port>::ReadStallGuard() noexcept {
  const auto r = readChecked(reg::kDrvStatus);
  if (const Error e = r ? lineCheck(r.value) : Error::Ok; e != Error::Ok) {
    return Result<uint32_t>::Failure(e);
  }
  return r ? Result<uint32_t>::Success(detail::DecodeDriver(r.value).sg_result) : r;
}

template <AnyPort Port>
Result<void> TMC5160<Port>::SetDcStep(const DcStep& dcstep, Verify verify) noexcept {
  Config next = config_;
  next.dcstep = dcstep;
  const std::array<uint8_t, 2> which{reg::kVdcmin, reg::kDcctrl};
  return setField(next, which, verify);
}

template <AnyPort Port>
Result<uint32_t> TMC5160<Port>::ReadLostSteps() noexcept {
  if (config_.mode != Mode::StepDir) {
    return Result<uint32_t>::Failure(initialized_ ? Error::Unsupported : Error::NotInitialized);
  }
  return readChecked(reg::kLostSteps);
}

// -------------------------------------------------------------------------------------------------
// Power stage (sections 3.3, 11.2; pages 18, 35, 78): written from declared values, driver off
// -------------------------------------------------------------------------------------------------

template <AnyPort Port>
Result<void> TMC5160<Port>::SetBridge(const Bridge& bridge, AckSet ack, Verify verify) noexcept {
  if (auto r = ready(); !r) {
    return r;
  }
  if (enabled_ || off_pending_) {
    // BBMTIME and BBMCLKS must not change with a running motor (page 36)
    return Result<void>::Failure(Error::Busy);
  }
  Config next = config_;
  next.bridge = bridge;
  next.bridge.declared = true;
  next.ack.bits |= ack.bits;
  const std::array<uint8_t, 1> which{reg::kDrvConf};
  return setField(next, which, verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::SetProtection(const Protection& protection, AckSet ack,
                                          Verify verify) noexcept {
  if (auto r = ready(); !r) {
    return r;
  }
  if (enabled_ || off_pending_) {
    return Result<void>::Failure(Error::Busy);
  }
  Config next = config_;
  next.protection = protection;
  next.protection.declared = true;
  next.ack.bits |= ack.bits;
  const std::array<uint8_t, 2> which{reg::kShortConf, reg::kChopconf};
  return setField(next, which, verify);
}

// -------------------------------------------------------------------------------------------------
// Flags, DIAG, switches, compare, direct mode
// -------------------------------------------------------------------------------------------------

template <AnyPort Port>
Result<void> TMC5160<Port>::SetFlags(const Flags& flags, Verify verify) noexcept {
  Config next = config_;
  next.flags = flags;
  const std::array<uint8_t, 1> which{reg::kGconf};
  return setField(next, which, verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::SetDiag(const Diag& diag, Verify verify) noexcept {
  Config next = config_;
  next.diag = diag;
  const std::array<uint8_t, 1> which{reg::kGconf};
  return setField(next, which, verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::SetSwitches(const Switches& switches, Verify verify) noexcept {
  if (auto r = readyController(); !r) {
    return r;
  }
  Config next = config_;
  next.switches = switches;
  if (auto r = guard(next); !r) {
    return r;
  }
  if (auto r = holdIfReleasing(switches); !r) {
    return r;
  }
  const std::array<uint8_t, 1> which{reg::kSwMode};
  return setField(next, which, verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::SetStopOnStall(bool enable, Verify verify) noexcept {
  Switches s = config_.switches;
  s.stop_on_stall = enable;
  return SetSwitches(s, verify);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::SetPositionCompare(int32_t position) noexcept {
  if (auto r = readyController(); !r) {
    return r;
  }
  return writeReg(reg::kXCompare, static_cast<uint32_t>(position), Verify::None);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::EnterDirectMode() noexcept {
  if (auto r = readyController(); !r) {
    return r;
  }
  if (!config_.ack.Has(Ack::DirectMode)) {
    return Result<void>::Failure(
        Error::Refused); // coil currents from XTARGET: no ramp, no limit (page 110)
  }
  if (auto r = ensureStopped(); !r) {
    return r;
  }
  if (auto r = Hold(Verify::None); !r) {
    return r;
  }
  if (auto r = writeReg(reg::kXtarget, 0U, Verify::None); !r) { // no stale target as coil current
    return r;
  }
  direct_mode_ = true; // a write that failed may still have taken effect: see below
  const std::array<uint8_t, 1> which{reg::kGconf};
  const auto r = pushConfig(config_, which, Verify::None);
  direct_mode_ = static_cast<bool>(r);
  direct_unsure_ = !r;
  return r;
}

template <AnyPort Port>
Result<void> TMC5160<Port>::SetDirectCurrents(int32_t coil_a, int32_t coil_b) noexcept {
  if (auto r = ready(); !r) {
    return r;
  }
  if (!direct_mode_) {
    return Result<void>::Failure(Error::Refused);
  }
  const bool voltage_mode = config_.stealth.enable;
  if (voltage_mode && !config_.ack.Has(Ack::VoltageModeNoCurrentLimit)) {
    return Result<void>::Failure(
        Error::Refused); // StealthChop has no current limit in direct mode (page 110)
  }
  const int32_t limit = voltage_mode ? detail::kDirectMaxVoltage : detail::kDirectMax;
  if ((detail::Abs64(coil_a) > limit) || (detail::Abs64(coil_b) > limit)) {
    return Result<void>::Failure(Error::LimitExceeded);
  }
  const uint32_t word =
      ((static_cast<uint32_t>(coil_b) & 0x1FFU) << 16U) | (static_cast<uint32_t>(coil_a) & 0x1FFU);
  return writeReg(reg::kXtarget, word, Verify::None);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::ExitDirectMode() noexcept {
  if (auto r = ready(); !r) {
    return r;
  }
  if (auto r = writeReg(reg::kRampmode, static_cast<uint32_t>(RampMode::Hold), Verify::None); !r) {
    return r;
  }
  direct_mode_ = false;
  direct_unsure_ = true;
  const std::array<uint8_t, 1> which{reg::kGconf};
  if (auto r = pushConfig(config_, which, Verify::None); !r) {
    return r;
  }
  direct_unsure_ = false;
  const auto now = readReg(reg::kXactual);
  if (!now) {
    return Result<void>::Failure(now.error);
  }
  return writeReg(reg::kXtarget, now.value, Verify::None); // the motor must not jump (page 110)
}

// -------------------------------------------------------------------------------------------------
// Encoder and microstep table
// -------------------------------------------------------------------------------------------------

template <AnyPort Port>
Result<void> TMC5160<Port>::SetEncoder(const Encoder& encoder, Verify verify) noexcept {
  if (auto r = readyController(); !r) {
    return r;
  }
  Config next = config_;
  next.encoder = encoder;
  next.encoder.declared = true;
  const std::array<uint8_t, 3> which{reg::kEncmode, reg::kEncConst, reg::kEncDeviation};
  return setField(next, which, verify);
}

template <AnyPort Port>
Result<int32_t> TMC5160<Port>::ReadEncoderPosition() noexcept {
  const auto r = readControlled(reg::kXEnc);
  return r ? Result<int32_t>::Success(static_cast<int32_t>(r.value))
           : Result<int32_t>::Failure(r.error);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::SetEncoderPosition(int32_t position) noexcept {
  if (auto r = readyController(); !r) {
    return r;
  }
  return writeReg(reg::kXEnc, static_cast<uint32_t>(position), Verify::None);
}

template <AnyPort Port>
Result<int32_t> TMC5160<Port>::ReadEncoderLatch() noexcept {
  const auto r = readControlled(reg::kEncLatch);
  return r ? Result<int32_t>::Success(static_cast<int32_t>(r.value))
           : Result<int32_t>::Failure(r.error);
}

template <AnyPort Port>
Result<EncoderStatus> TMC5160<Port>::ReadEncoderStatus() noexcept {
  const auto r = readControlled(reg::kEncStatus);
  return r ? Result<EncoderStatus>::Success(
                 EncoderStatus{detail::Bit(r.value, 0), detail::Bit(r.value, 1)})
           : Result<EncoderStatus>::Failure(r.error);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::ClearEncoderEvents(bool n_event, bool deviation_warn) noexcept {
  if (auto r = readyController(); !r) {
    return r;
  }
  const uint32_t mask = detail::Put(n_event, 0) | detail::Put(deviation_warn, 1);
  return writeReg(reg::kEncStatus, mask, Verify::None);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::SetMicrostepTable(const MicrostepTable& table) noexcept {
  if (auto r = ready(); !r) {
    return r;
  }
  if (CheckTable(table) != Error::Ok) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  Config next = config_;
  next.table = table;
  next.custom_table = true;
  if (auto r = guard(next); !r) {
    return r;
  }
  const auto mscnt = readReg(reg::kMscnt);
  if (!mscnt) {
    return Result<void>::Failure(mscnt.error);
  }
  if (mscnt.value != 0U) {
    return Result<void>::Failure(Error::Refused); // rewrite only at MSCNT = 0 (page 47)
  }
  const auto r = writeTable(next);
  return r;
}

template <AnyPort Port>
Result<uint32_t> TMC5160<Port>::ReadMicrostepCounter() noexcept {
  return readChecked(reg::kMscnt);
}

template <AnyPort Port>
Result<MicrostepCurrents> TMC5160<Port>::ReadMicrostepCurrents() noexcept {
  const auto r = readChecked(reg::kMscuract);
  return r ? Result<MicrostepCurrents>::Success(detail::DecodeMscuract(r.value))
           : Result<MicrostepCurrents>::Failure(r.error);
}

// -------------------------------------------------------------------------------------------------
// Status and diagnostics
// -------------------------------------------------------------------------------------------------

template <AnyPort Port>
Result<GlobalStatus> TMC5160<Port>::ReadGlobalStatus() noexcept {
  const auto r = readChecked(reg::kGstat);
  if (!r) {
    return Result<GlobalStatus>::Failure(r.error);
  }
  const GlobalStatus g = detail::DecodeGlobal(r.value);
  fault_ = fault_ || g.drv_err;
  if (g.reset && watch_reset_) { // a reset seen through GSTAT (UART has no status byte)
    noteReset();
    return Result<GlobalStatus>::Failure(Error::UnexpectedReset);
  }
  return Result<GlobalStatus>::Success(g);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::ClearGlobalStatus(uint32_t mask) noexcept {
  if (auto r = ready(); !r) {
    return r;
  }
  if ((mask == 0U) || ((mask & ~detail::kGstatAll) != 0U)) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  // Clearing the reset flag unseen would hide a lost register file.
  if ((mask & reg::kGstatReset) != 0U) {
    if (const auto seen = ReadGlobalStatus(); !seen) {
      return Result<void>::Failure(seen.error);
    }
  }
  return writeReg(reg::kGstat, mask, Verify::None);
}

template <AnyPort Port>
Result<DriverStatus> TMC5160<Port>::ReadDriverStatus() noexcept {
  const auto r = readChecked(reg::kDrvStatus);
  if (const Error e = r ? lineCheck(r.value) : Error::Ok; e != Error::Ok) {
    return Result<DriverStatus>::Failure(e);
  }
  return r ? Result<DriverStatus>::Success(detail::DecodeDriver(r.value))
           : Result<DriverStatus>::Failure(r.error);
}

template <AnyPort Port>
Result<RampStatus> TMC5160<Port>::ReadRampStatus() noexcept {
  const auto r = readControlled(reg::kRampStat);
  return r ? Result<RampStatus>::Success(detail::DecodeRamp(r.value))
           : Result<RampStatus>::Failure(r.error);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::ClearRampEvents(uint32_t mask) noexcept {
  if (auto r = readyController(); !r) {
    return r;
  }
  if ((mask == 0U) || ((mask & ~detail::kRampClearMask) != 0U)) {
    return Result<void>::Failure(Error::InvalidArgument); // read-only bits are never written (A24)
  }
  if ((mask & reg::kRampEventStopStall) != 0U) {
    return Result<void>::Failure(Error::Refused); // only `ReleaseStall()` releases a stall stop
  }
  return writeReg(reg::kRampStat, mask, Verify::None);
}

template <AnyPort Port>
Result<InputPins> TMC5160<Port>::ReadInputs() noexcept {
  const auto r = readChecked(reg::kIoin);
  if (!r) {
    return Result<InputPins>::Failure(r.error);
  }
  const InputPins pins = detail::DecodeInputs(r.value);
  if (pins.version != reg::kVersion) { // a stuck line reads all zeros or all ones
    return Result<InputPins>::Failure(Error::DeviceIdMismatch);
  }
  return Result<InputPins>::Success(pins);
}

template <AnyPort Port>
Result<Offsets> TMC5160<Port>::ReadOffsets() noexcept {
  const auto r = readChecked(reg::kOffsetRead);
  return r ? Result<Offsets>::Success(
                 Offsets{detail::SignExtend(r.value >> 8U, 8), detail::SignExtend(r.value, 8)})
           : Result<Offsets>::Failure(r.error);
}

template <AnyPort Port>
Result<uint32_t> TMC5160<Port>::ReadStepTime() noexcept {
  return readChecked(reg::kTstep);
}

template <AnyPort Port>
Result<uint32_t> TMC5160<Port>::ReadInterfaceCounter() noexcept {
  if (!initialized_) {
    return Result<uint32_t>::Failure(Error::NotInitialized);
  }
  if constexpr (kSpi) {
    return Result<uint32_t>::Failure(
        Error::Unsupported); // IFCNT is disabled in SPI operation (page 33)
  } else {
    return readReg(reg::kIfcnt);
  }
}

template <AnyPort Port>
Result<OtpImage> TMC5160<Port>::ReadOtp() noexcept {
  const auto r = readChecked(reg::kOtpRead);
  return r ? Result<OtpImage>::Success(detail::DecodeOtp(r.value))
           : Result<OtpImage>::Failure(r.error);
}

template <AnyPort Port>
Result<uint32_t> TMC5160<Port>::ReadClockTrim() noexcept {
  const auto r = readChecked(reg::kFactoryConf);
  return r ? Result<uint32_t>::Success(r.value & 0x1FU) : r;
}

template <AnyPort Port>
Result<uint32_t> TMC5160<Port>::Identify() noexcept {
  const auto r = readChecked(reg::kIoin);
  return r ? Result<uint32_t>::Success(r.value >> 24U) : r;
}

template <AnyPort Port>
Result<Snapshot> TMC5160<Port>::ReadSnapshot() noexcept {
  if (auto r = readyController(); !r) {
    return Result<Snapshot>::Failure(r.error);
  }
  static constexpr std::array<uint8_t, 7> kList{reg::kGstat,   reg::kDrvStatus, reg::kRampStat,
                                                reg::kXactual, reg::kVactual,   reg::kTstep,
                                                reg::kMscnt};
  std::array<uint32_t, 7> v{};
  const Error e = readMany(kList, v);
  if (e != Error::Ok) {
    return Result<Snapshot>::Failure(e);
  }
  if (const Error line = lineCheck(v[0] | v[1] | v[2] | v[3] | v[4] | v[5] | v[6]);
      line != Error::Ok) {
    return Result<Snapshot>::Failure(line);
  }
  Snapshot s{};
  s.global = detail::DecodeGlobal(v[0]);
  s.driver = detail::DecodeDriver(v[1]);
  s.ramp = detail::DecodeRamp(v[2]);
  s.position = static_cast<int32_t>(v[3]);
  s.velocity = detail::SignExtend(v[4], 24);
  s.step_time = v[5];
  s.mscnt = v[6];
  return Result<Snapshot>::Success(s);
}

template <AnyPort Port>
Result<Events> TMC5160<Port>::Service() noexcept {
  if (auto r = ready(); !r) {
    return Result<Events>::Failure(r.error);
  }
  const bool motion = config_.mode == Mode::MotionController;
  std::array<uint8_t, 4> list{reg::kGstat, reg::kDrvStatus, reg::kRampStat, reg::kEncStatus};
  std::array<uint32_t, 4> v{};
  const std::size_t count = motion ? 4U : 2U;
  if (const Error e = readMany(std::span<const uint8_t>{list}.first(count),
                               std::span<uint32_t>{v}.first(count));
      e != Error::Ok) {
    return Result<Events>::Failure(e);
  }
  // A MISO line stuck low reads all zeros, status byte included (page 24). `v` starts zeroed and
  // only the first `count` entries are read, so the OR of all four is the OR of the words read.
  if (const Error line = lineCheck(v[0] | v[1] | v[2] | v[3]); line != Error::Ok) {
    return Result<Events>::Failure(line);
  }
  Events ev{};
  ev.global = detail::DecodeGlobal(v[0]);
  ev.driver = detail::DecodeDriver(v[1]);
  ev.ramp = detail::DecodeRamp(v[2]);
  ev.encoder = EncoderStatus{detail::Bit(v[3], 0), detail::Bit(v[3], 1)};
  fault_ = fault_ || ev.global.drv_err;
  if (ev.global.reset) {
    noteReset();
    return Result<Events>::Failure(Error::UnexpectedReset);
  }
  const Result<void> cleared = clearEvents(ev);
  return cleared ? Result<Events>::Success(ev) : Result<Events>::Failure(cleared.error);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::clearEvents(const Events& ev) noexcept {
  if (ev.global.uv_cp) {
    if (auto r = writeReg(reg::kGstat, reg::kGstatUvCp, Verify::None); !r) {
      return r;
    }
  }
  // A stall stop stays until `ReleaseStall()`: the ramp would restart towards the old target.
  const uint32_t ramp = ev.ramp.raw & detail::kRampClearMask & ~reg::kRampEventStopStall;
  if ((config_.mode == Mode::MotionController) && (ramp != 0U)) {
    if (auto r = writeReg(reg::kRampStat, ramp, Verify::None); !r) {
      return r;
    }
  }
  const uint32_t enc =
      detail::Put(ev.encoder.n_event, 0) | detail::Put(ev.encoder.deviation_warn, 1);
  if ((config_.mode == Mode::MotionController) && (enc != 0U)) {
    return writeReg(reg::kEncStatus, enc, Verify::None);
  }
  return Result<void>::Success();
}

template <AnyPort Port>
Result<uint32_t> TMC5160<Port>::ReadRegister(uint8_t address) noexcept {
  return readChecked(address);
}

template <AnyPort Port>
Result<bool> TMC5160<Port>::pinActive(InputPin pin, bool push_pull) noexcept {
  if constexpr (detail::HasGpioGet<Port>) {
    bool level = false;
    if (!port_.GpioGet(pin, level)) {
      return Result<bool>::Failure(Error::Bus);
    }
    return Result<bool>::Success(push_pull ? level : !level); // open drain pulls low when active
  } else {
    static_cast<void>(pin);
    static_cast<void>(push_pull);
    return Result<bool>::Failure(Error::Unsupported);
  }
}

template <AnyPort Port>
Result<bool> TMC5160<Port>::Diag0Active() noexcept {
  return pinActive(InputPin::Diag0, config_.diag.diag0_push_pull);
}

template <AnyPort Port>
Result<bool> TMC5160<Port>::Diag1Active() noexcept {
  return pinActive(InputPin::Diag1, config_.diag.diag1_push_pull);
}

template <AnyPort Port>
Result<void> TMC5160<Port>::ProgramNode(uint8_t from_node, uint8_t new_node,
                                        bool nao_level) noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if constexpr (kSpi) {
    static_cast<void>(from_node);
    static_cast<void>(new_node);
    static_cast<void>(nao_level);
    return Result<void>::Failure(Error::Unsupported); // NODECONF is not written over SPI (page 34)
  } else {
    // Node 254 needs `Config::uart.multi_node`: a stored configuration must stay valid.
    if (new_node > (config_.uart.multi_node ? 254U : 253U)) {
      return Result<void>::Failure(Error::InvalidArgument);
    }
    const uint8_t own = config_.uart.node;
    node_ = from_node;
    Error e = uartWrite(reg::kNodeconf, detail::NodeconfWord(new_node, config_.uart.send_delay));
    if (e == Error::Ok) {
      node_ = new_node;
      config_.uart.node = (from_node == own) ? new_node : own;
      e = uartWrite(reg::kOutput, nao_level ? 1U : 0U);
    }
    node_ = config_.uart.node;
    return detail::ToResult(e);
  }
}

} // namespace tmc5160

#endif // TMC5160_IPP
