/**
 * @file test_current.cpp
 * @brief Current limits, guards and acknowledgements, StealthChop tuning, chopper and power-stage
 * setters (T50 to T75).
 */
#include "fixture.hpp"

using namespace tmc5160;
using namespace tmc5160_test;
namespace m = tmc5160_test::m;

TEST_CASE(
    "T50 the coil current the chip computes never exceeds the declared motor, whatever is asked") {
  SpiRig rig;
  REQUIRE(rig.Up());
  const uint32_t asks[] = {1, 50, 100, 300, 500, 800, 950, 951, 952, 953, 1000, 1500, 2000, 5000};
  for (const uint32_t ask : asks) {
    const auto r = rig.drv.SetCurrent(ask, ask / 2);
    if (r.ok()) {
      CHECK(rig.ChipOf().RunMa() <= 1000.0);
      CHECK(rig.ChipOf().RunMa() <= (static_cast<double>(ask) + 0.5));
    } else {
      CHECK((ask > 952U) || (ask < 30U)); // above the ceiling, or below one current step
    }
  }
  CHECK(rig.ChipOf().peak_coil_ma <= 1000.0);
}

TEST_CASE("T51 SetCurrent refusals: zero, above the ceiling, hold above run") {
  SpiRig rig;
  REQUIRE(rig.Up());
  CHECK_ERR(rig.drv.SetCurrent(0, 0), Error::InvalidArgument);
  CHECK_ERR(rig.drv.SetCurrent(1200, 100), Error::LimitExceeded);
  CHECK_ERR(rig.drv.SetCurrent(400, 500), Error::Refused);
  const uint32_t before = rig.ChipOf().writes;
  CHECK_ERR(rig.drv.SetCurrent(5000, 5000), Error::LimitExceeded);
  CHECK_EQ(rig.ChipOf().writes, before); // a refusal sends nothing
}

TEST_CASE("T52 the hold-above-run acknowledgement lifts only its own guard") {
  Config c = BaseConfig();
  c.ack = c.ack.With(Ack::HoldAboveRun);
  SpiRig rig(c);
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.SetCurrent(300, 500));
  CHECK_ERR(rig.drv.SetCurrent(300, 1200), Error::LimitExceeded); // the ceiling stays
}

TEST_CASE("T53 a sense resistor too small for the request is LimitExceeded") {
  Config c = BaseConfig();
  c.board.rsense_mohm = 1000; // full scale 230 mA
  c.motor.rated_rms_ma = 2000;
  c.current = Current{200, 100};
  SpiRig rig(c);
  REQUIRE(rig.Up());
  CHECK_ERR(rig.drv.SetCurrent(500, 100), Error::LimitExceeded);
}

TEST_CASE("T54 GLOBALSCALER is 129 to 256 and a register value of 0 stands for 256") {
  SpiRig rig;
  REQUIRE(rig.Up());
  for (uint32_t ma = 100; ma <= 950; ma += 50) {
    const auto p = rig.drv.SetCurrent(ma, ma / 3);
    REQUIRE(p.ok());
    const uint32_t reg = rig.ChipOf().Reg(m::GLOBAL_SCALER);
    CHECK((reg == 0U) || ((reg >= 129U) && (reg <= 255U)));
    CHECK_EQ(p.value.global_scaler_reg, reg);
  }
}

TEST_CASE("T55 a high supply with a high short level needs the guard values (A11)") {
  Config c = BaseConfig();
  c.board.supply_mv = 24'000;
  c.board.supply_max_mv = 54'000;
  SpiRig rig(c);
  Protection p{.declared = true};
  p.s2g_level = 6;
  CHECK_ERR(rig.drv.SetProtection(p), Error::NotInitialized);
}

