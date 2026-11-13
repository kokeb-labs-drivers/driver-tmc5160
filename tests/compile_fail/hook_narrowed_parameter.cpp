/**
 * @file hook_narrowed_parameter.cpp
 * @brief Must NOT compile: a Transfer hook with a narrower length would truncate every transfer, so
 * the port concept rejects it (handbook KB-CPP20-006).
 *
 * EXPECT: SpiPort
 */
#include <cstddef>
#include <cstdint>

#include "tmc5160.hpp"

class NarrowPort : public tmc5160::SpiInterface<NarrowPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, uint16_t len) noexcept {
    static_cast<void>(tx);
    static_cast<void>(rx);
    return len > 0U;
  }
  void DelayUs(uint32_t us) noexcept {
    static_cast<void>(us);
  }
};

int main() {
  NarrowPort port;
  tmc5160::TMC5160<NarrowPort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
