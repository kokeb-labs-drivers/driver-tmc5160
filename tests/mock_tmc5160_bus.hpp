/**
 * @file mock_tmc5160_bus.hpp
 * @brief Mock ports over the chip model: SPI (one chip or a daisy chain) and UART (one or several
 * nodes).
 *
 * | Port | Bus | Hooks |
 * | --- | --- | --- |
 * | `MinimalSpiPort` | SPI | the required ones plus `DelayUs()` |
 * | `FullSpiPort` | SPI | also `GpioSet()`, `GpioGet()`, `SckHz()` and `Log()` |
 * | `MinimalUartPort` | UART | the required ones plus `DelayUs()` |
 * | `FullUartPort` | UART | also `GpioSet()`, `GpioGet()` and `Log()` |
 *
 * The bus fault injector (`Faults`) breaks one transfer, every transfer from some index on,
 * corrupts a reply bit, or holds MISO or RX at one level.
 */
#ifndef TMC5160_MOCK_TMC5160_BUS_HPP
#define TMC5160_MOCK_TMC5160_BUS_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "mock_tmc5160.hpp"
#include "tmc5160_spi_interface.hpp"
#include "tmc5160_types.hpp"
#include "tmc5160_uart_interface.hpp"

namespace tmc5160_test {

/// Bus fault injection shared by the ports.
struct Faults {
  int fail_at = -1;        ///< Fail the transfer with this index (0-based).
  int fail_from = -1;      ///< Fail every transfer from this index on.
  int corrupt_at = -1;     ///< Flip one bit of the reply of this transfer.
  int corrupt_from = -1;   ///< Flip one bit of the reply of every transfer from this index on.
  int patch_from = -1;     ///< From this transfer on, flip `patch_xor` in reply byte `patch_index`.
  uint8_t patch_index = 0; ///< UART reply byte the patch flips (1 master address, 2 register).
  uint8_t patch_xor = 0;   ///< The bits the patch flips.
  uint8_t corrupt_mask = 1; ///< The bit flipped.
  int drop_reply_at = -1;   ///< UART: no reply to this exchange (timeout).
  int corrupt_tx_at =
      -1; ///< UART: flip a bit of the request of this exchange (the chip sees a bad CRC).
  int corrupt_mosi_at =
      -1;            ///< SPI: flip a data bit of the datagram the chip receives in this transfer.
  uint8_t stuck = 0; ///< 0 none, 1 line held low, 2 line held high: every reply byte.
  bool bus_up_ok = true; ///< `EnsureInitialized()` result.
  uint8_t spi_mode = 3;  ///< The SPI mode the port runs; the chip understands mode 3 only (A1).
  uint32_t sck_hz = 1'000'000; ///< `SckHz()` result.
};

/// Chips, pins and counters the ports share.
struct Wire {
  std::array<Chip, 8>
      chips{};           ///< The chips: one for a plain bus, more for a chain or several nodes.
  std::size_t count = 1; ///< How many of them are on the bus.
  Faults faults{};
  int transfers = 0;             ///< Transfers so far.
  int bus_up_calls = 0;          ///< `EnsureInitialized()` calls.
  uint64_t delayed_us = 0;       ///< Sum of `DelayUs()` arguments.
  uint32_t xfer_us = 0;          ///< Model time each transfer takes (not counted in `delayed_us`).
  uint32_t longest_transfer = 0; ///< Longest SPI transfer, bytes.
  int log_lines = 0;
  int error_lines = 0;
  bool gpio_ok = true;
  int drv_enn_writes = 0;
  int last_corrupted = -1;
  const uint8_t* last_tx = nullptr; ///< The buffers of the latest transfer (where they live).
  const uint8_t* last_rx = nullptr;

  Chip& ChipAt(std::size_t i = 0) noexcept {
    return chips[i];
  }

  void Delay(uint32_t us) noexcept {
    delayed_us += us;
    for (std::size_t i = 0; i < count; ++i) {
      chips[i].Elapse(us);
    }
  }

  /// The model time of one transfer: the chips run on while the bytes are on the line.
  void Tick() noexcept {
    for (std::size_t i = 0; (xfer_us != 0U) && (i < count); ++i) {
      chips[i].Elapse(xfer_us);
    }
  }

