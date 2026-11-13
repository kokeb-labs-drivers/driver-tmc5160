/**
 * @file test_lifecycle.cpp
 * @brief Start-up, strap and version checks, reset detection, faults and stuck lines (T01 to T10).
 */
#include "fixture.hpp"

using namespace tmc5160;
using namespace tmc5160_test;
namespace m = tmc5160_test::m;

TEST_CASE("T01 SPI start-up writes the configuration and leaves the driver off") {
  SpiRig rig;
  CHECK(!rig.drv.IsInitialized());
  CHECK_ERR(rig.drv.Enable(), Error::NotInitialized);
  CHECK_OK(rig.drv.EnsureInitialized());
  CHECK(rig.drv.IsInitialized());
  CHECK(!rig.drv.IsEnabled());
  CHECK_EQ(rig.ChipOf().Reg(m::CHOPCONF) & 0xFU, 0U); // TOFF 0 until Enable()
  CHECK_EQ(rig.ChipOf().Reg(m::GSTAT) & 7U, 0U);      // cleared by start-up
  CHECK_EQ(rig.ChipOf().forbidden_writes, 0U);
  CHECK_EQ(rig.ChipOf().reserved_bit_writes, 0U);
  CHECK_EQ(rig.ChipOf().wo_reads, 0U);
  CHECK_EQ(rig.drv.GetBeginReport().version, 0x30U);
  CHECK_OK(rig.drv.EnsureInitialized()); // second call: no traffic
}

TEST_CASE("T01b UART start-up and the same invariants") {
  UartRig rig;
  CHECK_OK(rig.drv.EnsureInitialized());
  CHECK_EQ(rig.ChipOf().forbidden_writes, 0U);
  CHECK_EQ(rig.ChipOf().reserved_bit_writes, 0U);
  CHECK_EQ(rig.ChipOf().wo_reads, 0U);
  CHECK_EQ(rig.ChipOf().crc_errors, 0U);
}

TEST_CASE("T02 Enable and Disable drive DRV_ENN first and TOFF") {
  SpiRig rig;
  REQUIRE(rig.drv.EnsureInitialized().ok());
  CHECK_OK(rig.drv.Enable());
  CHECK(rig.drv.IsEnabled());
  CHECK(rig.ChipOf().DriverOn());
  CHECK_EQ(rig.ChipOf().Reg(m::CHOPCONF) & 0xFU, 3U);
  CHECK_OK(rig.drv.Disable());
  CHECK(!rig.ChipOf().DriverOn());
  CHECK(!rig.ChipOf().DrvEnnActive());
}

TEST_CASE("T03 a wrong IOIN.VERSION is DeviceIdMismatch") {
  SpiRig rig;
  rig.ChipOf().k.version_byte = 0x11;
  CHECK_ERR(rig.drv.EnsureInitialized(), Error::DeviceIdMismatch);
  CHECK(!rig.drv.IsInitialized());
}

TEST_CASE("T04 the SD_MODE strap must match the configuration") {
  SpiRig rig;
  rig.ChipOf().k.sd_mode = true;
  CHECK_ERR(rig.drv.EnsureInitialized(), Error::ModeMismatch);
  Config c = BaseConfig();
  c.mode = Mode::StepDir;
  SpiRig step(c);
  CHECK_OK(step.drv.EnsureInitialized());
  CHECK_ERR(step.drv.MoveTo(100), Error::Unsupported);
  CHECK_ERR(step.drv.ReadPosition(), Error::Unsupported);
  CHECK_OK(step.drv.Enable());
}

TEST_CASE("T05 a stuck-low or stuck-high line is not a chip") {
  for (uint8_t level = 1; level <= 2; ++level) {
    SpiRig rig;
    rig.WireOf().faults.stuck = level;
    CHECK_ERR(rig.drv.EnsureInitialized(), Error::DeviceIdMismatch);
    UartRig uart;
    uart.WireOf().faults.stuck = level;
    const Error e = uart.drv.EnsureInitialized().error;
    CHECK(e != Error::Ok);
  }
}

TEST_CASE("T06 bus failures propagate and leave the driver uninitialised") {
  for (int at = 0; at < 12; ++at) {
    SpiRig rig;
    rig.WireOf().faults.fail_at = at;
    const auto r = rig.drv.EnsureInitialized();
    CHECK_ERR(r, Error::Bus);
    CHECK(!rig.drv.IsInitialized());
  }
  SpiRig down;
  down.WireOf().faults.bus_up_ok = false;
  CHECK_ERR(down.drv.EnsureInitialized(), Error::Bus);
}

TEST_CASE("T07 a reset behind the driver's back is reported once and every register is rewritten") {
  SpiRig rig;
  REQUIRE(rig.Up());
  const uint32_t current = rig.ChipOf().Reg(m::IHOLD_IRUN);
  rig.ChipOf().PowerOn(); // brown-out: write-only registers are lost
  const auto r = rig.drv.ReadPosition();
  CHECK_ERR(r, Error::UnexpectedReset); // the status byte shows the reset flag
  CHECK(!rig.drv.IsInitialized());
  CHECK_ERR(rig.drv.MoveTo(10), Error::NotInitialized);
  CHECK_OK(rig.drv.EnsureInitialized());
  CHECK_EQ(rig.ChipOf().Reg(m::IHOLD_IRUN), current); // written again, as before the brown-out
  CHECK((rig.ChipOf().Reg(m::GLOBAL_SCALER) != 0U) || (rig.ChipOf().Reg(m::IHOLD_IRUN) != 0U));
}

