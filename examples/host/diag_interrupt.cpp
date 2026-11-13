/**
 * @file diag_interrupt.cpp
 * @brief DIAG0 as an error output: read the pin, then `Service()` to learn what happened and clear
 * it.
 *
 * In STEP/DIR mode the DIAG pins carry the events; the application reads them with `Diag0Active()`
 * and calls `Service()` when one is active.
 */
#include <cstdio>

#include "host_tmc5160_port.hpp"

int main() {
  HostSpiPort port;
  tmc5160::Config config = ExampleConfig();
  config.mode = tmc5160::Mode::StepDir;
  config.diag.diag0_error = true;
  port.wire.ChipAt().k.sd_mode = true;
  tmc5160::TMC5160<HostSpiPort> motor(port, config);
  EXAMPLE_CHECK(motor.EnsureInitialized());
  EXAMPLE_CHECK(motor.Enable());

  port.wire.ChipAt()
      .InjectShort(); // a short to ground: the chip shuts the driver down and raises DIAG0
  const auto pin = motor.Diag0Active();
  EXAMPLE_CHECK(pin);
  const auto events = motor.Service();
  EXAMPLE_CHECK(events);
  std::printf("DIAG0 %s, driver error %s\n", pin.value ? "active" : "idle",
              events.value.global.drv_err ? "yes" : "no");
  port.wire.ChipAt().ClearShort();
  EXAMPLE_CHECK(motor.ClearFaults());
  return (pin.value && events.value.global.drv_err) ? 0 : 1;
}
