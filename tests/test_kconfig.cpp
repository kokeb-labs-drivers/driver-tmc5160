/**
 * @file test_kconfig.cpp
 * @brief KconfigDefaults(): the empty configuration without Kconfig, and menuconfig values when
 * they are set.
 *
 * Built twice: without CONFIG_ macros (KCONFIG_SET undefined) and with the values an sdkconfig.h
 * would define (KCONFIG_SET=1, see tests/CMakeLists.txt).
 */
#include "test_harness.hpp"
#include "tmc5160_kconfig.hpp"

#ifdef KCONFIG_SET
TEST_CASE("Kconfig values reach the Config") {
  constexpr tmc5160::Config kC = tmc5160::KconfigDefaults();
  CHECK_EQ(kC.motor.rated_rms_ma, 1500U);
  CHECK_EQ(kC.board.rsense_mohm, 75U);
  CHECK_EQ(kC.board.supply_mv, 36'000U);
  CHECK_EQ(kC.chopper.mres, tmc5160::Microsteps::M16);
  CHECK(kC.stealth.enable);
  CHECK_EQ(kC.uart.baud, 250'000U);
  CHECK_EQ(kC.uart.node, 2U);
  CHECK_EQ(kC.spi.chain_length, 3U);
  CHECK_EQ(kC.spi.chain_index, 1U);
  CHECK_EQ(kC.current.run_ma, 1000U);
  CHECK_EQ(kC.current.hold_ma, 400U);
  CHECK(kC.verify == tmc5160::Verify::ReadBack);
  CHECK_EQ(tmc5160::Validate(kC), tmc5160::Error::Ok);
}
#else
TEST_CASE("Without Kconfig, KconfigDefaults() equals Config{}, which is invalid until a motor is "
          "declared") {
  constexpr tmc5160::Config kC = tmc5160::KconfigDefaults();
  constexpr tmc5160::Config kD{};
  CHECK_EQ(kC.motor.rated_rms_ma, kD.motor.rated_rms_ma);
  CHECK_EQ(kC.board.rsense_mohm, kD.board.rsense_mohm);
  CHECK_EQ(kC.chopper.toff, kD.chopper.toff);
  CHECK_EQ(tmc5160::Validate(kC), tmc5160::Error::InvalidArgument);
}
#endif
