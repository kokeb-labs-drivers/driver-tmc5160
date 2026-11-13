/**
 * @file fuzz_tmc5160.cpp
 * @brief Fuzz target: the driver over a bus whose behaviour the fuzzer controls.
 *
 * What the driver reads comes from the chip over the bus, so read data is the untrusted input. The
 * fuzzer's bytes choose the bus (SPI or UART), a declared motor and board, then a sequence of
 * driver calls; on every transfer they also choose what the bus does: pass it to the chip model,
 * fail it, flip bits of the data read, replace it, or corrupt a UART request. They can also change
 * the chip: a wrong VERSION, a flipped strap, a short circuit, an overtemperature, a stall, a
 * reference switch, a brown-out.
 *
 * After every call the target checks the driver's promises (docs/contract.md), and aborts, naming
 * the line, when one breaks:
 *
 * - F1 a failed call returns an `Error` the enum defines, and `T{}`;
 * - F2 a call that returns `NotInitialized` made no transfer (a switch-off, a stop and
 * `Deinitialize()` need only a started driver: after a reset report they still reach the chip);
 * - F3 `InvalidArgument` means nothing was sent;
 * - F4 `UnexpectedReset` leaves the driver uninitialised;
 * - F5 the chip's OTP_PROG and FACTORY_CONF are never written, the GCONF test bit is never set;
 * - F6 no write sets a bit the register does not define;
 * - F7 no write-only register is read;
 * - F8 the coil current the chip computes never exceeds the declared rated current, on any
 * interleaving of failed transfers (not after a brown-out followed by a noisy SPI read: SPI has no
 * CRC, so a corrupted GSTAT read can hide the reset flag that `Enable()` looks for);
 * - F9 an enabled driver is a started driver, and a latched fault belongs to a started driver;
 * - F10 a position, velocity or status read of a started driver succeeds only with the register's
 * range;
 * - F11 every log line is a non-empty plain-text message;
 * - F12 the SPI frames are 5 bytes times the chain length, the UART frames 4 or 8 bytes;
 * - F13 a refused call (`Refused`, `LimitExceeded`, `Busy`) never leaves the driver enabled with a
 * changed current plan above the ceiling;
 * - F14 after a failed `EnsureInitialized()` the driver is off and not started, and the chip's TOFF
 * is 0 once the first write went out, unless the error is a bus error (the quiesce write itself may
 * not have been deliverable);
 * - F15 `Deinitialize()` always leaves the driver uninitialised;
 * - F16 `ProgramNode()` on SPI is `Unsupported`;
 * - F17 a latched stall stop that holds the motor still is released only by `ReleaseStall()` or by
 * an explicit motion command, never by a setter or a start-up (no frame was tampered with);
 * - F18 after a successful `EnsureInitialized()` in the motion-controller mode the ramp generator
 * is in position mode, so a ramp left running by the last session is ended;
 * - F19 a `Disable()` that returns `Ok` left DRV_ENN inactive and TOFF 0, also after a reset
 * report;
 * - F20 a stop or hold that returns `Ok` on a chip in direct mode left XTARGET (the coil currents)
 * at 0;
 * - F21 a stop that returns `Ok` on a motion-controller chip left the ramp generator commanded to
 * stop (velocity mode with VMAX 0, or position mode with XTARGET = XACTUAL), never in Hold.
 *
 * Built two ways (tests/fuzz/CMakeLists.txt): with libFuzzer (Clang, `-DTMC5160_BUILD_FUZZERS=ON`),
 * and as a plain executable that replays the corpus in tests/fuzz/corpus/ as a CTest on any
 * compiler.
 */
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "mock_tmc5160_bus.hpp"
#include "tmc5160.hpp"

namespace {

using tmc5160::Error;
using tmc5160_test::Chip;
using tmc5160_test::Wire;

/// The fuzzer's bytes, read front to back; zeros once they run out.
class Input {
public:
  Input(const uint8_t* data, std::size_t size) noexcept : data_(data), size_(size) {}

