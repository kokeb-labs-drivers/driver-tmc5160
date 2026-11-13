/**
 * @file gpio_set_returns_void.cpp
 * @brief Must NOT compile: an optional hook with the wrong signature is rejected, not ignored.
 *
 * EXPECT: GpioSet hook must be
 */
#include <cstddef>
#include <cstdint>

#include "tmc5160.hpp"

class VoidGpioPort : public tmc5160::SpiInterface<VoidGpioPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    static_cast<void>(tx);
    static_cast<void>(rx);
    return len > 0U;
  }
  void DelayUs(uint32_t us) noexcept {
    static_cast<void>(us);
  }
  void GpioSet(tmc5160::CtrlPin pin, tmc5160::GpioSignal signal) noexcept {
    static_cast<void>(pin);
    static_cast<void>(signal);
  }
};

int main() {
  VoidGpioPort port;
  tmc5160::TMC5160<VoidGpioPort> driver(port);
  return static_cast<int>(driver.EnsureInitialized().error);
}
