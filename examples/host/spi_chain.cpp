/**
 * @file spi_chain.cpp
 * @brief Three TMC5160 in one SPI daisy chain: one driver object per chip, each knows its place.
 *
 * Index 0 is the chip whose SDI is on MOSI. Every transfer carries 5 bytes per chip; the other
 * chips see a read of GCONF, which changes nothing (page 25).
 */
#include <cstdio>

#include "host_tmc5160_port.hpp"

int main() {
  HostSpiPort port;
  port.wire.count = 3;
  tmc5160::Config config = ExampleConfig();
  config.spi.chain_length = 3;

  tmc5160::Config c0 = config;
  c0.spi.chain_index = 0;
  tmc5160::Config c1 = config;
  c1.spi.chain_index = 1;
  tmc5160::Config c2 = config;
  c2.spi.chain_index = 2;
  tmc5160::TMC5160<HostSpiPort> motor0(port, c0);
  tmc5160::TMC5160<HostSpiPort> motor1(port, c1);
  tmc5160::TMC5160<HostSpiPort> motor2(port, c2);
  EXAMPLE_CHECK(motor0.EnsureInitialized());
  EXAMPLE_CHECK(motor1.EnsureInitialized());
  EXAMPLE_CHECK(motor2.EnsureInitialized());
  EXAMPLE_CHECK(motor1.Enable());
  EXAMPLE_CHECK(motor1.MoveTo(3'000));
  port.DelayUs(1'000'000);
  const auto p1 = motor1.ReadPosition();
  const auto p0 = motor0.ReadPosition();
  EXAMPLE_CHECK(p0);
  EXAMPLE_CHECK(p1);
  std::printf("chip 0 at %d, chip 1 at %d\n", static_cast<int>(p0.value),
              static_cast<int>(p1.value));
  return ((p0.value == 0) && (p1.value == 3'000)) ? 0 : 1;
}
