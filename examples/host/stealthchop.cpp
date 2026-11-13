/**
 * @file stealthchop.cpp
 * @brief StealthChop at low speed, SpreadCycle above a velocity threshold, with the automatic
 * tuning wait.
 *
 * `SetStealthChop()` keeps IHOLD equal to IRUN until the first automatic tuning (AT#1) is over,
 * then restores the hold current (page 58). TPWMTHRS makes the chip switch to SpreadCycle above the
 * threshold.
 */
#include <cstdio>

#include "host_tmc5160_port.hpp"

int main() {
  HostSpiPort port;
  tmc5160::TMC5160<HostSpiPort> motor(port, ExampleConfig());
  EXAMPLE_CHECK(motor.EnsureInitialized());
  EXAMPLE_CHECK(motor.Enable());

  tmc5160::StealthChop stealth;
  stealth.enable = true;
  EXAMPLE_CHECK(
      motor.SetStealthChop(stealth)); // waits the AT#1 time at standstill, then restores IHOLD
  EXAMPLE_CHECK(motor.SetThresholds(tmc5160::Thresholds{.pwm = 500}));

  const auto state = motor.ReadStealthChopState();
  EXAMPLE_CHECK(state);
  std::printf("tuning wait done, plan: run %u mA, hold %u mA\n",
              static_cast<unsigned>(motor.GetCurrentPlan().run_ma),
              static_cast<unsigned>(motor.GetCurrentPlan().hold_ma));
  EXAMPLE_CHECK(motor.MoveTo(10'000));
  port.DelayUs(2'000'000);
  return 0;
}
