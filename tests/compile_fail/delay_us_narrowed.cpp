/**
 * @file delay_us_narrowed.cpp
 * @brief Must NOT compile: a DelayUs hook with a narrower parameter would truncate long waits, so
 * the driver rejects it instead of using it (handbook KB-CPP20-007).
 *
 * EXPECT: DelayUs hook must be
 */
#include <cstddef>
#include <cstdint>

#include "tmc5160.hpp"

class ShortDelayPort : public tmc5160::SpiInterface<ShortDelayPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    static_cast<void>(tx);
    static_cast<void>(rx);
    return len > 0U;
  }
  void DelayUs(uint16_t us) noexcept {
    static_cast<void>(us);
  }
};

int main() {
  ShortDelayPort port;
  tmc5160::TMC5160<ShortDelayPort> driver(port);
  return static_cast<int>(driver.EnsureInitialized().error);
}
