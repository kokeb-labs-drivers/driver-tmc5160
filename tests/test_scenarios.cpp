/**
 * @file test_scenarios.cpp
 * @brief The same behaviour on both buses: one template per scenario, run on SPI and on UART (T170
 * to T190).
 */
#include "fixture.hpp"

using namespace tmc5160;
using namespace tmc5160_test;
namespace m = tmc5160_test::m;

namespace {
Config Quick() {
  Config c = BaseConfig();
  c.ramp.amax = 20'000;
  c.ramp.dmax = 20'000;
  c.ramp.a1 = 20'000;
  c.ramp.d1 = 20'000;
  c.ramp.vmax = 100'000;
  c.ramp.v1 = 50'000;
  return c;
}

template <typename R>
void MoveScenario() {
  R rig(Quick());
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.MoveTo(10'000));
  rig.Run(2'000'000);
  CHECK_EQ(rig.ChipOf().Xactual(), 10'000);
  CHECK_EQ(rig.drv.ReadPosition().value, 10'000);
  CHECK_OK(rig.drv.MoveBy(-2000));
  rig.Run(2'000'000);
  CHECK_EQ(rig.ChipOf().Xactual(), 8'000);
  CHECK_EQ(rig.ChipOf().forbidden_writes, 0U);
  CHECK_EQ(rig.ChipOf().reserved_bit_writes, 0U);
  CHECK_EQ(rig.ChipOf().wo_reads, 0U);
}

template <typename R>
void StatusScenario() {
  R rig(Quick());
  REQUIRE(rig.Up());
  CHECK(rig.drv.ReadGlobalStatus().ok());
  CHECK(rig.drv.ReadDriverStatus().ok());
  CHECK(rig.drv.ReadRampStatus().ok());
  CHECK(rig.drv.ReadInputs().ok());
  CHECK(rig.drv.ReadStepTime().ok());
  CHECK(rig.drv.ReadMicrostepCounter().ok());
  const auto snap = rig.drv.ReadSnapshot();
  CHECK(snap.ok());
  const auto ev = rig.drv.Service();
  CHECK(ev.ok());
  CHECK_OK(rig.drv.ClearGlobalStatus(7));
  const uint8_t status = rig.drv.GetLastStatusByte();
  const int sent = rig.WireOf().transfers;
  CHECK_ERR(rig.drv.ClearGlobalStatus(0), Error::InvalidArgument);
  CHECK_ERR(rig.drv.ClearGlobalStatus(8), Error::InvalidArgument);
  CHECK_EQ(rig.WireOf().transfers, sent); // InvalidArgument sends nothing
  CHECK_EQ(rig.drv.GetLastStatusByte(), status);
}

template <typename R>
void SetterScenario() {
  R rig(Quick());
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.SetCurrent(500, 200, Verify::ReadBack));
  CHECK_OK(rig.drv.SetStandstill(
      Standstill{.hold_delay = 4, .power_down_delay = 20, .fast_standstill = true}));
  CHECK_OK(rig.drv.SetRamp(Quick().ramp, Verify::ReadBack));
  CHECK_OK(rig.drv.SetChopper(Chopper{}, Verify::ReadBack));
  CHECK_OK(rig.drv.SetThresholds(Thresholds{.pwm = 400, .cool = 300, .high = 100}));
  CHECK_OK(rig.drv.SetCoolStep(CoolStep{.semin = 2, .semax = 1}));
  CHECK_OK(rig.drv.SetFlags(Flags{.multistep_filt = true}, Verify::ReadBack));
  CHECK_OK(rig.drv.SetSwitches(Switches{.stop_left = true, .left_active_low = true}));
  CHECK_OK(rig.drv.SetStopOnStall(false));
  CHECK_OK(rig.drv.SetPositionCompare(77));
  CHECK_OK(rig.drv.SetEncoder(Encoder{}));
  CHECK_OK(rig.drv.Disable(Verify::ReadBack));
  CHECK_OK(rig.drv.SetBridge(Bridge{.declared = true, .bbm_time = 1}, AckSet{}, Verify::ReadBack));
  CHECK_OK(rig.drv.SetProtection(Protection{.declared = true}, AckSet{}, Verify::ReadBack));
  CHECK_OK(rig.drv.Enable(Verify::ReadBack));
  CHECK_EQ(rig.ChipOf().forbidden_writes, 0U);
  CHECK_EQ(rig.ChipOf().reserved_bit_writes, 0U);
  CHECK_EQ(rig.ChipOf().wo_reads, 0U);
}

template <typename R>
void ErrorPathScenario() {
  // Fail each transfer of a representative call in turn: every failure propagates and the link
  // recovers.
  const auto each = [](auto call) {
    int used = 0;
    {
      R rig(Quick());
      REQUIRE(rig.Up());
      const int before = rig.WireOf().transfers;
      CHECK(call(rig).ok());
      used = rig.WireOf().transfers - before;
    }
    CHECK(used > 0);
    for (int at = 0; at < used; ++at) {
      R rig(Quick());
      REQUIRE(rig.Up());
      rig.WireOf().faults.fail_from =
          rig.WireOf().transfers + at; // from here on: a UART read retry cannot hide it
      CHECK(!call(rig).ok());
      rig.WireOf().faults.fail_from = -1;
      CHECK(rig.drv.ReadPosition().ok());
    }
  };
  each([](R& r) { return r.drv.SetCurrent(500, 200, Verify::ReadBack); });
  each([](R& r) { return r.drv.MoveTo(1234); });
  each([](R& r) { return r.drv.SetFlags(Flags{.invert_direction = true}, Verify::ReadBack); });
  each([](R& r) { return r.drv.Stop(StopKind::Immediate); });
  each([](R& r) { return r.drv.SetThresholds(Thresholds{.pwm = 400, .cool = 300, .high = 100}); });
  R rig(Quick());
  REQUIRE(rig.Up());
  rig.WireOf().faults.fail_from = rig.WireOf().transfers;
  CHECK_ERR(rig.drv.ReadPosition(), Error::Bus);
  CHECK_ERR(rig.drv.ReadSnapshot(), Error::Bus);
  CHECK_ERR(rig.drv.Service(), Error::Bus);
  CHECK_ERR(rig.drv.Enable(), Error::Bus);
  CHECK_ERR(rig.drv.Disable(), Error::Bus);
  CHECK_ERR(rig.drv.ClearFaults(), Error::Bus);
  CHECK_ERR(rig.drv.SetStealthChop(StealthChop{}), Error::Bus);
  CHECK_ERR(rig.drv.RunVelocity(100), Error::Bus);
  CHECK_ERR(rig.drv.Stop(StopKind::HoldTarget), Error::Bus);
  CHECK_ERR(rig.drv.Stop(StopKind::Ramp), Error::Bus);
  CHECK_ERR(rig.drv.Hold(), Error::Bus);
  CHECK_ERR(rig.drv.SetHomePosition(0), Error::Bus);
  CHECK_ERR(rig.drv.ReleaseStall(), Error::Bus);
  CHECK_ERR(rig.drv.ReadGlobalStatus(), Error::Bus);
  CHECK_ERR(rig.drv.ReadRegister(m::GCONF), Error::Bus);
  CHECK_ERR(rig.drv.SetMicrostepTable(MicrostepTable{}), Error::Bus);
}

template <typename R>
void StealthScenario() {
  Config c = Quick();
  R rig(c);
  REQUIRE(rig.Up());
  StealthChop s;
  s.enable = true;
  CHECK_OK(rig.drv.SetStealthChop(s, TuningPolicy::HoldAtRunForAt1, Verify::ReadBack));
  CHECK(rig.ChipOf().at1_done);
  s.enable = false;
  CHECK_OK(rig.drv.SetStealthChop(s));
  s.enable = true;
  CHECK_OK(rig.drv.SetStealthChop(s, TuningPolicy::HoldAtRunForAt1));
}

template <typename R>
void FaultScenario() {
  R rig(Quick());
  REQUIRE(rig.Up());
  rig.ChipOf().InjectChargePumpUv();
  const auto g = rig.drv.ReadGlobalStatus();
  CHECK(g.ok() && g.value.uv_cp);
  const auto ev = rig.drv.Service();
  CHECK(ev.ok() && ev.value.global.uv_cp);
  CHECK_EQ(rig.ChipOf().Reg(m::GSTAT) & 4U, 0U); // Service cleared it
}

template <typename R>
void RegisterScenario() {
  R rig(Quick());
  REQUIRE(rig.Up());
  CHECK_ERR(rig.drv.ReadRegister(m::VMAX), Error::Refused);
  const auto id = rig.drv.Identify();
  CHECK(id.ok() && (id.value == 0x30U));
  CHECK(rig.drv.ReadOtp().ok());
  CHECK(rig.drv.ReadClockTrim().ok());
  CHECK(rig.drv.ReadOffsets().ok());
  CHECK(rig.drv.ReadEncoderLatch().ok());
  CHECK(rig.drv.ReadLatchedPosition().ok());
  CHECK(rig.drv.ReadEncoderStatus().ok());
  CHECK(rig.drv.ReadStealthChopState().ok());
  CHECK(rig.drv.ReadStallGuard().ok());
  CHECK(rig.drv.ReadMicrostepCurrents().ok());
  CHECK_EQ(rig.drv.GetCurrentPlan().global_scaler >= 129U, true);
}
} // namespace

TEST_CASE("T170 move scenario, SPI") {
  MoveScenario<SpiRig>();
}
TEST_CASE("T171 move scenario, UART") {
  MoveScenario<UartRig>();
}
TEST_CASE("T172 status scenario, SPI") {
  StatusScenario<SpiRig>();
}
TEST_CASE("T173 status scenario, UART") {
  StatusScenario<UartRig>();
}
TEST_CASE("T174 setter scenario, SPI") {
  SetterScenario<SpiRig>();
}
TEST_CASE("T175 setter scenario, UART") {
  SetterScenario<UartRig>();
}
TEST_CASE("T176 error paths, SPI") {
  ErrorPathScenario<SpiRig>();
}
TEST_CASE("T177 error paths, UART") {
  ErrorPathScenario<UartRig>();
}
TEST_CASE("T178 StealthChop scenario, SPI") {
  StealthScenario<SpiRig>();
}
TEST_CASE("T179 StealthChop scenario, UART") {
  StealthScenario<UartRig>();
}
TEST_CASE("T180 fault scenario, SPI") {
  FaultScenario<SpiRig>();
}
TEST_CASE("T181 fault scenario, UART") {
  FaultScenario<UartRig>();
}
TEST_CASE("T182 register scenario, SPI") {
  RegisterScenario<SpiRig>();
}
TEST_CASE("T183 register scenario, UART") {
  RegisterScenario<UartRig>();
}

TEST_CASE("T184 the minimal ports (no GPIO, no log) run the whole life cycle") {
  Rig<MinimalSpiPort> spi(Quick());
  REQUIRE(spi.drv.EnsureInitialized().ok());
  CHECK_OK(spi.drv.Enable());
  CHECK_OK(spi.drv.MoveTo(100));
  CHECK_OK(spi.drv.Disable());
  Rig<MinimalUartPort> uart(Quick());
  REQUIRE(uart.drv.EnsureInitialized().ok());
  CHECK_OK(uart.drv.Enable());
  CHECK_OK(uart.drv.Deinitialize());
}

TEST_CASE("T185 GPIO failures propagate from Enable and Disable") {
  SpiRig rig(Quick());
  REQUIRE(rig.drv.EnsureInitialized().ok());
  rig.WireOf().gpio_ok = false;
  CHECK_ERR(rig.drv.Enable(), Error::Bus);
  CHECK_ERR(rig.drv.Disable(), Error::Bus);
  CHECK_ERR(rig.drv.Diag0Active(), Error::Bus);
  CHECK(!rig.drv.IsEnabled());
}