  [[nodiscard]] bool Empty() const noexcept {
    return pos_ >= size_;
  }
  uint8_t Byte() noexcept {
    return pos_ < size_ ? data_[pos_++] : uint8_t{0};
  }
  uint32_t Word() noexcept {
    const uint32_t high = Byte();
    return (high << 8U) | Byte();
  }

private:
  const uint8_t* data_;
  std::size_t size_;
  std::size_t pos_ = 0;
};

/// A broken promise: name the line and abort, so libFuzzer saves the input as a crash.
void Check(bool promise, int line = __builtin_LINE()) noexcept {
  if (!promise) {
    static_cast<void>(std::fprintf(stderr, "fuzz_tmc5160: promise broken at line %d\n", line));
    std::abort();
  }
}

/// What the bus does with one transfer, chosen by the fuzzer; any other value passes it.
constexpr uint8_t kFail = 0xFF;    ///< The transfer fails; it never reached the chip.
constexpr uint8_t kFlip = 0xFE;    ///< Data read: bits flipped (noise).
constexpr uint8_t kReplace = 0xFD; ///< The data read is replaced (another device answered).
constexpr uint8_t kBadTx = 0xFC;   ///< A UART request arrives with a bad CRC.

/// Shared by both ports: the chip model behind a bus the fuzzer controls.
struct Shared {
  explicit Shared(Input& input) noexcept : in(input) {}
  Input& in;
  Wire wire{};
  int sent = 0;
  bool bus_up = true;
  bool armed = false;  ///< A brown-out happened: the chip lost its registers.
  bool exempt = false; ///< A noisy SPI read followed a brown-out: the reset flag may have been
                       ///< hidden (no CRC on SPI).
  bool spi = false;
  bool tampered = false; ///< Some byte of some frame was changed on the way.

  uint8_t Next() noexcept {
    ++sent;
    return in.Byte();
  }

  void Tamper(uint8_t action, uint8_t* data, std::size_t len) noexcept {
    tampered = tampered || ((action == kFlip) || (action == kReplace));
    exempt = exempt || (armed && spi && ((action == kFlip) || (action == kReplace)));
    for (std::size_t i = 0; i < len; ++i) {
      if (action == kFlip) {
        data[i] = static_cast<uint8_t>(data[i] ^ in.Byte());
      } else if (action == kReplace) {
        data[i] = in.Byte();
      } else {
        // Pass the byte through unchanged.
      }
    }
  }
};

class FuzzSpiPort : public tmc5160::SpiInterface<FuzzSpiPort> {
public:
  explicit FuzzSpiPort(Input& in) noexcept : s(in) {
    s.spi = true;
  }
  Shared s;

  [[nodiscard]] bool EnsureInitialized() const noexcept {
    return s.bus_up;
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    Check((tx != nullptr) && (rx != nullptr) && (len >= 5U) && ((len % 5U) == 0U) &&
          (len <= (static_cast<std::size_t>(5U) * tmc5160::kMaxChain))); // F12
    const uint8_t action = s.Next();
    if (action == kFail) {
      return false;
    }
    static_cast<void>(s.wire.Spi(tx, rx, len));
    s.Tamper(action, rx, len);
    return true;
  }
  void DelayUs(uint32_t us) noexcept {
    s.wire.Delay(us);
  }
  bool GpioSet(tmc5160::CtrlPin pin, tmc5160::GpioSignal signal) noexcept {
    return (s.Next() != kFail) && s.wire.Pin(pin, signal);
  }
  bool GpioGet(tmc5160::InputPin pin, bool& level) noexcept {
    return (s.Next() != kFail) && s.wire.Level(pin, level);
  }
  void Log(tmc5160::LogLevel level, const char* message) noexcept {
    static_cast<void>(level);
    Check((message != nullptr) && !std::string_view{message}.empty()); // F11
  }
};

class FuzzUartPort : public tmc5160::UartInterface<FuzzUartPort> {
public:
  explicit FuzzUartPort(Input& in) noexcept : s(in) {}
  Shared s;

