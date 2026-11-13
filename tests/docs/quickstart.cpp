/**
 * @file quickstart.cpp
 * @brief The Quickstart page's example, compiled by the host build (never run).
 */
#include "my_port.hpp"

// --8<-- [start:quickstart]
#include "tmc5160.hpp"

int spin_one_revolution(MyPort& port) {
  // 1. Declare the motor and the board. The driver derives every current register from these two.
  const tmc5160::Config config =
      tmc5160::MakeConfig(tmc5160::Motor{.rated_rms_ma = 1000, .coil_resistance_mohm = 6000},
                          tmc5160::Board{.rsense_mohm = 100, .supply_mv = 24'000});
  tmc5160::TMC5160<MyPort> motor(port, config); // no I/O yet

  if (auto r = motor.EnsureInitialized();
      !r) {                           // 2. bus, strap, VERSION, every register, driver off
    return static_cast<int>(r.error); //    Bus, Timeout, DeviceIdMismatch, InvalidArgument ...
  }
  if (auto r = motor.Enable(); !r) { // 3. DRV_ENN active, TOFF written
    return static_cast<int>(r.error);
  }
  if (auto r = motor.MoveTo(51'200); !r) { // 4. one revolution at 256 microsteps, 200 full steps
    return static_cast<int>(r.error);
  }
  return 0; // poll PositionReached(); Disable() when done
}
// --8<-- [end:quickstart]