TEST_CASE("T56 switching a short protection off needs its acknowledgement") {
  SpiRig rig;
  REQUIRE(rig.drv.EnsureInitialized().ok());
  Protection p{.declared = true};
  p.disable_s2g = true;
  CHECK_ERR(rig.drv.SetProtection(p), Error::Refused);
  CHECK_OK(rig.drv.SetProtection(p, AckSet{}.With(Ack::ShortProtectionOff)));
  CHECK((rig.ChipOf().Reg(m::CHOPCONF) & (1U << 30)) != 0U); // diss2g
  CHECK(!rig.drv.GetConfig().ack.Has(
      Ack::ShortProtectionOff)); // the acknowledgement was for one call
}

TEST_CASE("T57 bridge and protection are written only with the driver off") {
  SpiRig rig;
  REQUIRE(rig.Up());
  Bridge b{.declared = true, .bbm_time = 2};
  CHECK_ERR(rig.drv.SetBridge(b), Error::Busy);
  CHECK_OK(rig.drv.Disable());
  CHECK_OK(rig.drv.SetBridge(b));
  CHECK_EQ(rig.ChipOf().Reg(m::DRV_CONF) & 0x1FU, 2U);
  b.ot_level = OtLevel::C120;
  CHECK_ERR(rig.drv.SetBridge(b), Error::Refused);
  CHECK_OK(rig.drv.SetBridge(b, AckSet{}.With(Ack::LowOtLevel)));
}

TEST_CASE("T58 StealthChop: IHOLD equals IRUN until the AT#1 wait is over, then the hold current "
          "returns") {
  Config c = BaseConfig();
  c.stealth.enable = true;
  SpiRig rig(c);
  REQUIRE(rig.drv.EnsureInitialized().ok());
  const uint32_t irun = (rig.ChipOf().Reg(m::IHOLD_IRUN) >> 8) & 0x1FU;
  CHECK_EQ(rig.ChipOf().Reg(m::IHOLD_IRUN) & 0x1FU, irun); // tuning: IHOLD = IRUN
  CHECK_OK(rig.drv.Enable());
  CHECK(rig.ChipOf().at1_done);
  CHECK(!rig.ChipOf().at1_failed);
  CHECK((rig.ChipOf().Reg(m::IHOLD_IRUN) & 0x1FU) < irun);
}

TEST_CASE("T59 StealthChop needs a run current above the lower limit") {
  Config c = BaseConfig();
  c.stealth.enable = true;
  c.motor.coil_resistance_mohm = 500; // a low resistance raises the limit
  c.board.supply_mv = 48'000;
  c.current = Current{100, 50};
  SpiRig rig(c);
  CHECK_ERR(rig.drv.EnsureInitialized(), Error::Refused);
}

TEST_CASE("T60 StealthChop switched on later waits for standstill and tunes") {
  SpiRig rig;
  REQUIRE(rig.Up());
  StealthChop s;
  s.enable = true;
  CHECK_OK(rig.drv.SetStealthChop(s));
  CHECK((rig.ChipOf().Reg(m::GCONF) & 4U) != 0U);
  CHECK(rig.ChipOf().at1_done);
  CHECK(!rig.ChipOf().at1_failed);
  const auto st = rig.drv.ReadStealthChopState();
  CHECK(st.ok());
  CHECK_OK(rig.drv.SetCurrent(400, 100)); // a current change in StealthChop keeps working
}