  [[nodiscard]] bool EnsureInitialized() const noexcept {
    return s.bus_up;
  }
  tmc5160::BusStatus Exchange(const uint8_t* tx, std::size_t tx_len, uint8_t* rx,
                              std::size_t rx_len, uint32_t timeout_us) noexcept {
    Check((tx != nullptr) && ((tx_len == 4U) || (tx_len == 8U)) &&
          ((rx_len == 0U) || (rx_len == 8U))); // F12
    const uint8_t action = s.Next();
    if (action == kFail) {
      return tmc5160::BusStatus::Fault;
    }
    s.tampered = s.tampered || (action == kBadTx);
    s.wire.faults.corrupt_tx_at = (action == kBadTx) ? s.wire.transfers : -1;
    const auto status = s.wire.Uart(tx, tx_len, rx, rx_len, timeout_us);
    if ((status == tmc5160::BusStatus::Ok) && (rx_len != 0U)) {
      s.Tamper(action, rx, rx_len);
    }
    return status;
  }
  void DelayUs(uint32_t us) noexcept {
    s.wire.Delay(us);
  }
  bool GpioSet(tmc5160::CtrlPin pin, tmc5160::GpioSignal signal) noexcept {
    return (s.Next() != kFail) && s.wire.Pin(pin, signal);
  }
  bool GpioGet(tmc5160::InputPin pin, bool& level) noexcept {
    return (s.Next() != kFail) && s.wire.Level(pin, level);
  }
  void Log(tmc5160::LogLevel level, const char* message) noexcept {
    static_cast<void>(level);
    Check((message != nullptr) && !std::string_view{message}.empty()); // F11
  }
};

bool KnownError(Error e) noexcept {
  switch (e) {
  case Error::Ok:
  case Error::InvalidArgument:
  case Error::NotInitialized:
  case Error::Bus:
  case Error::Crc:
  case Error::Timeout:
  case Error::Busy:
  case Error::Unsupported:
  case Error::DeviceFault:
  case Error::DeviceIdMismatch:
  case Error::UnexpectedReset:
  case Error::VerifyFailed:
  case Error::Refused:
  case Error::ModeMismatch:
  case Error::LimitExceeded:
    return true;
  default:
    break;
  }
  return false;
}

tmc5160::Verify FuzzVerify(Input& in) noexcept {
  return static_cast<tmc5160::Verify>(in.Byte() % 5U); // 4 is not an enumerator
}

/// A declared motor and board, sometimes invalid on purpose; the model's sense resistor follows.
tmc5160::Config FuzzConfig(Input& in, uint32_t& rsense_mohm) noexcept {
  const uint8_t flags = in.Byte();
  tmc5160::Motor motor{};
  motor.rated_rms_ma = 200U + (in.Byte() * 8U);
  motor.coil_resistance_mohm = (in.Byte() % 4U) * 1500U;
  tmc5160::Board board{};
  board.rsense_mohm = 30U + (in.Byte() % 8U) * 40U;
  board.supply_mv = 8'000U + (in.Byte() % 50U) * 1'000U;
  tmc5160::Config c = tmc5160::MakeConfig(motor, board);
  c.mode = ((flags & 1U) != 0U) ? tmc5160::Mode::StepDir : tmc5160::Mode::MotionController;
  c.stealth.enable = (flags & 2U) != 0U;
  c.begin_enabled = (flags & 4U) != 0U;
  c.verify = ((flags & 8U) != 0U) ? tmc5160::Verify::ReadBack : tmc5160::Verify::None;
  c.chopper.mres = static_cast<tmc5160::Microsteps>(in.Byte() % 10U);
  c.ack = c.ack.With(((flags & 16U) != 0U) ? tmc5160::Ack::DirectMode : tmc5160::Ack::HoldAboveRun);
  if ((flags & 32U) != 0U) {
    c.ramp.vmax = in.Word() * 64U;
    c.ramp.amax = in.Word();
  }
  if ((flags & 64U) != 0U) {
    c.spi.chain_length = static_cast<uint8_t>(1U + (in.Byte() % 3U));
    c.spi.chain_index = static_cast<uint8_t>(in.Byte() % 3U);
  }
  if ((flags & 128U) != 0U) {
    c.uart.multi_node = true;
    c.uart.send_delay = static_cast<uint8_t>(in.Byte() % 16U);
  }
  rsense_mohm = board.rsense_mohm;
  return c;
}

void FuzzChip(Chip& chip, Input& in) noexcept {
  switch (in.Byte() % 12U) {
  case 0:
    chip.k.version_byte = (in.Byte() & 1U) != 0U ? in.Byte() : uint8_t{0x30};
    break;
  case 1:
    chip.k.sd_mode = !chip.k.sd_mode;
    break;
  case 2:
    chip.InjectShort();
    break;
  case 3:
    chip.ClearShort();
    chip.ClearTemperature();
    break;
  case 4:
    chip.InjectOvertemp((in.Byte() & 1U) != 0U);
    break;
  case 5:
    chip.InjectStall((in.Byte() & 1U) != 0U);
    break;
  case 6: {
    // One byte per named local: the order of argument evaluation is unspecified, so a byte read
    // in an argument list would mean different things on different compilers.
    const bool left = (in.Byte() & 1U) != 0U;
    const bool right = (in.Byte() & 1U) != 0U;
    chip.SetReferenceSwitches(left, right);
    break;
  }
  case 7:
    chip.k.wo_read_mode = static_cast<uint8_t>(in.Byte() % 3U);
    break;
  case 8:
    chip.k.echo_writes = !chip.k.echo_writes;
    break;
  case 9:
    chip.SetSgResult(in.Word());
    break;
  case 10:
    chip.InjectChargePumpUv();
    break;
  default:
    chip.PowerOn(); // a brown-out: reset values, GSTAT.reset set
    break;
  }
}

template <typename Port>
class Run {
public:
  Run(Port& port, tmc5160::TMC5160<Port>& driver, uint32_t rated_ma) noexcept
      : port_(port), d_(driver), rated_ma_(rated_ma) {}

