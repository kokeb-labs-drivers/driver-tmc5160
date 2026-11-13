/**
 * @file ramp_profile.cpp
 * @brief A six-point ramp from physical units: velocity in steps per second, acceleration in steps
 * per second squared.
 */
#include <cstdio>

#include "host_tmc5160_port.hpp"

int main() {
  HostSpiPort port;
  tmc5160::Config config = ExampleConfig();
  const uint32_t fclk = config.clock.hz;
  // 20 kHz cruise, 40 kHz/s^2 up, 60 kHz/s^2 down: registers from the datasheet formulas
  // (section 12.1).
  const auto vmax = tmc5160::units::VelocityRegFromHz(20'000, fclk);
  const auto amax = tmc5160::units::AccelRegFromHzPerS(40'000, fclk);
  const auto dmax = tmc5160::units::AccelRegFromHzPerS(60'000, fclk);
  EXAMPLE_CHECK(vmax);
  EXAMPLE_CHECK(amax);
  EXAMPLE_CHECK(dmax);
  config.ramp = tmc5160::RampProfile{.vstart = 0,
                                     .a1 = amax.value,
                                     .v1 = 0,
                                     .amax = amax.value,
                                     .vmax = vmax.value,
                                     .dmax = dmax.value,
                                     .d1 = dmax.value,
                                     .vstop = 10,
                                     .zero_wait = 0};
  tmc5160::TMC5160<HostSpiPort> motor(port, config);
  EXAMPLE_CHECK(motor.EnsureInitialized());
  EXAMPLE_CHECK(motor.Enable());
  EXAMPLE_CHECK(motor.MoveTo(40'000));
  for (int i = 0; i < 8; ++i) {
    port.DelayUs(500'000);
    const auto v = motor.ReadVelocity();
    EXAMPLE_CHECK(v);
    std::printf("t=%.1f s  velocity register %d\n", 0.5 * (i + 1), static_cast<int>(v.value));
  }
  const auto done = motor.PositionReached();
  EXAMPLE_CHECK(done);
  return done.value ? 0 : 1;
}
