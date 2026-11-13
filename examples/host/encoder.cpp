/**
 * @file encoder.cpp
 * @brief An incremental encoder: scale from steps and counts per revolution, position read-out,
 * deviation warning.
 */
#include <cstdio>

#include "host_tmc5160_port.hpp"

int main() {
  HostSpiPort port;
  tmc5160::TMC5160<HostSpiPort> motor(port, ExampleConfig());
  EXAMPLE_CHECK(motor.EnsureInitialized());

  // 200 full steps x 256 microsteps per revolution against a 4000-count (1000-line) encoder:
  // factor 12.8.
  const auto scale = tmc5160::units::EncoderScaleFor(51'200, 4'000, true, false);
  EXAMPLE_CHECK(scale);
  tmc5160::Encoder encoder;
  encoder.declared = true;
  encoder.scale = scale.value;
  encoder.deviation = 400;
  EXAMPLE_CHECK(motor.SetEncoder(encoder));

  port.wire.ChipAt().AddEncoderCounts(1'000); // the model counts encoder edges
  const auto position = motor.ReadEncoderPosition();
  EXAMPLE_CHECK(position);
  std::printf("encoder position %d microsteps\n", static_cast<int>(position.value));
  return position.value == 12'800 ? 0 : 1;
}