  void One(Input& in) noexcept {
    const uint8_t op = in.Byte() % 34U;
    const bool was_up = d_.IsInitialized();
    const int before = port_.s.sent;
    const Chip& first = port_.s.wire.ChipAt(0);
    const bool stalled_before =
        ((first.RampEvents() & (1U << 6)) != 0U) && (first.VelocityHz() == 0.0) &&
        ((first.Reg(tmc5160_test::m::SW_MODE) & (1U << 10)) != 0U) && !port_.s.tampered;
    switch (op) {
    case 0: {
      const auto r = d_.EnsureInitialized();
      result(r, before);
      if (r.ok() && !was_up && motion && !port_.s.tampered) {
        // F18: a started driver has its ramp generator in position mode (the last session's
        // velocity run is ended).
        Check(firstRampMode() == 0U);
      }
      if (!r.ok() && !was_up && (port_.s.sent > before)) {
        // F14: a start-up that failed after its first write leaves the driver off and not
        // started, and the chip's bridge off unless the bus itself failed (then the quiesce write
        // may not have been deliverable and the error says so).
        Check(!d_.IsEnabled() && !d_.IsInitialized());
        const bool bus_error = (r.error == Error::Bus) || (r.error == Error::Timeout) ||
                               (r.error == Error::Crc) || (r.error == Error::VerifyFailed);
        if (!bus_error && !(is_spi && port_.s.tampered)) {
          Check((port_.s.wire.ChipAt(0).Reg(tmc5160_test::m::CHOPCONF) & 0xFU) == 0U);
        }
      }
      break;
    }
    case 1:
      result(d_.Enable(FuzzVerify(in)), before);
      break;
    case 2: {
      const auto r = d_.Disable(FuzzVerify(in));
      result(r, before);
      if (r.ok() && !port_.s.tampered && (port_.s.wire.count == 1U)) {
        // F19: a switch-off that reports success reached the chip, whether or
        // not the driver had reported a reset: DRV_ENN inactive and TOFF 0.
        Check(!first.DrvEnnActive());
        Check((first.Reg(tmc5160_test::m::CHOPCONF) & 0xFU) == 0U);
      }
      break;
    }
    case 3:
      result(d_.ClearFaults(), before);
      break;
    case 4: {
      const uint32_t run = in.Word() % 3000U;
      const uint32_t hold = in.Word() % 3000U;
      const tmc5160::Verify verify = FuzzVerify(in);
      result(d_.SetCurrent(run, hold, verify), before);
      break;
    }
    case 5:
      result(d_.MoveTo(static_cast<int32_t>(in.Word() << 8U)), before);
      break;
    case 6:
      result(d_.MoveBy(static_cast<int32_t>(in.Word()) - 32768), before);
      break;
    case 7:
      result(d_.RunVelocity(static_cast<int32_t>(in.Word() * 64U) - 1'000'000), before);
      break;
    case 8: {
      const auto kind = static_cast<tmc5160::StopKind>(in.Byte() % 5U);
      const auto r = d_.Stop(kind, FuzzVerify(in));
      result(r, before);
      if (r.ok() && (kind != tmc5160::StopKind::Emergency)) {
        directStopCheck(first);
        stopCheck(first);
      }
      break;
    }
    case 9: {
      const auto r = d_.Hold();
      result(r, before);
      if (r.ok()) {
        directStopCheck(first);
      }
      break;
    }
    case 10:
      result(d_.SetHomePosition(static_cast<int32_t>(in.Word())), before);
      break;
    case 11: {
      const auto r = d_.ReadPosition();
      result(r, before);
      break;
    }
    case 12: {
      const auto r = d_.ReadVelocity();
      result(r, before);
      if (r.ok()) {
        Check((r.value >= -(1 << 23)) && (r.value < (1 << 23))); // F10
      }
      break;
    }
    case 13:
      result(d_.ReadSnapshot(), before);
      break;
    case 14:
      result(d_.Service(), before);
      break;
    case 15:
      result(d_.ReadGlobalStatus(), before);
      break;
    case 16:
      result(d_.ReadDriverStatus(), before);
      break;
    case 17: {
      tmc5160::StealthChop s;
      s.enable = (in.Byte() & 1U) != 0U;
      s.reg = static_cast<uint8_t>(in.Byte() % 18U);
      result(d_.SetStealthChop(s, tmc5160::TuningPolicy::HoldAtRunForAt1, FuzzVerify(in)), before);
      break;
    }
    case 18: {
      tmc5160::Chopper c;
      c.toff = static_cast<uint8_t>(in.Byte() % 17U);
      c.tbl = static_cast<uint8_t>(in.Byte() % 5U);
      result(d_.SetChopper(c), before);
      break;
    }
    case 19: {
      tmc5160::Thresholds t;
      t.pwm = in.Word() * 16U;
      t.cool = in.Word() * 8U;
      t.high = in.Word();
      result(d_.SetThresholds(t), before);
      break;
    }
    case 20: {
      tmc5160::Bridge b;
      b.declared = true;
      b.bbm_time = static_cast<uint8_t>(in.Byte() % 27U);
      result(d_.SetBridge(b, tmc5160::AckSet{in.Byte()}), before);
      break;
    }
    case 21: {
      tmc5160::Protection p;
      p.declared = true;
      p.s2g_level = static_cast<uint8_t>(in.Byte() % 17U);
      p.disable_s2g = (in.Byte() & 1U) != 0U;
      result(d_.SetProtection(p, tmc5160::AckSet{in.Byte()}), before);
      break;
    }
    case 22:
      result(d_.EnterDirectMode(), before);
      break;
    case 23: {
      const int32_t coil_a = static_cast<int32_t>(in.Byte()) - 128;
      const int32_t coil_b = static_cast<int32_t>(in.Byte()) - 128;
      result(d_.SetDirectCurrents(coil_a, coil_b), before);
      break;
    }
    case 24:
      result(d_.ExitDirectMode(), before);
      break;
    case 25: {
      const uint8_t from_node = in.Byte();
      const uint8_t new_node = in.Byte();
      const bool nao_level = (in.Byte() & 1U) != 0U;
      const auto r = d_.ProgramNode(from_node, new_node, nao_level);
      result(r, before);
      if (is_spi) {
        Check((r.error == Error::Unsupported) || (r.error == Error::NotInitialized)); // F16
      }
      break;
    }
    case 26: {
      const auto r = d_.Deinitialize();
      Check(KnownError(r.error) && !d_.IsInitialized()); // F15
      break;
    }
    case 27:
      result(d_.SetMicrostepTable(tmc5160::MicrostepTable{}), before);
      break;
    case 28:
      result(d_.Diag0Active(), before);
      break;
    case 30:
      result(d_.SetStopOnStall((in.Byte() & 1U) != 0U), before);
      break;
    case 31:
      result(d_.ReleaseStall(), before);
      break;
    case 32: {
      tmc5160::Switches sw{};
      const uint8_t bits = in.Byte();
      sw.stop_left = (bits & 1U) != 0U;
      sw.stop_right = (bits & 2U) != 0U;
      sw.stop_on_stall = (bits & 4U) != 0U;
      sw.soft_stop = (bits & 8U) != 0U;
      result(d_.SetSwitches(sw), before);
      break;
    }
    case 33: {
      const auto r = d_.EnsureInitialized();
      result(r, before);
      break;
    }
    default:
      FuzzChip(port_.s.wire.ChipAt(in.Byte() % port_.s.wire.count), in);
      port_.s.armed = port_.s.armed || (port_.s.wire.ChipAt(0).reset_count > 1U);
      port_.s.bus_up = (in.Byte() & 0x07U) != 0U;
      break;
    }
    invariants();
    // F17: a stall stop that is latched and has stopped the motor is released only by
    // `ReleaseStall()` (or by a chip reset or an explicit motion command).
    const bool asks_motion = (op == 5U) || (op == 6U) || (op == 7U) || (op == 29U) || (op == 31U);
    if (stalled_before && !asks_motion && !port_.s.tampered) {
      const uint32_t mode = first.Reg(tmc5160_test::m::RAMPMODE);
      const bool stop_on = (first.Reg(tmc5160_test::m::SW_MODE) & (1U << 10)) != 0U;
      // Still held: the chip keeps the stop, or the ramp ends where the motor stands.
      Check((first.VelocityHz() == 0.0) &&
            (stop_on || ((mode == 0U) && (first.Xtarget() == first.Xactual())) || (mode == 3U)));
    }
  }

