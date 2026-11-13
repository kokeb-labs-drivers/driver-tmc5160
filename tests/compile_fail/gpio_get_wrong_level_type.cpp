/**
 * @file gpio_get_wrong_level_type.cpp
 * @brief Must NOT compile: a GpioGet hook that returns the level instead of filling it is rejected,
 * not ignored.
 *
 * EXPECT: GpioGet hook must be
 */
#include <cstddef>
#include <cstdint>

#include "tmc5160.hpp"

class WrongGetPort : public tmc5160::SpiInterface<WrongGetPort> {
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
  bool GpioGet(tmc5160::InputPin pin, bool level) noexcept {
    static_cast<void>(pin);
    return level;
  }
};

int main() {
  WrongGetPort port;
  tmc5160::TMC5160<WrongGetPort> driver(port);
  return static_cast<int>(driver.EnsureInitialized().error);
}