TEST_CASE("T61 StealthChop is refused while the motor moves") {
  SpiRig rig;
  REQUIRE(rig.Up());
  REQUIRE(rig.drv.RunVelocity(50'000).ok());
  rig.Run(1'000'000);
  StealthChop s;
  s.enable = true;
  CHECK_ERR(rig.drv.SetStealthChop(s), Error::Busy);
}

TEST_CASE("T62 chopper, thresholds, CoolStep and StallGuard words reach the right registers") {
  SpiRig rig;
  REQUIRE(rig.Up());
  Chopper ch;
  ch.toff = 5;
  ch.tbl = 1;
  ch.mres = Microsteps::M16;
  CHECK_OK(rig.drv.SetChopper(ch));
  CHECK_EQ(rig.ChipOf().Reg(m::CHOPCONF) & 0xFU, 5U);
  CHECK_EQ((rig.ChipOf().Reg(m::CHOPCONF) >> 24) & 0xFU, 4U);
  Thresholds t{.pwm = 500, .cool = 300, .high = 100};
  CHECK_OK(rig.drv.SetThresholds(t));
  CHECK_EQ(rig.ChipOf().Reg(m::TPWMTHRS), 500U);
  CHECK_EQ(rig.ChipOf().Reg(m::TCOOLTHRS), 300U);
  CHECK_EQ(rig.ChipOf().Reg(m::THIGH), 100U);
  CHECK_OK(rig.drv.SetCoolStep(CoolStep{.semin = 3, .semax = 2, .seup = 1, .sedn = 1}));
  CHECK_EQ(rig.ChipOf().Reg(m::COOLCONF) & 0xFU, 3U);
  CHECK_OK(rig.drv.SetStallGuard(StallGuard{.threshold = -5, .filter = true}));
  CHECK_EQ((rig.ChipOf().Reg(m::COOLCONF) >> 16) & 0x7FU, 0x7BU);
  CHECK_EQ((rig.ChipOf().Reg(m::COOLCONF) >> 24) & 1U, 1U);
}

TEST_CASE("T63 setters reject what the datasheet forbids and send nothing") {
  SpiRig rig;
  REQUIRE(rig.Up());
  const uint32_t w = rig.ChipOf().writes;
  Chopper ch;
  ch.toff = 16;
  CHECK_ERR(rig.drv.SetChopper(ch), Error::InvalidArgument);
  Thresholds t{.pwm = 100, .cool = 500};
  CHECK_ERR(rig.drv.SetThresholds(t),
            Error::InvalidArgument); // TPWMTHRS above TCOOLTHRS order broken
  CHECK_ERR(rig.drv.SetStallGuard(StallGuard{.threshold = 0}), Error::Ok);
  CHECK_EQ(rig.ChipOf().writes - w, 1U); // only the valid one wrote
}

TEST_CASE("T64 DcStep and its readings") {
  SpiRig rig;
  REQUIRE(rig.Up());
  CHECK_ERR(rig.drv.SetDcStep(DcStep{.vdcmin = 1024, .dc_time = 100, .dc_sg = 5}),
            Error::InvalidArgument);
  Chopper ch;
  ch.high_fullstep = true;
  ch.high_const_off = true;
  CHECK_OK(rig.drv.SetChopper(ch));
  CHECK_OK(rig.drv.SetDcStep(DcStep{.vdcmin = 1024, .dc_time = 100, .dc_sg = 5}));
  CHECK_EQ(rig.ChipOf().Reg(m::VDCMIN), 1024U);
  CHECK_ERR(rig.drv.SetDcStep(DcStep{.vdcmin = 100}), Error::InvalidArgument);
  CHECK_ERR(rig.drv.ReadLostSteps(), Error::Unsupported); // motion controller mode
}

TEST_CASE(
    "T65 StepDir mode: the ramp, switches and encoder are out; configuration and readings stay") {
  Config c = BaseConfig();
  c.mode = Mode::StepDir;
  SpiRig rig(c);
  REQUIRE(rig.Up());
  CHECK_ERR(rig.drv.SetRamp(RampProfile{}), Error::Unsupported);
  CHECK_ERR(rig.drv.SetSwitches(Switches{}), Error::Unsupported);
  CHECK_ERR(rig.drv.SetEncoder(Encoder{}), Error::Unsupported);
  CHECK_ERR(rig.drv.ReadSnapshot(), Error::Unsupported);
  CHECK_OK(rig.drv.SetCurrent(300, 100));
  CHECK_OK(rig.drv.SetChopper(Chopper{}));
  CHECK_OK(rig.drv.ReadDriverStatus());
  CHECK_OK(rig.drv.ReadLostSteps());
  CHECK_EQ(rig.ChipOf().Reg(m::RAMPMODE), 0U);
}

TEST_CASE("T66 flags and DIAG outputs (StepDir mode has DIAG0 and DIAG1 functions)") {
  Config c = BaseConfig();
  c.mode = Mode::StepDir;
  SpiRig rig(c);
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.SetFlags(Flags{.invert_direction = true}));
  CHECK_OK(rig.drv.SetDiag(Diag{.diag0_error = true, .diag0_push_pull = true}));
  CHECK((rig.ChipOf().Reg(m::GCONF) & 0x10U) != 0U);
  CHECK((rig.ChipOf().Reg(m::GCONF) & 0x20U) != 0U);
  CHECK((rig.ChipOf().Reg(m::GCONF) & (1U << 12)) != 0U);
  const auto idle = rig.drv.Diag0Active();
  REQUIRE(idle.ok());
  CHECK(!idle.value);
  rig.ChipOf().InjectShort();
  const auto active = rig.drv.Diag0Active();
  CHECK(active.ok() && active.value);
  Config mc = BaseConfig();
  SpiRig rig2(mc);
  REQUIRE(rig2.Up());
  CHECK_ERR(rig2.drv.SetDiag(Diag{.diag0_error = true}), Error::InvalidArgument); // StepDir only
}

TEST_CASE("T67 DIAG reading without a GPIO hook is Unsupported") {
  Rig<MinimalSpiPort> rig;
  REQUIRE(rig.drv.EnsureInitialized().ok());
  CHECK_ERR(rig.drv.Diag0Active(), Error::Unsupported);
  CHECK_ERR(rig.drv.Diag1Active(), Error::Unsupported);
}

TEST_CASE(
    "T68 the GCONF test_mode bit is never written, even if the application asks for raw words") {
  SpiRig rig;
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.SetFlags(Flags{
      .invert_direction = true, .recalibrate = true, .multistep_filt = true, .stop_input = true}));
  CHECK_EQ(rig.ChipOf().forbidden_writes, 0U);
  CHECK_EQ(rig.ChipOf().Reg(m::GCONF) & (1U << 17), 0U);
}

