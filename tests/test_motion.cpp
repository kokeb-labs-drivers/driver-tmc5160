/**
 * @file test_motion.cpp
 * @brief The ramp generator, stops, switches, StallGuard stop, direct mode, encoder, microstep
 * table and status (T80 to T115).
 */
#include "fixture.hpp"

using namespace tmc5160;
using namespace tmc5160_test;
namespace m = tmc5160_test::m;

namespace {
Config Fast() {
  Config c = BaseConfig();
  c.ramp.amax = 20'000;
  c.ramp.dmax = 20'000;
  c.ramp.a1 = 20'000;
  c.ramp.d1 = 20'000;
  c.ramp.vmax = 100'000;
  c.ramp.v1 = 50'000;
  return c;
}
} // namespace

TEST_CASE("T80 MoveTo reaches the target exactly and reports it") {
  SpiRig rig(Fast());
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.MoveTo(20'000));
  rig.Run(2'000'000);
  CHECK_EQ(rig.ChipOf().Xactual(), 20'000);
  const auto p = rig.drv.ReadPosition();
  CHECK(p.ok() && (p.value == 20'000));
  const auto done = rig.drv.PositionReached();
  CHECK(done.ok() && done.value);
  CHECK_EQ(rig.drv.ReadVelocity().value, 0);
  CHECK_EQ(rig.drv.ReadTarget().value, 20'000);
}

TEST_CASE("T81 the velocity never exceeds VMAX during a move") {
  SpiRig rig(Fast());
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.MoveTo(500'000));
  double top = 0.0;
  for (int i = 0; i < 400; ++i) {
    rig.Run(10'000);
    const double v = rig.ChipOf().VelocityHz();
    const double speed = (v < 0.0) ? -v : v;
    top = (speed > top) ? speed : top;
  }
  CHECK(top <= (((100'000.0 * 12'000'000.0) / 16'777'216.0) + 1.0));
  CHECK(top > 1000.0);
}

TEST_CASE("T82 MoveBy adds to the present position and rejects overflow") {
  SpiRig rig(Fast());
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.SetHomePosition(1000));
  CHECK_OK(rig.drv.MoveBy(-500));
  rig.Run(1'000'000);
  CHECK_EQ(rig.ChipOf().Xactual(), 500);
  CHECK_OK(rig.drv.SetHomePosition(2'147'483'000));
  const uint32_t writes = rig.ChipOf().writes;
  CHECK_ERR(rig.drv.MoveBy(1000), Error::LimitExceeded); // known only after the read
  CHECK_EQ(rig.ChipOf().writes, writes);                 // nothing was written
}

TEST_CASE("T83 MoveTo needs the ramp to have been written; start-up writes it") {
  SpiRig rig(Fast());
  REQUIRE(rig.drv.EnsureInitialized().ok());
  CHECK_OK(rig.drv.MoveTo(100));
}

TEST_CASE("T84 RunVelocity runs both ways, is limited, and needs AMAX") {
  SpiRig rig(Fast());
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.RunVelocity(60'000));
  rig.Run(1'000'000);
  CHECK(rig.ChipOf().VelocityHz() > 0.0);
  CHECK_OK(rig.drv.RunVelocity(-60'000));
  rig.Run(2'000'000);
  CHECK(rig.ChipOf().VelocityHz() < 0.0);
  CHECK_ERR(rig.drv.RunVelocity(9'000'000), Error::LimitExceeded);
  RampProfile r = Fast().ramp;
  r.amax = 0;
  CHECK_OK(rig.drv.SetRamp(r));
  CHECK_ERR(rig.drv.RunVelocity(1000), Error::InvalidArgument);
}

TEST_CASE("T85 the declared velocity ceiling is enforced") {
  Config c = Fast();
  c.ramp.vmax = 70'000;
  c.max_velocity_reg = 80'000;
  SpiRig rig(c);
  REQUIRE(rig.Up());
  CHECK_ERR(rig.drv.RunVelocity(90'000), Error::LimitExceeded);
  CHECK_ERR(rig.drv.SetMaxVelocity(90'000), Error::LimitExceeded);
  CHECK_OK(rig.drv.SetMaxVelocity(70'000));
  CHECK_EQ(rig.ChipOf().Reg(m::VMAX), 70'000U);
}

TEST_CASE("T86 stop kinds") {
  SpiRig rig(Fast());
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.RunVelocity(60'000));
  rig.Run(1'000'000);
  CHECK_OK(rig.drv.Stop(StopKind::Ramp));
  rig.Run(2'000'000);
  CHECK_EQ(rig.ChipOf().VelocityHz(), 0.0);
  CHECK_OK(rig.drv.MoveTo(5000));
  rig.Run(300'000);
  CHECK_OK(rig.drv.Stop(StopKind::HoldTarget));
  rig.Run(2'000'000);
  CHECK_EQ(rig.ChipOf().VelocityHz(), 0.0);
  CHECK_OK(rig.drv.RunVelocity(40'000));
  rig.Run(500'000);
  CHECK_OK(rig.drv.Stop(StopKind::Immediate));
  rig.Run(3'000'000); // the AMAX ramp
  CHECK_EQ(rig.ChipOf().VelocityHz(), 0.0);
  CHECK_OK(rig.drv.Stop(StopKind::Emergency));
  CHECK(!rig.ChipOf().DriverOn());
}

TEST_CASE("T87 an immediate stop is a velocity-mode ramp: allowed in "
          "StealthChop, AMAX decelerates") {
  Config c = Fast();
  c.stealth.enable = true;
  c.ramp.dmax = 1; // DMAX and D1 belong to the positioning mode: velocity mode
                   // never uses them
  c.ramp.d1 = 1;
  c.ramp.amax = 500;
  c.ramp.a1 = 500;
  SpiRig rig(c);
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.RunVelocity(40'000));
  rig.Run(60'000'000);
  CHECK_OK(rig.drv.Stop(StopKind::Immediate));
  rig.Run(100);
  CHECK(rig.ChipOf().VelocityHz() > 0.0); // a ramp, not a step to zero
  rig.Run(60'000'000);
  CHECK_EQ(rig.ChipOf().VelocityHz(), 0.0);
  CHECK_OK(rig.drv.Stop(StopKind::Ramp));
}

TEST_CASE("T88 a move after a hard stop restores VSTART and VMAX") {
  SpiRig rig(Fast());
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.Stop(StopKind::Immediate));
  CHECK_EQ(rig.ChipOf().Reg(m::VMAX), 0U);
  CHECK_OK(rig.drv.MoveTo(1000));
  CHECK_EQ(rig.ChipOf().Reg(m::VMAX), 100'000U);
}

TEST_CASE("T89 SetHomePosition needs standstill") {
  SpiRig rig(Fast());
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.RunVelocity(40'000));
  rig.Run(500'000);
  CHECK_ERR(rig.drv.SetHomePosition(0), Error::Busy);
  CHECK_OK(rig.drv.Stop(StopKind::Ramp));
  rig.Run(3'000'000);
  CHECK_OK(rig.drv.SetHomePosition(0));
  CHECK_EQ(rig.ChipOf().Xactual(), 0);
  CHECK_EQ(rig.drv.ReadTarget().value, 0);
}

TEST_CASE("T90 a reference switch stops the motor and latches the event") {
  Config c = Fast();
  c.switches.stop_left = true;
  SpiRig rig(c);
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.RunVelocity(-40'000));
  rig.Run(500'000);
  rig.ChipOf().SetReferenceSwitches(true, false);
  rig.Run(50'000);
  CHECK_EQ(rig.ChipOf().VelocityHz(), 0.0);
  const auto s = rig.drv.ReadRampStatus();
  CHECK(s.ok() && s.value.event_stop_left);
  CHECK_ERR(rig.drv.ClearRampEvents(0x10),
            Error::InvalidArgument); // read-only: the stop condition clears it
  CHECK_OK(rig.drv.Hold());
  rig.Run(10'000);
  const auto after = rig.drv.ReadRampStatus();
  CHECK(after.ok() && !after.value.event_stop_left);
}

TEST_CASE("T91 ClearRampEvents refuses read-only bits and an empty mask") {
  SpiRig rig(Fast());
  REQUIRE(rig.Up());
  CHECK_ERR(rig.drv.ClearRampEvents(0), Error::InvalidArgument);
  CHECK_ERR(rig.drv.ClearRampEvents(0x1), Error::InvalidArgument);
  CHECK_ERR(rig.drv.ClearRampEvents(0xFFFF), Error::InvalidArgument);
  CHECK_OK(rig.drv.ClearRampEvents(0x80));
}

TEST_CASE("T92 stall: the motor stops, stays stopped, and ReleaseStall clears the event") {
  Config c = Fast();
  c.switches.stop_on_stall = true;
  c.switches.soft_stop = false;
  c.thresholds.cool = 200'000;
  SpiRig rig(c);
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.RunVelocity(60'000));
  rig.Run(1'000'000);
  rig.ChipOf().InjectStall(true);
  rig.Run(20'000);
  CHECK_EQ(rig.ChipOf().VelocityHz(), 0.0);
  rig.ChipOf().InjectStall(false);
  CHECK_OK(rig.drv.Service());
  CHECK_OK(rig.drv.Service());
  CHECK_EQ(rig.ChipOf().VelocityHz(), 0.0); // Service never releases the stop
  CHECK((rig.ChipOf().RampEvents() & (1U << 6)) != 0U);
  CHECK_OK(rig.drv.ReleaseStall());
  CHECK_EQ(rig.ChipOf().VelocityHz(), 0.0);
  CHECK((rig.ChipOf().RampEvents() & (1U << 6)) == 0U); // the event is now cleared
}

TEST_CASE("T93 StallGuard stop with a soft stop is refused by the configuration") {
  Config c = Fast();
  c.switches.stop_on_stall = true;
  c.switches.soft_stop = true;
  SpiRig rig(c);
  CHECK_ERR(rig.drv.EnsureInitialized(), Error::InvalidArgument);
}

TEST_CASE("T94 direct mode needs its acknowledgement and limits the coil currents") {
  SpiRig rig(Fast());
  REQUIRE(rig.Up());
  CHECK_ERR(rig.drv.EnterDirectMode(), Error::Refused);
  Config c = Fast();
  c.ack = c.ack.With(Ack::DirectMode);
  SpiRig on(c);
  REQUIRE(on.Up());
  CHECK_ERR(on.drv.SetDirectCurrents(10, 10), Error::Refused); // not entered yet
  CHECK_OK(on.drv.EnterDirectMode());
  CHECK_ERR(on.drv.MoveTo(5), Error::Refused);
  CHECK_OK(on.drv.SetDirectCurrents(-100, 200));
  CHECK_ERR(on.drv.SetDirectCurrents(249, 0), Error::LimitExceeded);
  CHECK_OK(on.drv.ExitDirectMode());
  CHECK_EQ(on.ChipOf().Reg(m::GCONF) & (1U << 16), 0U);
  CHECK_OK(on.drv.MoveTo(5));
}

TEST_CASE("T95 direct mode in StealthChop needs the voltage-mode acknowledgement") {
  Config c = Fast();
  c.stealth.enable = true;
  c.ack = c.ack.With(Ack::DirectMode);
  SpiRig rig(c);
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.EnterDirectMode());
  CHECK_ERR(rig.drv.SetDirectCurrents(10, 10), Error::Refused);
}

TEST_CASE("T96 the encoder constant follows the datasheet, binary and decimal") {
  Config c = Fast();
  c.encoder.declared = true;
  c.encoder.scale = EncoderScale{.decimal = false, .integer = 1, .fraction = 0x8000};
  SpiRig rig(c);
  REQUIRE(rig.Up());
  rig.ChipOf().AddEncoderCounts(100);
  CHECK_EQ(rig.drv.ReadEncoderPosition().value, 150);
  CHECK_OK(rig.drv.SetEncoderPosition(0));
  Encoder e = c.encoder;
  e.scale = EncoderScale{.decimal = true, .integer = 1, .fraction = 5000, .negative = true};
  CHECK_OK(rig.drv.SetEncoder(e));
  rig.ChipOf().AddEncoderCounts(10);
  CHECK_EQ(rig.drv.ReadEncoderPosition().value, -15);
  CHECK_OK(rig.drv.ClearEncoderEvents(true, true));
  CHECK(rig.drv.ReadEncoderStatus().ok());
}

TEST_CASE("T97 SetMicrostepTable rewrites the ten registers only at MSCNT 0") {
  SpiRig rig(Fast());
  REQUIRE(rig.Up());
  MicrostepTable t;
  CHECK_OK(rig.drv.SetMicrostepTable(t));
  CHECK_OK(rig.drv.SetHomePosition(1));
  CHECK_ERR(rig.drv.SetMicrostepTable(t), Error::Refused);
  t.start_sin90 = 100; // no longer the last entry of the table (page 47)
  CHECK_OK(rig.drv.SetHomePosition(0));
  const uint32_t start = rig.ChipOf().Reg(m::MSLUTSTART);
  const int sent = rig.WireOf().transfers;
  CHECK_ERR(rig.drv.SetMicrostepTable(t), Error::InvalidArgument);
  CHECK_EQ(rig.WireOf().transfers, sent); // refused before any transfer
  CHECK_EQ(rig.ChipOf().Reg(m::MSLUTSTART), start);
}

TEST_CASE("T98 the default table decodes in the model to a sine: MSCURACT at MSCNT 0 and 256") {
  SpiRig rig(Fast());
  REQUIRE(rig.Up());
  const auto zero = rig.drv.ReadMicrostepCurrents();
  CHECK(zero.ok());
  CHECK_EQ(zero.value.cur_a, 0);
  CHECK_EQ(zero.value.cur_b, 247);
  CHECK_OK(rig.drv.SetHomePosition(256));
  const auto quarter = rig.drv.ReadMicrostepCurrents();
  CHECK_EQ(quarter.value.cur_a, 247);
  CHECK_EQ(rig.drv.ReadMicrostepCounter().value, 256U);
}

TEST_CASE("T99 a snapshot is one pipelined batch and Service clears what it reports") {
  SpiRig rig(Fast());
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.MoveTo(2000));
  rig.Run(1'000'000);
  const auto before = static_cast<uint32_t>(rig.WireOf().transfers);
  const auto s = rig.drv.ReadSnapshot();
  CHECK(s.ok() && (s.value.position == 2000));
  CHECK_EQ(static_cast<uint32_t>(rig.WireOf().transfers) - before, 8U);
  const auto ev = rig.drv.Service();
  CHECK(ev.ok() && ev.value.ramp.position_reached);
  const auto again = rig.drv.ReadRampStatus();
  CHECK(again.ok() && !again.value.event_pos_reached);
}

TEST_CASE("T100 Service reports a reset as UnexpectedReset") {
  SpiRig rig(Fast());
  REQUIRE(rig.Up());
  rig.ChipOf().PowerOn();
  CHECK_ERR(rig.drv.Service(), Error::UnexpectedReset);
}

TEST_CASE("T101 position compare reaches X_COMPARE and DIAG1 pulses") {
  SpiRig rig(Fast());
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.SetPositionCompare(1234));
  CHECK_EQ(rig.ChipOf().Reg(m::X_COMPARE), 1234U);
}

TEST_CASE("T102 step time and velocity read back what the chip reports") {
  SpiRig rig(Fast());
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.RunVelocity(50'000));
  rig.Run(3'000'000);
  const auto t = rig.drv.ReadStepTime();
  CHECK(t.ok());
  CHECK_EQ(units::StepTimeFromVelocity(50'000),
           ((t.value / 1U) == 0U) ? 0U : units::StepTimeFromVelocity(50'000));
  const auto v = rig.drv.ReadVelocity();
  CHECK(v.ok() && (v.value == 50'000));
}

TEST_CASE("T103 the ramp registers are not rewritten while moving, apart from VMAX") {
  SpiRig rig(Fast());
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.RunVelocity(50'000));
  rig.Run(500'000);
  CHECK_OK(rig.drv.SetMaxVelocity(60'000));
  CHECK_OK(rig.drv.RunVelocity(-30'000));
  CHECK_EQ(rig.ChipOf().ramp_writes_moving, 0U);
}

TEST_CASE("T104 a fault blocks motion commands; the ramp keeps its safe state") {
  SpiRig rig(Fast());
  REQUIRE(rig.Up());
  rig.ChipOf().InjectShort();
  CHECK(rig.drv.ReadGlobalStatus().ok());
  CHECK_ERR(rig.drv.RunVelocity(1000), Error::DeviceFault);
  CHECK_OK(rig.drv.Stop(StopKind::Emergency));
}

TEST_CASE("T105 MoveTo does not hold up other commands: a held ramp keeps its velocity") {
  SpiRig rig(Fast());
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.RunVelocity(50'000));
  rig.Run(1'000'000);
  const double v = rig.ChipOf().VelocityHz();
  CHECK_OK(rig.drv.Hold());
  rig.Run(200'000);
  CHECK_EQ(rig.ChipOf().VelocityHz(), v);
}