  bool is_spi = false;
  bool motion = false;

private:
  /// F20: a stop or hold that succeeded on a chip in direct mode left the coil
  /// currents at zero (XTARGET holds them there, page 110), never XACTUAL,
  /// never above the 248 limit.
  /// F21: a stop that succeeded on a motion-controller chip left the ramp generator commanded to
  /// stop: velocity mode with VMAX 0, or position mode with XTARGET = XACTUAL (never Hold, which
  /// keeps the speed).
  void stopCheck(const Chip& first) noexcept {
    const bool direct = (first.Reg(tmc5160_test::m::GCONF) & (1U << 16U)) != 0U;
    if (motion && !direct && !port_.s.tampered && (port_.s.wire.count == 1U)) {
      const uint32_t mode = first.Reg(tmc5160_test::m::RAMPMODE);
      const bool velocity_stop =
          ((mode == 1U) || (mode == 2U)) && (first.Reg(tmc5160_test::m::VMAX) == 0U);
      const bool position_stop = (mode == 0U) && (first.Xtarget() == first.Xactual());
      Check(velocity_stop || position_stop);
    }
  }

  void directStopCheck(const Chip& first) noexcept {
    const bool direct = (first.Reg(tmc5160_test::m::GCONF) & (1U << 16U)) != 0U;
    if (direct && !port_.s.tampered && (port_.s.wire.count == 1U)) {
      Check(first.Xtarget() == 0);
    }
  }