TEST_CASE("T69 StallGuard and driver status reads decode the chip's values") {
  SpiRig rig;
  REQUIRE(rig.Up());
  rig.ChipOf().SetSgResult(321);
  const auto sg = rig.drv.ReadStallGuard();
  CHECK(sg.ok() && (sg.value == 321U));
  rig.ChipOf().InjectOvertemp(false);
  const auto d = rig.drv.ReadDriverStatus();
  CHECK(d.ok() && d.value.over_temp_warning && !d.value.over_temp);
}

TEST_CASE("T70 inputs, offsets, OTP, trim and identity") {
  SpiRig rig;
  REQUIRE(rig.Up());
  rig.ChipOf().SetOffsets(5, -3);
  const auto off = rig.drv.ReadOffsets();
  CHECK(off.ok() && (off.value.phase_a == 5) && (off.value.phase_b == -3));
  CHECK_EQ(rig.drv.Identify().value, 0x30U);
  CHECK_EQ(rig.drv.ReadClockTrim().value, 0x0AU);
  CHECK_OK(rig.drv.ReadOtp());
  const auto in = rig.drv.ReadInputs();
  CHECK(in.ok() && (in.value.version == 0x30U));
}

TEST_CASE("T71 a failure between the two current registers never leaves the coil above the rated "
          "current") {
  const uint32_t steps[][2] = {{900, 100}, {100, 900}, {950, 950}, {60, 900},
                               {900, 60},  {400, 800}, {800, 400}};
  for (const auto& step : steps) {
    for (int at = 0; at < 3; ++at) {
      SpiRig rig;
      REQUIRE(rig.Up());
      REQUIRE(rig.drv.SetCurrent(step[0], step[0] / 3).ok());
      rig.WireOf().faults.fail_at = rig.WireOf().transfers + at;
      // Ok or Bus, by where the failure falls: only the peak current is asserted.
      static_cast<void>(rig.drv.SetCurrent(step[1], step[1] / 3));
      rig.WireOf().faults.fail_at = -1;
      CHECK(rig.ChipOf().peak_coil_ma <= 1000.0);
    }
  }
}
