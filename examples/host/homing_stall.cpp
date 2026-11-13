/**
 * @file homing_stall.cpp
 * @brief Sensorless homing against a hard stop with StallGuard2: run, stall, release, set zero.
 */
#include <cstdio>

#include "host_tmc5160_port.hpp"

int main() {
  HostSpiPort port;
  tmc5160::Config config = ExampleConfig();
  config.thresholds.cool = 200'000;
  config.stall.threshold = 2;
  config.switches.stop_on_stall = true;
  tmc5160::TMC5160<HostSpiPort> motor(port, config);
  EXAMPLE_CHECK(motor.EnsureInitialized());
  EXAMPLE_CHECK(motor.Enable());

  EXAMPLE_CHECK(motor.RunVelocity(-40'000));
  port.DelayUs(800'000);
  port.wire.ChipAt().InjectStall(true); // the carriage meets the hard stop
  port.DelayUs(20'000);
  port.wire.ChipAt().InjectStall(false);
  const auto velocity = motor.ReadVelocity();
  EXAMPLE_CHECK(velocity);
  EXAMPLE_CHECK(motor.ReleaseStall()); // hold, clear the event; the motor stays stopped
  EXAMPLE_CHECK(motor.SetHomePosition(0));
  std::printf("stopped at velocity %d, home set\n", static_cast<int>(velocity.value));
  return velocity.value == 0 ? 0 : 1;
}
