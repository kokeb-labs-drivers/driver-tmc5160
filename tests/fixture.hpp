/**
 * @file fixture.hpp
 * @brief Shared helpers of the host tests: a valid configuration and a rig of port, chip and
 * driver.
 */
#ifndef TMC5160_TESTS_FIXTURE_HPP
#define TMC5160_TESTS_FIXTURE_HPP

#include <cstdint>

#include "mock_tmc5160_bus.hpp"
#include "test_harness.hpp"
#include "tmc5160.hpp"

namespace tmc5160_test {

/// A declared 1 A motor on 100 mOhm sense resistors and 24 V, in the motion-controller mode.
inline tmc5160::Config BaseConfig() {
  tmc5160::Config c =
      tmc5160::MakeConfig(tmc5160::Motor{.rated_rms_ma = 1000, .coil_resistance_mohm = 6000},
                          tmc5160::Board{.rsense_mohm = 100, .supply_mv = 24'000});
  return c;
}

/// A fast ramp with the stall stop on: the set-up of the stall and stop tests.
inline tmc5160::Config StallConfig() {
  tmc5160::Config c = BaseConfig();
  c.ramp.amax = 20'000;
  c.ramp.dmax = 20'000;
  c.ramp.a1 = 20'000;
  c.ramp.d1 = 20'000;
  c.ramp.vmax = 100'000;
  c.ramp.v1 = 50'000;
  c.switches.stop_on_stall = true;
  c.switches.soft_stop = false;
  c.thresholds.cool = 200'000;
  return c;
}

/// Port, chip model and driver together.
template <typename Port>
struct Rig {
  Port port;
  tmc5160::TMC5160<Port> drv;

  explicit Rig(const tmc5160::Config& config = BaseConfig()) : drv(port, config) {
    port.board.ChipAt().k.rsense_mohm = config.board.rsense_mohm;
    port.board.ChipAt().k.sd_mode = config.mode == tmc5160::Mode::StepDir;
  }
  Chip& ChipOf(std::size_t i = 0) {
    return port.board.ChipAt(i);
  }
  Wire& WireOf() {
    return port.board;
  }
  /// Let the model run for `us` microseconds.
  void Run(uint32_t us) {
    port.board.Delay(us);
  }
  /// Start the driver and enable it.
  bool Up() {
    return drv.EnsureInitialized().ok() && drv.Enable().ok();
  }
};

using SpiRig = Rig<FullSpiPort>;
using UartRig = Rig<FullUartPort>;

} // namespace tmc5160_test

#define CHECK_OK(expr) CHECK((expr).ok())
#define CHECK_ERR(expr, code) CHECK_EQ(static_cast<int>((expr).error), static_cast<int>(code))

#endif // TMC5160_TESTS_FIXTURE_HPP
