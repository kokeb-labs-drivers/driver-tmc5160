/**
 * @file checked_config_invalid.cpp
 * @brief Must NOT compile: a CheckedConfig built from a configuration that Validate() rejects fails
 * the build, naming the error (handbook KB-CPP20-013). The empty `Config{}` declares no motor.
 *
 * EXPECT: ConfigIsInvalid
 */
#include "tmc5160_types.hpp"

int main() {
  constexpr tmc5160::CheckedConfig kBoard{tmc5160::Config{}};
  return static_cast<int>(kBoard.Get().mode);
}
