/**
 * @file configuration.cpp
 * @brief The Configuration page's examples, compiled by the host build (never run).
 */
#include "my_port.hpp"
#include "tmc5160.hpp"
#include "tmc5160_kconfig.hpp"

// --8<-- [start:custom]
tmc5160::Config StealthConfig() {
  tmc5160::Config c =
      tmc5160::MakeConfig(tmc5160::Motor{.rated_rms_ma = 1500, .coil_resistance_mohm = 2000},
                          tmc5160::Board{.rsense_mohm = 75, .supply_mv = 24'000});
  c.current.run_ma = 1000; // the run current in mA rms: never above the declared motor
  c.current.hold_ma = 300; // the hold current
  c.chopper.mres = tmc5160::Microsteps::M16; // 16 microsteps per full step
  c.stealth.enable = true;
  return c; // tmc5160::Validate(c) checks it without the chip
}

tmc5160::Error StartStealth(MyPort& port) {
  static tmc5160::TMC5160<MyPort> motor(port, StealthConfig()); // stored, no I/O
  return motor.EnsureInitialized()
      .error; // InvalidArgument if a value is out of range or above the motor
}
// --8<-- [end:custom]

// --8<-- [start:kconfig]
tmc5160::Error StartFromMenuconfig(MyPort& port) {
  static tmc5160::TMC5160<MyPort> motor(port, tmc5160::KconfigDefaults());
  return motor.EnsureInitialized().error;
}
// --8<-- [end:kconfig]
