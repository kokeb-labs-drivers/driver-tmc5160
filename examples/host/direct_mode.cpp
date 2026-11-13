/**
 * @file direct_mode.cpp
 * @brief Direct mode: the coil currents are set from XTARGET, without the ramp generator.
 *
 * Direct mode bypasses the current plan's use of the ramp; it needs `Ack::DirectMode` in the
 * configuration and is limited to the coil range of the datasheet (page 110).
 */
#include <array>
#include <cstddef>
#include <cstdio>

#include "host_tmc5160_port.hpp"

int main() {
  HostSpiPort port;
  tmc5160::Config config = ExampleConfig();
  config.ack = config.ack.With(tmc5160::Ack::DirectMode);
  tmc5160::TMC5160<HostSpiPort> motor(port, config);
  EXAMPLE_CHECK(motor.EnsureInitialized());
  EXAMPLE_CHECK(motor.Enable());
  EXAMPLE_CHECK(motor.EnterDirectMode());
  for (std::size_t angle = 0; angle < 4U;
       ++angle) { // a hand-made quarter turn in four coil-current steps
    static constexpr std::array<int, 4> kA{200, 0, -200, 0};
    static constexpr std::array<int, 4> kB{0, 200, 0, -200};
    EXAMPLE_CHECK(motor.SetDirectCurrents(kA[angle], kB[angle]));
    port.DelayUs(10'000);
  }
  EXAMPLE_CHECK(motor.ExitDirectMode()); // the motor does not jump: XTARGET is set to XACTUAL
  return 0;
}
