/**
 * @file uart_exchange_narrowed.cpp
 * @brief Must NOT compile: an Exchange hook with a narrower length would truncate every frame, so
 * the UART port concept rejects it (handbook KB-CPP20-006).
 *
 * EXPECT: UartPort
 */
#include <cstddef>
#include <cstdint>

#include "tmc5160.hpp"

class NarrowUartPort : public tmc5160::UartInterface<NarrowUartPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  tmc5160::BusStatus Exchange(const uint8_t* tx, uint8_t tx_len, uint8_t* rx, std::size_t rx_len,
                              uint32_t timeout_us) noexcept {
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
  NarrowUartPort port;
  tmc5160::TMC5160<NarrowUartPort> driver(port);
  return static_cast<int>(driver.EnsureInitialized().error);
}
