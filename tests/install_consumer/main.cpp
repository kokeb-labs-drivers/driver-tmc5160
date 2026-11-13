/**
 * @file main.cpp
 * @brief Builds the driver from the installed package only: headers, version header and .ipp.
 */
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "tmc5160.hpp"

namespace {

// A port that answers every read with 0: enough to instantiate the whole driver template.
class NullPort : public tmc5160::SpiInterface<NullPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    static_cast<void>(tx);
    for (std::size_t i = 0; (rx != nullptr) && (i < len); ++i) {
      rx[i] = 0;
    }
    return true;
  }
  void DelayUs(uint32_t us) noexcept {
    static_cast<void>(us);
  }
};

} // namespace

int main() {
  NullPort port;
  const tmc5160::Config config =
      tmc5160::MakeConfig(tmc5160::Motor{.rated_rms_ma = 1000, .coil_resistance_mohm = 6000},
                          tmc5160::Board{.rsense_mohm = 100, .supply_mv = 24'000});
  tmc5160::TMC5160<NullPort> motor(port, config);
  // Every read returns 0, so the chip's VERSION is wrong and EnsureInitialized() must fail: proof
  // the template compiled and ran.
  const auto r = motor.EnsureInitialized();
  return (!r && !std::string_view{tmc5160::GetDriverVersion()}.empty()) ? 0 : 1;
}