  template <typename R>
  void result(const R& r, int before) noexcept {
    Check(KnownError(r.error)); // F1
    if (r.error == Error::NotInitialized) {
      Check(port_.s.sent == before); // F2
    }
    if (r.error == Error::InvalidArgument) {
      Check(port_.s.sent == before); // F3
    }
    if (r.error == Error::UnexpectedReset) {
      Check(!d_.IsInitialized()); // F4
    }
    if ((r.error == Error::Refused) || (r.error == Error::LimitExceeded) ||
        (r.error == Error::Busy)) {
      // F13: a refusal never leaves a bridge on at a current above the rating.
      Check(!d_.IsEnabled() || (d_.GetCurrentPlan().run_ma <= rated_ma_));
    }
  }

  [[nodiscard]] uint32_t firstRampMode() const noexcept {
    return port_.s.wire.ChipAt(0).Reg(tmc5160_test::m::RAMPMODE);
  }

  void invariants() noexcept {
    for (std::size_t i = 0; i < port_.s.wire.count; ++i) {
      const Chip& chip = port_.s.wire.ChipAt(i);
      Check(chip.forbidden_writes == 0U);                                             // F5
      Check(chip.reserved_bit_writes == 0U);                                          // F6
      Check(chip.wo_reads == 0U);                                                     // F7
      Check(port_.s.exempt || (chip.peak_coil_ma <= static_cast<double>(rated_ma_))); // F8
    }
    Check(!d_.IsEnabled() || d_.IsInitialized()); // F9
    if (d_.HasFault()) {
      Check(d_.IsInitialized()); // a fault is only ever latched by a started driver
    }
  }

  Port& port_;
  tmc5160::TMC5160<Port>& d_;
  uint32_t rated_ma_;
};

template <typename Port>
void Drive(Input& in, bool spi) noexcept {
  Port port(in);
  uint32_t rsense = 100U;
  const tmc5160::Config config = FuzzConfig(in, rsense);
  port.s.wire.count = spi ? config.spi.chain_length : 1U;
  for (std::size_t i = 0; i < port.s.wire.count; ++i) {
    port.s.wire.ChipAt(i).k.rsense_mohm = rsense;
    port.s.wire.ChipAt(i).k.sd_mode = config.mode == tmc5160::Mode::StepDir;
  }
  tmc5160::TMC5160<Port> driver(port, config);
  Run<Port> run(port, driver, config.motor.rated_rms_ma);
  run.is_spi = spi;
  run.motion = config.mode == tmc5160::Mode::MotionController;
  for (int calls = 0; (calls < 96) && !in.Empty(); ++calls) {
    run.One(in);
  }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  Input in(data, size);
  if ((in.Byte() & 1U) != 0U) {
    Drive<FuzzSpiPort>(in, true);
  } else {
    Drive<FuzzUartPort>(in, false);
  }
  return 0;
}
