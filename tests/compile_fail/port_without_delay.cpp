/**
 * @file port_without_delay.cpp
 * @brief Must NOT compile: a port without DelayUs fails at the first call that waits, with a
 * message that names the hook.
 *
 * EXPECT: must implement
 */
#include <cstddef>
#include <cstdint>

#include "tmc5160.hpp"

class NoDelayPort : public tmc5160::SpiInterface<NoDelayPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    static_cast<void>(tx);
    static_cast<void>(rx);
    return len > 0U;
  }
};

int main() {
  NoDelayPort port;
  tmc5160::TMC5160<NoDelayPort> driver(port);
  return static_cast<int>(driver.EnsureInitialized().error);
}
