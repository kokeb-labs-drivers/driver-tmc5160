/**
 * @file homing_switch.cpp
 * @brief Homing on a left reference switch: the driver gives the pieces, the application owns the
 * procedure.
 *
 * Run towards the switch, let the chip stop on it (SW_MODE), back off, then declare the position
 * zero. Homing is deliberately not a driver call: the right speed, back-off distance and sequence
 * depend on the machine.
 */
#include <cstdio>

#include "host_tmc5160_port.hpp"

int main() {
  HostSpiPort port;
  tmc5160::Config config = ExampleConfig();
  config.switches.stop_left = true;
  config.switches.latch_left_active = true;
  tmc5160::TMC5160<HostSpiPort> motor(port, config);
  EXAMPLE_CHECK(motor.EnsureInitialized());
  EXAMPLE_CHECK(motor.Enable());

  EXAMPLE_CHECK(motor.RunVelocity(-30'000)); // towards the switch
  port.DelayUs(500'000);
  port.wire.ChipAt().SetReferenceSwitches(true, false); // the model closes the switch
  port.DelayUs(100'000);
  const auto ramp = motor.ReadRampStatus();
  EXAMPLE_CHECK(ramp);
  std::printf("left stop event: %s\n", ramp.value.event_stop_left ? "yes" : "no");

  EXAMPLE_CHECK(motor.Hold()); // clears the stop event
  port.wire.ChipAt().SetReferenceSwitches(false, false);
  EXAMPLE_CHECK(motor.SetHomePosition(0)); // needs standstill
  EXAMPLE_CHECK(motor.MoveTo(5'000));      // back off
  port.DelayUs(1'000'000);
  const auto position = motor.ReadPosition();
  EXAMPLE_CHECK(position);
  std::printf("position %d\n", static_cast<int>(position.value));
  return (ramp.value.event_stop_left && (position.value == 5'000)) ? 0 : 1;
}