TEST_CASE("T08 a reset seen through GSTAT on UART gives UnexpectedReset") {
  UartRig rig;
  REQUIRE(rig.Up());
  rig.ChipOf().PowerOn();
  CHECK_ERR(rig.drv.ReadGlobalStatus(), Error::UnexpectedReset);
  CHECK_ERR(rig.drv.Service(), Error::NotInitialized);
}

TEST_CASE("T08b clearing GSTAT never hides an unseen reset on UART") {
  UartRig rig;
  REQUIRE(rig.Up());
  rig.ChipOf().PowerOn();
  CHECK_ERR(rig.drv.ClearFaults(), Error::UnexpectedReset);
  UartRig other;
  REQUIRE(other.Up());
  other.ChipOf().PowerOn();
  CHECK_ERR(other.drv.ClearGlobalStatus(1), Error::UnexpectedReset);
}

TEST_CASE("T08c SetChopper after an unseen reset never switches the bridge on at reset currents") {
  UartRig rig;
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.Enable());
  rig.ChipOf().PowerOn(); // UART has no status byte: only GSTAT shows the reset
  const double before = rig.ChipOf().peak_coil_ma;
  CHECK_ERR(rig.drv.SetChopper(Chopper{}), Error::UnexpectedReset);
  CHECK(!rig.drv.IsInitialized());
  CHECK(rig.ChipOf().peak_coil_ma <= before);
}

TEST_CASE("T09 a driver error latches a fault until ClearFaults") {
  SpiRig rig;
  REQUIRE(rig.Up());
  rig.ChipOf().InjectShort();
  const auto g = rig.drv.ReadGlobalStatus();
  CHECK(g.ok() && g.value.drv_err);
  CHECK(rig.drv.HasFault());
  CHECK_ERR(rig.drv.Enable(), Error::DeviceFault);
  CHECK_ERR(rig.drv.MoveTo(5), Error::DeviceFault);
  CHECK_ERR(rig.drv.ClearFaults(), Error::DeviceFault); // the short is still there
  rig.ChipOf().ClearShort();
  CHECK_OK(rig.drv.ClearFaults());
  CHECK(!rig.drv.HasFault());
  CHECK(!rig.drv.IsEnabled()); // a failed attempt left the driver off: it stays off until Enable()
  CHECK_OK(rig.drv.Enable());
}

TEST_CASE("T10 ClearFaults refuses while the chip is still too hot") {
  SpiRig rig;
  REQUIRE(rig.Up());
  rig.ChipOf().InjectOvertemp(true);
  CHECK_ERR(rig.drv.ClearFaults(), Error::Refused);
  rig.ChipOf().ClearTemperature();
  CHECK_OK(rig.drv.ClearFaults());
}

TEST_CASE("T11 an external SPI clock above the limit is refused before any traffic") {
  SpiRig rig;
  rig.WireOf().faults.sck_hz = 5'000'000;
  CHECK_ERR(rig.drv.EnsureInitialized(), Error::Unsupported);
  CHECK_EQ(rig.WireOf().transfers, 0);
}

TEST_CASE("T12 Deinitialize switches the driver off") {
  SpiRig rig;
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.Deinitialize());
  CHECK(!rig.ChipOf().DriverOn());
  CHECK_ERR(rig.drv.ReadPosition(), Error::NotInitialized);
  CHECK_OK(rig.drv.Deinitialize());
}

TEST_CASE("T13 begin_enabled leaves the driver on") {
  Config c = BaseConfig();
  c.begin_enabled = true;
  SpiRig rig(c);
  CHECK_OK(rig.drv.EnsureInitialized());
  CHECK(rig.drv.IsEnabled());
  CHECK(rig.ChipOf().DriverOn());
}

TEST_CASE("T14 Enable after an unseen brown-out refuses: the reset current scale could exceed the "
          "motor") {
  Config c = BaseConfig();
  c.board.rsense_mohm = 30; // full scale at reset: 239 mA per 1/32 step, above this 200 mA motor
  c.motor.rated_rms_ma = 200;
  c.current = Current{130, 60};
  SpiRig rig(c);
  REQUIRE(rig.drv.EnsureInitialized().ok());
  rig.ChipOf().PowerOn();
  CHECK_ERR(rig.drv.Enable(), Error::UnexpectedReset);
  CHECK(!rig.ChipOf().DriverOn());
  CHECK(rig.ChipOf().peak_coil_ma <= 200.0);
  CHECK(!rig.drv.IsEnabled());
}

TEST_CASE("T15 Enable refuses while GSTAT shows a driver error") {
  SpiRig rig;
  REQUIRE(rig.drv.EnsureInitialized().ok());
  rig.ChipOf().InjectShort();
  CHECK_ERR(rig.drv.Enable(), Error::DeviceFault);
  CHECK(!rig.ChipOf().DriverOn());
}