  [[nodiscard]] bool ShouldFail(int index) const noexcept {
    return (index == faults.fail_at) || ((faults.fail_from >= 0) && (index >= faults.fail_from));
  }

  /// A SPI transfer through the chain: chip `i` owns bytes `5 * (count - 1 - i)` to `+4` (index 0
  /// on MOSI, A34).
  bool Spi(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    const int index = transfers++;
    Tick();
    last_tx = tx;
    last_rx = rx;
    if (len > longest_transfer) {
      longest_transfer = static_cast<uint32_t>(len);
    }
    if (ShouldFail(index) || (tx == nullptr) || (rx == nullptr) || (len == 0U)) {
      return false;
    }
    for (std::size_t i = 0; i < len; ++i) {
      rx[i] = 0;
    }
    if (faults.spi_mode != 3U) {
      for (std::size_t i = 0; i < len; ++i) {
        rx[i] = 0xFF; // the chip never frames the data
      }
      return true;
    }
    if (len == (5U * count)) { // a frame of any other length is not committed
      std::array<uint8_t, std::size_t{40}> seen{};
      for (std::size_t i = 0; (i < len) && (i < seen.size()); ++i) {
        seen[i] = tx[i];
      }
      if (index == faults.corrupt_mosi_at) {
        seen[len - 1U] =
            static_cast<uint8_t>(seen[len - 1U] ^ 0x10U); // a bit of the last data byte
      }
      for (std::size_t i = 0; i < count; ++i) {
        const std::size_t slot = chips[0].k.chain_order ? 5U * i : 5U * (count - 1U - i);
        chips[i].SpiDatagram(seen.data() + slot, rx + slot);
      }
    }
    Impair(index, rx, len);
    return true;
  }

  /// A UART exchange: the frame goes to every node; the addressed one answers.
  tmc5160::BusStatus Uart(const uint8_t* tx, std::size_t tx_len, uint8_t* rx, std::size_t rx_len,
                          uint32_t timeout_us) noexcept {
    last_tx = tx;
    last_rx = rx;
    const int index = transfers++;
    Tick();
    if (ShouldFail(index)) {
      return tmc5160::BusStatus::Fault;
    }
    std::array<uint8_t, 8> reply{};
    std::array<uint8_t, 8> request{};
    for (std::size_t i = 0; (i < tx_len) && (i < request.size()); ++i) {
      request[i] = tx[i];
    }
    if (index == faults.corrupt_tx_at) {
      request[tx_len - 1U] = static_cast<uint8_t>(request[tx_len - 1U] ^ 0x10U);
    }
    std::size_t reply_len = 0;
    for (std::size_t i = 0; i < count; ++i) {
      const std::size_t n = chips[i].UartFrame(request.data(), tx_len, reply.data());
      if (n != 0U) {
        reply_len = n;
      }
    }
    if (rx_len == 0U) {
      return tmc5160::BusStatus::Ok;
    }
    double reply_delay_us = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
      reply_delay_us =
          chips[i].ReplyDelayUs() > reply_delay_us ? chips[i].ReplyDelayUs() : reply_delay_us;
    }
    if ((reply_len != rx_len) || (index == faults.drop_reply_at) ||
        (reply_delay_us > static_cast<double>(timeout_us))) {
      return tmc5160::BusStatus::Timeout;
    }
    for (std::size_t i = 0; i < rx_len; ++i) {
      rx[i] = reply[i];
    }
    if ((faults.patch_from >= 0) && (index >= faults.patch_from) && (faults.patch_index < rx_len)) {
      rx[faults.patch_index] = static_cast<uint8_t>(rx[faults.patch_index] ^ faults.patch_xor);
    }
    Impair(index, rx, rx_len);
    return tmc5160::BusStatus::Ok;
  }

  void Impair(int index, uint8_t* rx, std::size_t len) noexcept {
    if (((index == faults.corrupt_at) ||
         ((faults.corrupt_from >= 0) && (index >= faults.corrupt_from))) &&
        (len > 2U)) {
      rx[len - 2U] = static_cast<uint8_t>(rx[len - 2U] ^ faults.corrupt_mask);
      last_corrupted = index;
    }
    if (faults.stuck != 0U) {
      for (std::size_t i = 0; i < len; ++i) {
        rx[i] = faults.stuck == 1U ? 0x00U : 0xFFU;
      }
    }
  }

