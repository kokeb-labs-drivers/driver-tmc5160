/**
 * @file quickstart.cpp
 * @brief The TMC5160 driver on your PC: declare a motor, start the driver, move 20000 microsteps.
 *
 * No hardware and no SDK: the host port connects the real driver to the register model the host
 * tests use. The same calls run on an MCU with that MCU's port (examples/esp32/). Exit code 0 when
 * every call succeeded.
 */
#include <cstdio>

#include "host_tmc5160_port.hpp"

int main() {
  HostSpiPort port;
  tmc5160::TMC5160<HostSpiPort> motor(port, ExampleConfig()); // no I/O yet

  std::printf("TMC5160 driver %s on the host\n", tmc5160::GetDriverVersion());
  EXAMPLE_CHECK(
      motor
          .EnsureInitialized()); // bus, strap and version check, every register written, driver off
  EXAMPLE_CHECK(motor.Enable()); // DRV_ENN active, TOFF written
  EXAMPLE_CHECK(motor.MoveTo(20'000));
  port.DelayUs(2'000'000); // a real application polls PositionReached()
  const auto position = motor.ReadPosition();
  EXAMPLE_CHECK(position);
  std::printf("position %d\n", static_cast<int>(position.value));
  EXAMPLE_CHECK(motor.Disable());
  return position.value == 20'000 ? 0 : 1;
}
