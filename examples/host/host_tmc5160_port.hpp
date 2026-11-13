/**
 * @file host_tmc5160_port.hpp
 * @brief The host ports: the driver's bus hooks over a simulated TMC5160, so the examples run on
 * any PC.
 *
 * `tmc5160_test::Wire` (tests/mock_tmc5160_bus.hpp) holds the chip model the host tests use and
 * does the framing of both buses. The two classes below add what a real port adds: `DelayUs()`
 * (here it advances the simulated chips' clock instead of waiting), the pins (`GpioSet()` for
 * DRV_ENN, `GpioGet()` for DIAG0 and DIAG1) and `Log()` to the console. A port for a real bus on a
 * PC (Linux spidev, a USB bridge) implements the same hooks: see docs/porting.md.
 */
#ifndef TMC5160_HOST_TMC5160_PORT_HPP
#define TMC5160_HOST_TMC5160_PORT_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "mock_tmc5160_bus.hpp"
#include "tmc5160.hpp"

/// The SPI host port: simulated chip or chain, real console.
class HostSpiPort : public tmc5160::SpiInterface<HostSpiPort> {
public:
  /// The bus settings a real port would use; checked against the driver's bus requirements
  /// (handbook KB-CPP-123).
  static constexpr uint32_t kClockHz = 1'000'000;
  static constexpr uint8_t kMode = 3;
  static_assert(kClockHz <= tmc5160::kMaxClockHz, "clock above the TMC5160's f_SCK max");
  static_assert(kMode == tmc5160::kSpiMode, "the TMC5160 talks SPI mode 3");

  tmc5160_test::Wire wire; ///< The simulated chips behind the bus.

  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    return wire.Spi(tx, rx, len);
  }
  /// Time passes in the simulated chips; nothing sleeps.
  void DelayUs(uint32_t us) noexcept {
    wire.Delay(us);
  }
  bool GpioSet(tmc5160::CtrlPin pin, tmc5160::GpioSignal signal) noexcept {
    return wire.Pin(pin, signal);
  }
  bool GpioGet(tmc5160::InputPin pin, bool& level) noexcept {
    return wire.Level(pin, level);
  }
  /// Driver messages (they carry their own `tmc5160:` prefix) go to the console, one line each.
  void Log(tmc5160::LogLevel level, const char* message) noexcept {
    static_cast<void>(level);
    static_cast<void>(std::puts(message)); // plain text: never pass it as a format string
  }
};

/// The UART host port: one or several simulated nodes on one line.
class HostUartPort : public tmc5160::UartInterface<HostUartPort> {
public:
  tmc5160_test::Wire wire; ///< The simulated nodes behind the line.

  bool EnsureInitialized() noexcept {
    return true;
  }
  tmc5160::BusStatus Exchange(const uint8_t* tx, std::size_t tx_len, uint8_t* rx,
                              std::size_t rx_len, uint32_t timeout_us) noexcept {
    return wire.Uart(tx, tx_len, rx, rx_len, timeout_us);
  }
  void DelayUs(uint32_t us) noexcept {
    wire.Delay(us);
  }
  bool GpioSet(tmc5160::CtrlPin pin, tmc5160::GpioSignal signal) noexcept {
    return wire.Pin(pin, signal);
  }
  bool GpioGet(tmc5160::InputPin pin, bool& level) noexcept {
    return wire.Level(pin, level);
  }
  void Log(tmc5160::LogLevel level, const char* message) noexcept {
    static_cast<void>(level);
    static_cast<void>(std::puts(message));
  }
};

/// A declared 1 A motor on 100 mOhm sense resistors at 24 V: every example starts from this.
inline tmc5160::Config ExampleConfig() {
  tmc5160::Config c =
      tmc5160::MakeConfig(tmc5160::Motor{.rated_rms_ma = 1000, .coil_resistance_mohm = 6000},
                          tmc5160::Board{.rsense_mohm = 100, .supply_mv = 24'000});
  c.ramp.a1 = 20'000;
  c.ramp.amax = 20'000;
  c.ramp.dmax = 20'000;
  c.ramp.d1 = 20'000;
  c.ramp.v1 = 50'000;
  c.ramp.vmax = 100'000;
  return c;
}

/// Print and count a failed call; examples exit non-zero when any call failed.
#define EXAMPLE_CHECK(expr)                                                                        \
  do {                                                                                             \
    const auto example_result = (expr);                                                            \
    if (!example_result) {                                                                         \
      std::printf("%s:%d: %s failed: %s\n", __FILE__, __LINE__, #expr,                             \
                  tmc5160::ToString(example_result.error));                                        \
      return 1;                                                                                    \
    }                                                                                              \
  } while (0)

#endif // TMC5160_HOST_TMC5160_PORT_HPP
