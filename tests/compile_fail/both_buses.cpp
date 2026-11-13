/**
 * @file both_buses.cpp
 * @brief Must NOT compile: a port that is both an SPI and a UART port has no defined bus, so the
 * driver rejects it.
 *
 * EXPECT: AnyPort
 */
#include <cstddef>
#include <cstdint>

#include "tmc5160.hpp"

class TwoBusPort : public tmc5160::SpiInterface<TwoBusPort>,
                   public tmc5160::UartInterface<TwoBusPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    static_cast<void>(tx);
    static_cast<void>(rx);
    return len > 0U;
  }
  tmc5160::BusStatus Exchange(const uint8_t* tx, std::size_t tx_len, uint8_t* rx,
                              std::size_t rx_len, uint32_t timeout_us) noexcept {
    static_cast<void>(tx);
    static_cast<void>(tx_len);
    static_cast<void>(rx);
    static_cast<void>(rx_len);
    static_cast<void>(timeout_us);
    return tmc5160::BusStatus::Ok;
  }
  void DelayUs(uint32_t us) noexcept {
    static_cast<void>(us);
  }
};

int main() {
  TwoBusPort port;
  tmc5160::TMC5160<TwoBusPort> driver(port);
  return static_cast<int>(driver.EnsureInitialized().error);
}
