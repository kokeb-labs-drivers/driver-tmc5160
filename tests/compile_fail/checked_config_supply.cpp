/**
 * @file checked_config_supply.cpp
 * @brief Must NOT compile: a declared supply below 10 V is outside the datasheet range (page 120),
 * so the `CheckedConfig` fails the build.
 *
 * EXPECT: ConfigIsInvalid
 */
#include "tmc5160_types.hpp"

int main() {
  constexpr tmc5160::CheckedConfig kBad{
      tmc5160::MakeConfig(tmc5160::Motor{.rated_rms_ma = 1000},
                          tmc5160::Board{.rsense_mohm = 100, .supply_mv = 5'000})};
  return static_cast<int>(kBad.Get().mode);
}