  bool Pin(tmc5160::CtrlPin pin, tmc5160::GpioSignal signal) noexcept {
    if (!gpio_ok) {
      return false;
    }
    if (pin == tmc5160::CtrlPin::DrvEnn) {
      ++drv_enn_writes;
      for (std::size_t i = 0; i < count; ++i) {
        chips[i].SetDrvEnn(signal == tmc5160::GpioSignal::ACTIVE);
      }
    }
    return true;
  }

  bool Level(tmc5160::InputPin pin, bool& level) noexcept {
    if (!gpio_ok) {
      return false;
    }
    level = pin == tmc5160::InputPin::Diag0 ? chips[0].Diag0Level() : chips[0].Diag1Level();
    return true;
  }
};

/// The required SPI hooks plus the delay.
class MinimalSpiPort : public tmc5160::SpiInterface<MinimalSpiPort> {
public:
  Wire board;
  bool EnsureInitialized() noexcept {
    ++board.bus_up_calls;
    return board.faults.bus_up_ok;
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    return board.Spi(tx, rx, len);
  }
  void DelayUs(uint32_t us) noexcept {
    board.Delay(us);
  }
};

/// Every optional hook of the SPI port.
class FullSpiPort : public tmc5160::SpiInterface<FullSpiPort> {
public:
  Wire board;
  bool EnsureInitialized() noexcept {
    ++board.bus_up_calls;
    return board.faults.bus_up_ok;
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    return board.Spi(tx, rx, len);
  }
  void DelayUs(uint32_t us) noexcept {
    board.Delay(us);
  }
  bool GpioSet(tmc5160::CtrlPin pin, tmc5160::GpioSignal signal) noexcept {
    return board.Pin(pin, signal);
  }
  bool GpioGet(tmc5160::InputPin pin, bool& level) noexcept {
    return board.Level(pin, level);
  }
  [[nodiscard]] uint32_t SckHz() const noexcept {
    return board.faults.sck_hz;
  }
  void Log(tmc5160::LogLevel level, const char* message) noexcept {
    static_cast<void>(message);
    ++board.log_lines;
    if (level == tmc5160::LogLevel::Error) {
      ++board.error_lines;
    }
  }
};

/// The required UART hooks plus the delay.
class MinimalUartPort : public tmc5160::UartInterface<MinimalUartPort> {
public:
  Wire board;
  bool EnsureInitialized() noexcept {
    ++board.bus_up_calls;
    return board.faults.bus_up_ok;
  }
  tmc5160::BusStatus Exchange(const uint8_t* tx, std::size_t tx_len, uint8_t* rx,
                              std::size_t rx_len, uint32_t timeout_us) noexcept {
    return board.Uart(tx, tx_len, rx, rx_len, timeout_us);
  }
  void DelayUs(uint32_t us) noexcept {
    board.Delay(us);
  }
};

/// Every optional hook of the UART port.
class FullUartPort : public tmc5160::UartInterface<FullUartPort> {
public:
  Wire board;
  bool EnsureInitialized() noexcept {
    ++board.bus_up_calls;
    return board.faults.bus_up_ok;
  }
  tmc5160::BusStatus Exchange(const uint8_t* tx, std::size_t tx_len, uint8_t* rx,
                              std::size_t rx_len, uint32_t timeout_us) noexcept {
    return board.Uart(tx, tx_len, rx, rx_len, timeout_us);
  }
  void DelayUs(uint32_t us) noexcept {
    board.Delay(us);
  }
  bool GpioSet(tmc5160::CtrlPin pin, tmc5160::GpioSignal signal) noexcept {
    return board.Pin(pin, signal);
  }
  bool GpioGet(tmc5160::InputPin pin, bool& level) noexcept {
    return board.Level(pin, level);
  }
  void Log(tmc5160::LogLevel level, const char* message) noexcept {
    static_cast<void>(message);
    ++board.log_lines;
    if (level == tmc5160::LogLevel::Error) {
      ++board.error_lines;
    }
  }
};

} // namespace tmc5160_test

#endif // TMC5160_MOCK_TMC5160_BUS_HPP
