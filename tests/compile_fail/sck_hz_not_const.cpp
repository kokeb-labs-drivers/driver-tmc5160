/**
 * @file sck_hz_not_const.cpp
 * @brief Must NOT compile: an SckHz hook with the wrong signature is rejected, not ignored.
 *
 * EXPECT: SckHz hook must be
 */
#include <cstddef>
#include <cstdint>

#include "tmc5160.hpp"

class BadSckPort : public tmc5160::SpiInterface<BadSckPort> {
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
  uint16_t SckHz() const noexcept {
    return 1000U;
  }
};

int main() {
  BadSckPort port;
  tmc5160::TMC5160<BadSckPort> driver(port);
  return static_cast<int>(driver.EnsureInitialized().error);
}
