/**
 * @file stallguard.cpp
 * @brief StallGuard2 and CoolStep: thresholds, stop on stall, and the explicit release.
 *
 * The motor never restarts by itself after a stall stop: `ReleaseStall()` clears the event (page
 * 90).
 */
#include <cstdio>

#include "host_tmc5160_port.hpp"

int main() {
  HostSpiPort port;
  tmc5160::Config config = ExampleConfig();
  config.thresholds.cool = 200'000; // StallGuard2 is valid above this TSTEP threshold
  config.stall = tmc5160::StallGuard{.threshold = 4, .filter = true};
  config.cool = tmc5160::CoolStep{.semin = 5, .semax = 2, .seup = 1, .sedn = 1};
  config.switches.stop_on_stall = true;
  tmc5160::TMC5160<HostSpiPort> motor(port, config);
  EXAMPLE_CHECK(motor.EnsureInitialized());
  EXAMPLE_CHECK(motor.Enable());
  EXAMPLE_CHECK(motor.RunVelocity(60'000));
  port.DelayUs(1'000'000);

  port.wire.ChipAt().InjectStall(
      true); // the model's stall input: a real load would pull SG_RESULT to 0
  port.DelayUs(20'000);
  const auto status = motor.ReadRampStatus();
  EXAMPLE_CHECK(status);
  std::printf("stall stop event: %s\n", status.value.event_stop_stall ? "yes" : "no");

  port.wire.ChipAt().InjectStall(false);
  EXAMPLE_CHECK(motor.ReleaseStall());
  EXAMPLE_CHECK(motor.Stop(tmc5160::StopKind::Ramp));
  return status.value.event_stop_stall ? 0 : 1;
}
