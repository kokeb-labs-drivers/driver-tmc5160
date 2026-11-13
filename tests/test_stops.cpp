/**
 * @file test_stops.cpp
 * @brief Tests that pin how the driver ends motion and when it energises again: a Stop after a
 * failed start-up, the switch-off ramp stop, the TZEROWAIT pause and the standstill check, the
 * order of the motion writes and the stuck-line checks (T246 to T261).
 */
#include <algorithm>
#include <cstdint>

#include "fixture.hpp"

using namespace tmc5160;
using namespace tmc5160_test;
namespace m = tmc5160_test::m;

TEST_CASE("T246 a Stop that returns Ok stops a chip left running when the start-up failed") {
  for (const StopKind kind : {StopKind::Ramp, StopKind::Immediate, StopKind::HoldTarget}) {
    int checked = 0;
    for (int k = 0; k < 14; ++k) {
      SpiRig rig(StallConfig());
      REQUIRE(rig.Up());
      CHECK_OK(rig.drv.RunVelocity(60'000));
      rig.Run(1'000'000);
      REQUIRE(rig.ChipOf().VelocityHz() > 1000.0);
      CHECK_OK(rig.drv.Deinitialize());
      rig.ChipOf().Write(m::RAMPMODE, 1U); // the last session's ramp is running
      rig.ChipOf().Write(m::VMAX, 100'000U);
      rig.WireOf().faults.fail_at = rig.WireOf().transfers + k;
      if (rig.drv.EnsureInitialized().ok()) {
        continue;
      }
      rig.WireOf().faults.fail_at = -1;
      const auto r = rig.drv.Stop(kind);
      if (r.ok()) {
        rig.Run(3'000'000);
        CHECK_EQ(rig.ChipOf().VelocityHz(), 0.0);
        ++checked;
      }
    }
    CHECK(checked > 0);
  }
}

TEST_CASE("T247 a switch-off stops the ramp generator, so Enable cannot resume the motion") {
  for (int how = 0; how < 3; ++how) {
    SpiRig rig(StallConfig());
    REQUIRE(rig.Up());
    if (how == 2) {
      CHECK_OK(rig.drv.MoveTo(5'000'000));
    } else {
      CHECK_OK(rig.drv.RunVelocity(60'000));
    }
    rig.Run(1'000'000);
    REQUIRE(rig.ChipOf().VelocityHz() > 1000.0);
    if (how == 1) {
      CHECK_OK(rig.drv.Stop(StopKind::Emergency));
    } else {
      CHECK_OK(rig.drv.Disable());
    }
    CHECK_EQ(rig.ChipOf().Reg(m::VMAX), 0U);
    CHECK_ERR(rig.drv.Enable(), Error::Busy); // still ramping down: not energised into it
    CHECK(!rig.ChipOf().DrvEnnActive());
    rig.Run(3'000'000);
    CHECK_OK(rig.drv.Enable());
    rig.Run(3'000'000);
    CHECK_EQ(rig.ChipOf().VelocityHz(), 0.0); // nobody asked for motion
  }
}

TEST_CASE("T248 SPI Enable proves GLOBALSCALER and IHOLD_IRUN each: a corrupted datagram of either "
          "is VerifyFailed") {
  int failed = 0;
  for (int k = 0; k < 16; ++k) {
    SpiRig rig;
    REQUIRE(rig.drv.EnsureInitialized().ok());
    rig.WireOf().faults.corrupt_mosi_at = rig.WireOf().transfers + k;
    const auto r = rig.drv.Enable();
    if (r.error == Error::VerifyFailed) {
      ++failed;
      CHECK(!rig.ChipOf().DrvEnnActive());
    }
  }
  CHECK_EQ(failed, 2);
}

TEST_CASE("T249 a multi-register Echo call proves every write") {
  int failed = 0;
  for (int k = 0; k < 6; ++k) {
    SpiRig rig;
    REQUIRE(rig.Up());
    rig.WireOf().faults.corrupt_mosi_at = rig.WireOf().transfers + k;
    const auto r = rig.drv.SetStandstill(Standstill{.hold_delay = 3}, Verify::Echo);
    failed += (r.error == Error::VerifyFailed) ? 1 : 0;
  }
  CHECK(failed >= 2);
}

TEST_CASE("T250 a reset flag in the first transfer of a stop does not abort the stop") {
  for (const StopKind kind : {StopKind::Ramp, StopKind::Immediate}) {
    SpiRig rig(StallConfig());
    REQUIRE(rig.Up());
    CHECK_OK(rig.drv.RunVelocity(60'000));
    rig.Run(500'000);
    rig.WireOf().faults.stuck = 2; // every status byte of this call reports a reset
    CHECK_ERR(rig.drv.Stop(kind), Error::UnexpectedReset);
    rig.WireOf().faults.stuck = 0;
    CHECK_EQ(rig.ChipOf().Reg(m::VMAX), 0U); // all the writes went out
    CHECK_EQ(rig.ChipOf().Reg(m::RAMPMODE), 1U);
    CHECK(!rig.drv.IsInitialized());
  }
}

TEST_CASE("T251 Stop and Hold map a verification the bus cannot do instead of refusing") {
  SpiRig spi(StallConfig());
  REQUIRE(spi.Up());
  CHECK_OK(spi.drv.RunVelocity(60'000));
  CHECK_OK(spi.drv.Stop(StopKind::Ramp, Verify::Counter));
  CHECK_EQ(spi.ChipOf().Reg(m::VMAX), 0U);
  UartRig uart(StallConfig());
  REQUIRE(uart.Up());
  CHECK_OK(uart.drv.RunVelocity(60'000));
  CHECK_OK(uart.drv.Stop(StopKind::Ramp, Verify::Echo));
  CHECK_EQ(uart.ChipOf().Reg(m::VMAX), 0U);
  CHECK_OK(uart.drv.Hold(Verify::Echo));
  CHECK_EQ(uart.ChipOf().Reg(m::RAMPMODE), 3U);
  Config direct = BaseConfig();
  direct.ack = direct.ack.With(Ack::DirectMode);
  UartRig coils(direct);
  REQUIRE(coils.Up());
  CHECK_OK(coils.drv.EnterDirectMode());
  CHECK_OK(coils.drv.Hold(Verify::Echo)); // zeroCoils in direct mode maps it too
}

TEST_CASE("T252 Enable never energises during the TZEROWAIT pause of a start-up stop") {
  Config c = StallConfig();
  c.ramp.zero_wait = 60'000; // about 2.5 s of standstill at the model's clock (page 83 c)
  SpiRig rig(c);
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.RunVelocity(60'000));
  rig.Run(1'000'000);
  REQUIRE(rig.ChipOf().VelocityHz() > 1000.0);
  CHECK_OK(rig.drv.Deinitialize());
  rig.ChipOf().Write(m::RAMPMODE, 1U); // the last session's ramp is running again
  rig.ChipOf().Write(m::VMAX, 100'000U);
  CHECK_OK(rig.drv.EnsureInitialized()); // position mode with XTARGET = XACTUAL (option c)
  rig.WireOf().xfer_us = 250U;           // the chip runs on while the reads are on the line
  int busy_in_pause = 0;
  bool enabled = false;
  for (int i = 0; (i < 600) && !enabled; ++i) {
    const bool still = rig.ChipOf().VelocityHz() == 0.0;
    const auto r = rig.drv.Enable();
    if (r.ok()) {
      enabled = true;
    } else {
      CHECK_EQ(static_cast<int>(r.error), static_cast<int>(Error::Busy));
      busy_in_pause += still ? 1 : 0;
    }
    rig.Run(20'000);
  }
  CHECK(enabled);
  CHECK(busy_in_pause > 0); // the pause itself answered Busy: VACTUAL 0 is not proof
  rig.Run(5'000'000);
  CHECK_EQ(rig.ChipOf().VelocityHz(), 0.0); // the shaft did not run back after the Enable
}

TEST_CASE("T253 a switch-off writes VSTART 0 with the ramp stop") {
  for (const bool emergency : {false, true}) {
    Config c = StallConfig();
    c.ramp.vstart = 8;
    SpiRig rig(c);
    REQUIRE(rig.Up());
    CHECK_OK(rig.drv.RunVelocity(60'000));
    rig.Run(500'000);
    CHECK_OK(emergency ? rig.drv.Stop(StopKind::Emergency) : rig.drv.Disable());
    CHECK_EQ(rig.ChipOf().Reg(m::VSTART), 0U);
    CHECK_EQ(rig.ChipOf().Reg(m::VMAX), 0U);
  }
}

TEST_CASE("T254 a switch-off zeroes XTARGET in direct mode and when the mode is in doubt") {
  Config direct = BaseConfig();
  direct.ack = direct.ack.With(Ack::DirectMode);
  SpiRig rig(direct);
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.EnterDirectMode());
  CHECK_OK(rig.drv.SetDirectCurrents(50, 50));
  REQUIRE(rig.ChipOf().Xtarget() != 0);
  CHECK_OK(rig.drv.Disable());
  CHECK_EQ(rig.ChipOf().Xtarget(), 0);
  CHECK_OK(rig.drv.Enable()); // energises zero coil current
  SpiRig doubt(direct);
  REQUIRE(doubt.Up());
  doubt.WireOf().faults.fail_at = doubt.WireOf().transfers + 4; // the GCONF write of Enter
  CHECK_ERR(doubt.drv.EnterDirectMode(), Error::Bus);
  doubt.WireOf().faults.fail_at = -1;
  doubt.ChipOf().Write(m::XTARGET, 0x00640064U); // the chip may hold coil currents
  CHECK_OK(doubt.drv.Disable());
  CHECK_EQ(doubt.ChipOf().Xtarget(), 0);
}

TEST_CASE("T255 a motor that stopped a moment ago is not a stuck line") {
  SpiRig rig(StallConfig());
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.RunVelocity(60'000));
  rig.Run(1'000'000);
  CHECK_OK(rig.drv.Stop(StopKind::Immediate));
  for (int i = 0; (i < 2000) && (rig.ChipOf().VelocityHz() != 0.0); ++i) {
    rig.Run(1'000);
  }
  REQUIRE(rig.ChipOf().VelocityHz() == 0.0);
  rig.WireOf().xfer_us = 50U; // 2^20 clocks is about 87 ms at the model's clock: still inside
  CHECK_OK(rig.drv.SetHomePosition(0)); // stst is not set yet (2^20 clocks, page 56)
  rig.WireOf().faults.stuck = 1;
  CHECK_ERR(rig.drv.SetHomePosition(0), Error::Bus); // a dead line still is
}

TEST_CASE(
    "T256 the ramp stop of a start-up in doubt never sends a position-mode chip to zero first") {
  SpiRig rig(StallConfig());
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.Deinitialize());
  rig.ChipOf().Write(m::RAMPMODE, 0U); // position mode, moving towards a far target
  rig.ChipOf().Write(m::XTARGET, 50'000'000U);
  rig.ChipOf().Write(m::VMAX, 100'000U);
  // the start-up fails right after the first writes: the stop must have written VMAX 0 before
  // XTARGET 0 (the last write)
  rig.WireOf().faults.fail_at = rig.WireOf().transfers + 2;
  CHECK(!rig.drv.EnsureInitialized().ok());
  rig.WireOf().faults.fail_at = -1;
  CHECK_OK(rig.drv.Stop(StopKind::Ramp));
  CHECK_EQ(rig.ChipOf().Reg(m::VMAX), 0U);
  CHECK_EQ(rig.ChipOf().Xtarget(), 0);
  rig.Run(3'000'000);
  CHECK_EQ(rig.ChipOf().VelocityHz(), 0.0);
  // Timed (UART, 2 ms per exchange): a chip at rest in position mode with VMAX set. XTARGET 0
  // written before VMAX 0 and velocity mode would start it towards 0 while the other writes are
  // on the line.
  UartRig rest(StallConfig());
  REQUIRE(rest.Up());
  CHECK_OK(rest.drv.Deinitialize());
  rest.ChipOf().Write(m::XACTUAL, 100'000U);
  rest.ChipOf().Write(m::XTARGET, 100'000U);
  rest.ChipOf().Write(m::RAMPMODE, 0U);
  rest.ChipOf().Write(m::VMAX, 100'000U);
  rest.WireOf().xfer_us = 2'000U;
  rest.ChipOf().ResetWatch();
  rest.WireOf().faults.fail_from = rest.WireOf().transfers + 2; // the bus dies during start-up
  CHECK(!rest.drv.EnsureInitialized().ok());
  rest.WireOf().faults.fail_from = -1;
  CHECK_OK(rest.drv.Stop(StopKind::Ramp));
  rest.Run(1'000'000);
  CHECK_EQ(rest.ChipOf().Xtarget(), 0);
  CHECK_EQ(rest.ChipOf().min_x, 100'000); // never moved towards 0
}

namespace {
/// The start-up stop of T252 with transfers that take model time: `Enable()` is polled every
/// 10 us after `offset_us`, and every Ok must find the pause over and the shaft at rest for good.
/// The ramp overshoots the captured target, stops, pauses for TZEROWAIT and runs back (page 83 c).
template <typename R>
struct ZeroWaitSweep {
  int busy_in_pause = 0;
  int unsafe_ok = 0;
  bool enabled = false;

  void Run(uint32_t xfer_us, uint32_t offset_us, uint32_t zero_wait = 6'000U) {
    Config c = StallConfig();
    c.ramp.zero_wait = zero_wait;     // 6000: about 0.25 s at the model's clock (page 38)
    c.switches.stop_on_stall = false; // a latched stall stop holds the run-back (Enable doc)
    R rig(c);
    REQUIRE(rig.Up());
    CHECK_OK(rig.drv.RunVelocity(60'000));
    rig.Run(1'000'000);
    CHECK_OK(rig.drv.Deinitialize());
    rig.ChipOf().Write(m::RAMPMODE, 1U); // the last session's ramp is running again
    rig.ChipOf().Write(m::VMAX, 100'000U);
    // Position mode with XTARGET = XACTUAL at speed: the ramp overshoots, stops (and pauses for
    // TZEROWAIT), then runs back (page 83 c).
    CHECK_OK(rig.drv.EnsureInitialized());
    rig.WireOf().xfer_us = xfer_us; // every read samples the chip at a later time
    rig.Run(offset_us);
    for (int i = 0; (i < 200'000) && !enabled; ++i) {
      const bool paused = rig.ChipOf().ZeroWaitActive();
      const auto r = rig.drv.Enable();
      if (r.ok()) {
        enabled = true;
        const int32_t at = rig.ChipOf().Xactual();
        const bool unsafe = rig.ChipOf().ZeroWaitActive() || !rig.ChipOf().Standstill();
        rig.Run(3'000'000);
        unsafe_ok += (unsafe || (rig.ChipOf().Xactual() != at)) ? 1 : 0;
      } else {
        CHECK_ERR(r, Error::Busy);
        busy_in_pause += paused ? 1 : 0;
        rig.Run(10);
      }
    }
  }
};
} // namespace

TEST_CASE("T257 Enable never energises during or at the end of the TZEROWAIT pause of a start-up "
          "stop") {
  // SPI: one datagram tens to hundreds of us; UART: one exchange about a millisecond.
  for (uint32_t offset_us = 0U; offset_us < 2'000U; offset_us += 100U) {
    for (const uint32_t xfer_us : {50U, 250U}) {
      ZeroWaitSweep<SpiRig> spi;
      spi.Run(xfer_us, offset_us);
      CHECK(spi.enabled);
      CHECK(spi.busy_in_pause > 0);
      CHECK_EQ(spi.unsafe_ok, 0);
    }
    for (const uint32_t xfer_us : {1'000U, 2'000U}) {
      ZeroWaitSweep<UartRig> uart;
      uart.Run(xfer_us, offset_us);
      CHECK(uart.enabled);
      CHECK(uart.busy_in_pause > 0);
      CHECK_EQ(uart.unsafe_ok, 0);
    }
  }
}

TEST_CASE("T262 Enable never energises at the reversal of a start-up stop without a pause") {
  // TZEROWAIT 0 (the default): the ramp stops past the captured XTARGET and turns back at once;
  // at that instant vzero is set and no pause runs, only position_reached is clear (page 44).
  for (uint32_t offset_us = 0U; offset_us < 2'000U; offset_us += 100U) {
    for (const uint32_t xfer_us : {50U, 250U}) {
      ZeroWaitSweep<SpiRig> spi;
      spi.Run(xfer_us, offset_us, 0U);
      CHECK(spi.enabled);
      CHECK_EQ(spi.unsafe_ok, 0);
    }
    for (const uint32_t xfer_us : {1'000U, 2'000U}) {
      ZeroWaitSweep<UartRig> uart;
      uart.Run(xfer_us, offset_us, 0U);
      CHECK(uart.enabled);
      CHECK_EQ(uart.unsafe_ok, 0);
    }
  }
}

namespace {
/// The fast ramp without the stall stop, with VSTART (and VSTOP, so that no acknowledgement is
/// needed) as given.
Config MoveConfig(uint32_t vstart) {
  Config c = StallConfig();
  c.switches.stop_on_stall = false;
  c.ramp.vstart = vstart;
  c.ramp.vstop = std::max(vstart, uint32_t{10});
  return c;
}

/// A move after a stop: the chip is in velocity mode with VMAX 0 and XTARGET is the old move's
/// target. The new move must never take the motor the other way (`max_x` stays at the start).
template <typename R>
void MoveAfterStop(bool via_disable, uint32_t vstart, uint32_t xfer_us) {
  R rig(MoveConfig(vstart));
  REQUIRE(rig.Up());
  rig.WireOf().xfer_us = xfer_us;
  CHECK_OK(rig.drv.MoveTo(400'000));
  rig.Run(500'000);
  CHECK_OK(via_disable ? rig.drv.Disable() : rig.drv.Stop(StopKind::Ramp));
  rig.Run(3'000'000);
  REQUIRE(rig.ChipOf().Standstill());
  if (via_disable) {
    CHECK_OK(rig.drv.Enable());
  }
  const int32_t at = rig.ChipOf().Xactual();
  rig.ChipOf().ResetWatch();
  CHECK_OK(rig.drv.MoveTo(at - 50'000));
  rig.Run(500'000);
  CHECK_EQ(rig.ChipOf().max_x, at);             // never above the start
  CHECK(rig.ChipOf().Xactual() < (at - 1'000)); // and on its way down
  CHECK_EQ(rig.ChipOf().stale_target_us, 0.0);
}
} // namespace

TEST_CASE("T258 a move after a stop or a switch-off never starts in the old direction") {
  for (const uint32_t vstart : {0U, 500U}) {
    for (const bool via_disable : {false, true}) {
      MoveAfterStop<UartRig>(via_disable, vstart, 2'000U); // one UART exchange: about 2 ms
      MoveAfterStop<SpiRig>(via_disable, vstart, 250U);
    }
  }
}

TEST_CASE("T259 Stop(HoldTarget) from velocity mode and a start-up stop never head for an old "
          "target") {
  UartRig rig(MoveConfig(0U));
  REQUIRE(rig.Up());
  rig.WireOf().xfer_us = 2'000U;
  CHECK_OK(rig.drv.MoveTo(5'000'000)); // the old target, far ahead
  rig.Run(200'000);
  CHECK_OK(rig.drv.Stop(StopKind::Ramp));
  rig.Run(3'000'000);
  CHECK_OK(rig.drv.RunVelocity(60'000));
  rig.Run(1'000'000);
  rig.ChipOf().ResetWatch();
  CHECK_OK(rig.drv.Stop(StopKind::HoldTarget));
  rig.Run(3'000'000);
  CHECK_EQ(rig.ChipOf().stale_target_us, 0.0);
  // The start-up on a chip left running in velocity mode, XTARGET still the old target.
  CHECK_OK(rig.drv.RunVelocity(60'000));
  rig.Run(1'000'000);
  CHECK_OK(rig.drv.MoveTo(5'000'000));
  rig.Run(100'000);
  CHECK_OK(rig.drv.Deinitialize());
  rig.ChipOf().Write(m::RAMPMODE, 1U);
  rig.ChipOf().Write(m::VMAX, 100'000U);
  rig.Run(1'000'000);
  rig.ChipOf().ResetWatch();
  CHECK_OK(rig.drv.EnsureInitialized());
  rig.Run(3'000'000);
  CHECK_EQ(rig.ChipOf().stale_target_us, 0.0);
}

TEST_CASE("T260 RunVelocity from a move the other way never speeds that move up") {
  UartRig rig(MoveConfig(0U));
  REQUIRE(rig.Up());
  rig.WireOf().xfer_us = 2'000U;
  CHECK_OK(rig.drv.MoveTo(-50'000'000));
  rig.Run(4'000'000); // at its VMAX by now
  const double cruise = rig.ChipOf().VelocityHz();
  REQUIRE(cruise < 0.0);
  rig.ChipOf().ResetWatch();
  CHECK_OK(rig.drv.RunVelocity(static_cast<int32_t>(rig.drv.GetConfig().ramp.vmax) * 2));
  rig.Run(100'000);
  CHECK(rig.ChipOf().min_v >= cruise); // no faster in the old direction
  CHECK(rig.ChipOf().VelocityHz() > cruise);
}

namespace {
/// Transfers of one call while the chip reads 0 everywhere except `keep` (0x80: status byte).
template <typename F>
int BlankTransfers(SpiRig& rig, uint32_t keep, uint32_t value, F call) {
  rig.ChipOf().blank_reads = true;
  rig.ChipOf().blank_keep = keep;
  rig.ChipOf().blank_value = value;
  const int before = rig.WireOf().transfers;
  CHECK(call().ok()); // a live chip, never a stuck line
  const int sent = rig.WireOf().transfers - before;
  rig.ChipOf().blank_reads = false;
  return sent;
}
} // namespace

TEST_CASE("T261 every word of a stuck-line check counts: one non-zero word is a live chip, all "
          "zeros ask IOIN") {
  Config c = BaseConfig();
  c.ack = c.ack.With(Ack::DirectMode);
  SpiRig rig(c);
  REQUIRE(rig.Up());
  const auto service = [&rig] { return rig.drv.Service(); };
  const auto snapshot = [&rig] { return rig.drv.ReadSnapshot(); };
  const auto status = [&rig] { return rig.drv.ReadDriverStatus(); };
  const auto stall = [&rig] { return rig.drv.ReadStallGuard(); };
  // a bit no decoder acts on in every register (status byte: standstill)
  constexpr uint32_t kQuiet = 1U << 10U;
  constexpr uint32_t kNone = 0x100U;  // every word reads 0
  constexpr uint32_t kStatus = 0x80U; // the status byte keeps its value
  const int service_zero = BlankTransfers(rig, kNone, 0U, service);
  for (const uint32_t keep : {uint32_t{m::GSTAT}, uint32_t{m::DRV_STATUS}, uint32_t{m::RAMP_STAT},
                              uint32_t{m::ENC_STATUS}}) {
    CHECK_EQ(BlankTransfers(rig, keep, kQuiet, service), service_zero - 2); // no IOIN read
  }
  CHECK_EQ(BlankTransfers(rig, kStatus, 8U, service), service_zero - 2);
  const int snapshot_zero = BlankTransfers(rig, kNone, 0U, snapshot);
  for (const uint32_t keep :
       {uint32_t{m::GSTAT}, uint32_t{m::DRV_STATUS}, uint32_t{m::RAMP_STAT}, uint32_t{m::XACTUAL},
        uint32_t{m::VACTUAL}, uint32_t{m::TSTEP}, uint32_t{m::MSCNT}}) {
    CHECK_EQ(BlankTransfers(rig, keep, kQuiet, snapshot), snapshot_zero - 2);
  }
  CHECK_EQ(BlankTransfers(rig, kStatus, 8U, snapshot), snapshot_zero - 2);
  const int status_zero = BlankTransfers(rig, kNone, 0U, status);
  CHECK_EQ(BlankTransfers(rig, m::DRV_STATUS, kQuiet, status), status_zero - 2);
  const int stall_zero = BlankTransfers(rig, kNone, 0U, stall);
  CHECK_EQ(BlankTransfers(rig, m::DRV_STATUS, kQuiet, stall), stall_zero - 2);
}

TEST_CASE(
    "T263 the standstill check asks for position_reached only in position mode with a speed") {
  SpiRig rig(MoveConfig(0U));
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.MoveTo(2'000'000));
  rig.Run(300'000);
  CHECK_OK(rig.drv.SetMaxVelocity(0U)); // VSTART 0, VMAX 0: the move stops short (page 84)
  rig.Run(3'000'000);
  REQUIRE(rig.ChipOf().Standstill());
  REQUIRE(rig.ChipOf().Xactual() != rig.ChipOf().Xtarget());
  CHECK_OK(rig.drv.SetHomePosition(0)); // at rest, though the target is not reached
  // Velocity mode with the target left behind: no position_reached needed either.
  CHECK_ERR(rig.drv.MoveTo(2'000'000), Error::Refused); // VMAX 0 would never move
  CHECK_OK(rig.drv.SetMaxVelocity(MoveConfig(0U).ramp.vmax));
  CHECK_OK(rig.drv.MoveTo(2'000'000));
  rig.Run(300'000);
  CHECK_OK(rig.drv.Stop(StopKind::Ramp));
  rig.Run(3'000'000);
  REQUIRE(rig.ChipOf().Xactual() != rig.ChipOf().Xtarget());
  CHECK_OK(rig.drv.SetHomePosition(0));
}

TEST_CASE(
    "T264 MoveTo from a running velocity mode heads for the new target, and restores VSTART") {
  // From RunVelocity (VMAX not 0): position mode before XTARGET would head for the old target.
  UartRig rig(MoveConfig(0U));
  REQUIRE(rig.Up());
  rig.WireOf().xfer_us = 2'000U;
  CHECK_OK(rig.drv.MoveTo(-5'000'000)); // the old target, behind
  rig.Run(200'000);
  CHECK_OK(rig.drv.Stop(StopKind::Ramp));
  rig.Run(3'000'000);
  CHECK_OK(rig.drv.RunVelocity(60'000));
  rig.Run(1'000'000);
  rig.ChipOf().ResetWatch();
  CHECK_OK(rig.drv.MoveTo(rig.ChipOf().Xactual() + 5'000'000));
  rig.Run(500'000);
  CHECK_EQ(rig.ChipOf().stale_target_us, 0.0);
  // VSTART after a stop that zeroed it: written again by the next move.
  for (const bool via_disable : {false, true}) {
    SpiRig spi(MoveConfig(500U));
    REQUIRE(spi.Up());
    CHECK_OK(spi.drv.MoveTo(400'000));
    spi.Run(300'000);
    CHECK_OK(via_disable ? spi.drv.Disable() : spi.drv.Stop(StopKind::Immediate));
    CHECK_EQ(spi.ChipOf().Reg(m::VSTART), 0U);
    spi.Run(3'000'000);
    if (via_disable) {
      CHECK_OK(spi.drv.Enable());
    }
    CHECK_OK(spi.drv.MoveTo(0));
    CHECK_EQ(spi.ChipOf().Reg(m::VSTART), 500U);
  }
}

TEST_CASE("T265 a failed IOIN read of the stuck-line check is the read's error, never Ok") {
  SpiRig rig;
  REQUIRE(rig.Up());
  rig.ChipOf().blank_reads = true;
  rig.ChipOf().blank_keep = 0x100U; // every word and the status byte read 0
  const int before = rig.WireOf().transfers;
  CHECK_OK(rig.drv.Service()); // a live chip: IOIN is read and VERSION is right
  const int per_call = rig.WireOf().transfers - before;
  rig.WireOf().faults.fail_at = rig.WireOf().transfers + per_call - 2; // the IOIN read: last two
  CHECK_ERR(rig.drv.Service(), Error::Bus);
  rig.WireOf().faults.fail_at = -1;
  rig.ChipOf().blank_reads = false;
}

TEST_CASE("T266 SetHomePosition writes the hold mode, XACTUAL and XTARGET, in that order") {
  // At rest at the target in position mode: XACTUAL written first would start a move towards the
  // old target while the hold write is still on the line, and the hold mode would keep that speed.
  UartRig rig(MoveConfig(0U));
  REQUIRE(rig.Up());
  rig.WireOf().xfer_us = 2'000U;
  CHECK_OK(rig.drv.MoveTo(100'000));
  rig.Run(4'000'000);
  REQUIRE(rig.ChipOf().Xactual() == 100'000);
  rig.ChipOf().ResetWatch();
  CHECK_OK(rig.drv.SetHomePosition(0));
  rig.Run(1'000'000);
  CHECK_EQ(rig.ChipOf().Xactual(), 0);
  CHECK_EQ(rig.ChipOf().Xtarget(), 0);
  CHECK_EQ(rig.ChipOf().max_x, 100'000); // never moved
  CHECK_EQ(rig.ChipOf().min_x, 0);
  CHECK(rig.ChipOf().Standstill());
}

TEST_CASE("T267 RunVelocity from a running position move has the new speed after two writes") {
  UartRig rig(MoveConfig(0U));
  REQUIRE(rig.Up());
  rig.WireOf().xfer_us = 2'000U;
  CHECK_OK(rig.drv.MoveTo(50'000'000));
  rig.Run(8'000); // still accelerating, far below VMAX
  const double earlier = rig.ChipOf().VelocityHz();
  rig.Run(2'000);
  const double before = rig.ChipOf().VelocityHz();
  const double per_exchange = before - earlier; // the gain of one 2 ms exchange
  REQUIRE(per_exchange > 0.0);
  rig.ChipOf().ResetWatch();
  CHECK_OK(rig.drv.RunVelocity(1'000));
  rig.Run(100'000);
  // The mode and the VMAX write go by before the lower speed applies; VSTART before VMAX would add
  // one more exchange of acceleration.
  CHECK(rig.ChipOf().max_v <= (before + (2.5 * per_exchange)));
  CHECK(rig.ChipOf().VelocityHz() < before);
}

TEST_CASE("T268 RunVelocity during the pause of a HoldTarget stop never runs the run-back way") {
  // VACTUAL 0 in position mode is a pause or a reversal of a pending run-back, not the asked
  // direction: the mode goes first, so the run-back never starts at the new speed. A pause that
  // ends before the VACTUAL read and the mode write are through (two 2 ms exchanges) starts the
  // run-back by itself; no write order can stop that, so only longer pauses count.
  for (const uint32_t vstart : {0U, 20'000U}) {
    int backwards = 0;
    for (uint32_t offset_us = 0U; offset_us < 260'000U; offset_us += 2'000U) {
      Config c = MoveConfig(vstart);
      c.ramp.zero_wait = 6'000U; // about 0.26 s
      UartRig rig(c);
      REQUIRE(rig.Up());
      rig.WireOf().xfer_us = 2'000U;
      CHECK_OK(rig.drv.MoveTo(50'000'000));
      rig.Run(1'000'000);
      CHECK_OK(rig.drv.Stop(StopKind::HoldTarget)); // overshoot, pause, run back (negative)
      for (int i = 0; (i < 4'000) && !rig.ChipOf().ZeroWaitActive(); ++i) {
        rig.Run(100);
      }
      REQUIRE(rig.ChipOf().ZeroWaitActive());
      rig.Run(offset_us);
      if (!rig.ChipOf().ZeroWaitActive()) {
        continue;
      }
      const bool long_pause = rig.ChipOf().ZeroWaitLeftUs() > 4'000.0;
      const int32_t at = rig.ChipOf().Xactual();
      rig.ChipOf().ResetWatch();
      CHECK_OK(rig.drv.RunVelocity(60'000)); // positive: away from the run-back
      rig.Run(200'000);
      backwards += (long_pause && (rig.ChipOf().min_x < at)) ? 1 : 0;
    }
    CHECK_EQ(backwards, 0);
  }
}

TEST_CASE("T269 the model: a HoldTarget stop at speed overshoots and returns to the captured "
          "position") {
  // The model's own run-back (page 83 c): the tests of the start-up and HoldTarget stops rely on
  // it.
  SpiRig rig(MoveConfig(0U));
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.RunVelocity(60'000));
  rig.Run(1'000'000);
  REQUIRE(rig.ChipOf().VelocityHz() > 1'000.0);
  rig.ChipOf().ResetWatch();
  CHECK_OK(rig.drv.Stop(StopKind::HoldTarget));
  const int32_t captured = rig.ChipOf().Xtarget();
  rig.Run(5'000'000);
  // about 43 kHz down at DMAX (1.3 MHz/s at the model's clock): some 700 microsteps past it
  CHECK(rig.ChipOf().max_x > (captured + 300));
  CHECK(rig.ChipOf().min_v < -1'000.0); // and ran back
  CHECK_EQ(rig.ChipOf().Xactual(), captured);
  CHECK(rig.ChipOf().Standstill());
}

TEST_CASE("T270 the model: a plain move with D1 below DMAX stops at its target") {
  // Two-phase deceleration: DMAX down to V1, then D1 (page 40); no overshoot on a plain move.
  Config c = MoveConfig(0U);
  c.ramp.dmax = 20'000;
  c.ramp.d1 = 2'000;
  c.ramp.v1 = 50'000;
  SpiRig rig(c);
  REQUIRE(rig.Up());
  rig.ChipOf().ResetWatch();
  CHECK_OK(rig.drv.MoveTo(200'000));
  rig.Run(20'000'000);
  CHECK(rig.ChipOf().max_x <= 200'000);
  CHECK(rig.ChipOf().min_v >= 0.0);
  CHECK_EQ(rig.ChipOf().Xactual(), 200'000);
}

TEST_CASE("T271 RunVelocity: a failed VACTUAL read (hold mode) sends nothing") {
  SpiRig rig(MoveConfig(0U));
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.MoveTo(50'000'000));
  rig.Run(200'000);
  CHECK_OK(rig.drv.Hold());
  const uint32_t vmax = rig.ChipOf().Reg(m::VMAX);
  rig.WireOf().faults.fail_at = rig.WireOf().transfers; // the VACTUAL read only
  CHECK_ERR(rig.drv.RunVelocity(-60'000), Error::Bus);
  rig.WireOf().faults.fail_at = -1;
  CHECK_EQ(rig.ChipOf().Reg(m::RAMPMODE), 3U); // still hold mode
  CHECK_EQ(rig.ChipOf().Reg(m::VMAX), vmax);
  // From position mode there is no read: the mode is the first write.
  CHECK_OK(rig.drv.MoveTo(-50'000'000));
  const int before = rig.WireOf().transfers;
  CHECK_OK(rig.drv.RunVelocity(-60'000));
  CHECK_EQ(rig.WireOf().transfers - before, 3); // three writes, no read
}

TEST_CASE("T272 RunVelocity from hold mode in the running direction lowers the speed first, and a "
          "start from rest begins at VSTART") {
  UartRig rig(MoveConfig(0U));
  REQUIRE(rig.Up());
  rig.WireOf().xfer_us = 2'000U;
  CHECK_OK(rig.drv.MoveTo(50'000'000));
  rig.Run(8'000); // accelerating, far below VMAX
  CHECK_OK(rig.drv.Hold());
  const double held = rig.ChipOf().VelocityHz();
  REQUIRE(held > 0.0);
  rig.ChipOf().ResetWatch();
  CHECK_OK(rig.drv.RunVelocity(1'000));
  rig.Run(100'000);
  CHECK(rig.ChipOf().max_v <= held); // hold mode keeps the speed until the lower VMAX is in
  // From rest after Stop(Immediate) (VSTART 0 in the chip): VSTART is written before VMAX.
  UartRig rest(MoveConfig(2'000U));
  REQUIRE(rest.Up());
  rest.WireOf().xfer_us = 2'000U;
  CHECK_OK(rest.drv.RunVelocity(60'000));
  rest.Run(300'000);
  CHECK_OK(rest.drv.Stop(StopKind::Immediate));
  rest.Run(3'000'000);
  REQUIRE(rest.ChipOf().Standstill());
  rest.ChipOf().ResetWatch();
  CHECK_OK(rest.drv.RunVelocity(-60'000));
  rest.Run(100'000);
  // VSTART 2000 is about 1.4 kHz at the model's clock: the first speed is at least that
  CHECK(rest.ChipOf().first_v >= (2'000.0 * 12'000'000.0 / 16'777'216.0));
}

TEST_CASE("T273 a position move a stall holds is Busy for SetHomePosition until Stop or Hold") {
  for (int how = 0; how < 3; ++how) {
    SpiRig rig(StallConfig());
    REQUIRE(rig.Up());
    CHECK_OK(rig.drv.MoveTo(5'000'000));
    rig.Run(500'000);
    rig.ChipOf().InjectStall(true);
    rig.Run(20'000);
    REQUIRE(rig.ChipOf().Standstill());
    rig.ChipOf().InjectStall(false);
    CHECK_ERR(rig.drv.SetHomePosition(0), Error::Busy); // the target is not reached
    if (how == 0) {
      CHECK_OK(rig.drv.Stop(StopKind::Ramp));
    } else if (how == 1) {
      CHECK_OK(rig.drv.Stop(StopKind::HoldTarget));
    } else {
      CHECK_OK(rig.drv.Hold());
    }
    rig.Run(10'000);
    CHECK_OK(rig.drv.SetHomePosition(0)); // a homing loop gets out
  }
}

namespace {
/// The largest fall of XACTUAL below its running peak over `us` microseconds, in 100 us slices.
template <typename R>
int32_t LargestDrop(R& rig, uint32_t us) {
  int32_t peak = rig.ChipOf().Xactual();
  int32_t drop = 0;
  for (uint32_t t = 0U; t < us; t += 100U) {
    rig.Run(100);
    peak = std::max(peak, rig.ChipOf().Xactual());
    drop = std::max(drop, peak - rig.ChipOf().Xactual());
  }
  return drop;
}
} // namespace

TEST_CASE("T274 the model: velocity mode passes through 0 at AMAX when it reverses or stops") {
  // Page 83: velocity mode accelerates and decelerates with AMAX; it never flips at full speed.
  SpiRig rig(MoveConfig(0U));
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.RunVelocity(-60'000));
  rig.Run(1'000'000);
  const int32_t x0 = rig.ChipOf().Xactual();
  rig.ChipOf().ResetWatch();
  CHECK_OK(rig.drv.Stop(StopKind::Ramp));
  rig.Run(3'000'000);
  CHECK_EQ(rig.ChipOf().max_x, x0);           // never the other way
  CHECK(rig.ChipOf().Xactual() < (x0 - 300)); // about 700 microsteps of deceleration
  SpiRig rev(MoveConfig(0U));
  REQUIRE(rev.Up());
  CHECK_OK(rev.drv.RunVelocity(-60'000));
  rev.Run(1'000'000);
  const double v0 = rev.ChipOf().VelocityHz();
  CHECK_OK(rev.drv.RunVelocity(60'000));
  rev.Run(1'000);
  CHECK(rev.ChipOf().VelocityHz() < 0.0); // still running down towards 0
  CHECK(rev.ChipOf().VelocityHz() > v0);
  rev.Run(1'000'000);
  CHECK(rev.ChipOf().VelocityHz() > 0.0);
}

TEST_CASE("T275 the model: every legal position move reaches its target") {
  // Below VSTOP the ramp goes on at VSTOP to the target (page 82); it never pauses short.
  int stuck = 0;
  for (const uint32_t a1 : {500U, 5'000U, 65'535U}) {
    for (const uint32_t d1 : {50U, 700U, 5'000U}) {
      for (const int32_t target : {3'000, 123'457}) {
        Config c = BaseConfig();
        c.ramp = RampProfile{};
        c.switches = Switches{};
        c.ramp.a1 = a1;
        c.ramp.amax = a1;
        c.ramp.d1 = d1;
        c.ramp.dmax = d1;
        SpiRig rig(c);
        REQUIRE(rig.Up());
        CHECK_OK(rig.drv.MoveTo(target));
        bool reached = false;
        for (int i = 0; (i < 600) && !reached; ++i) {
          rig.Run(100'000);
          reached = (rig.ChipOf().Xactual() == target) && rig.ChipOf().Standstill();
        }
        stuck += reached ? 0 : 1;
      }
    }
  }
  CHECK_EQ(stuck, 0);
}

TEST_CASE("T276 a TZEROWAIT pause is Busy: after a VMAX 0 stop and after an arrival") {
  Config c = MoveConfig(0U);
  c.ramp.zero_wait = 6'000U; // about 0.26 s
  SpiRig rig(c);
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.MoveTo(5'000'000));
  rig.Run(500'000);
  CHECK_OK(rig.drv.SetMaxVelocity(0U)); // a joystick stop (page 84): position mode, VMAX 0
  for (int i = 0; (i < 100'000) && !rig.ChipOf().ZeroWaitActive(); ++i) {
    rig.Run(100);
  }
  REQUIRE(rig.ChipOf().ZeroWaitActive());
  CHECK_ERR(rig.drv.SetHomePosition(0), Error::Busy);
  rig.Run(400'000);
  CHECK_OK(rig.drv.SetHomePosition(0));
  // A move that arrives pauses too (pages 41, 83): Busy until the pause is over.
  CHECK_OK(rig.drv.SetMaxVelocity(c.ramp.vmax));
  CHECK_OK(rig.drv.MoveTo(20'000));
  for (int i = 0; (i < 100'000) && !rig.ChipOf().ZeroWaitActive(); ++i) {
    rig.Run(100);
  }
  REQUIRE(rig.ChipOf().ZeroWaitActive());
  REQUIRE(rig.ChipOf().Xactual() == 20'000);
  CHECK_ERR(rig.drv.SetHomePosition(0), Error::Busy);
  rig.Run(400'000);
  CHECK_OK(rig.drv.SetHomePosition(0));
  // A high VSTOP (several microsteps per model step) arrives within one step: it pauses as well.
  Config fast = MoveConfig(20'000U);
  fast.ramp.zero_wait = 6'000U;
  SpiRig hop(fast);
  REQUIRE(hop.Up());
  CHECK_OK(hop.drv.MoveTo(20'000));
  for (int i = 0; (i < 10'000) && !hop.ChipOf().ZeroWaitActive(); ++i) {
    hop.Run(1'000); // 250 us model steps: about 3.6 microsteps each at VSTOP
  }
  CHECK(hop.ChipOf().ZeroWaitActive());
  CHECK_EQ(hop.ChipOf().Xactual(), 20'000);
}

TEST_CASE("T277 RunVelocity during a HoldTarget overshoot never lets it turn back (TZEROWAIT 0)") {
  // From position mode the mode is the first write: no VACTUAL read lets the ramp turn back first.
  int backwards = 0;
  for (uint32_t offset_us = 0U; offset_us < 60'000U; offset_us += 1'000U) {
    UartRig rig(MoveConfig(0U));
    REQUIRE(rig.Up());
    rig.WireOf().xfer_us = 2'000U;
    CHECK_OK(rig.drv.MoveTo(50'000'000));
    rig.Run(1'000'000);
    CHECK_OK(rig.drv.Stop(StopKind::HoldTarget)); // decelerates past the captured position
    rig.Run(offset_us);
    if (rig.ChipOf().VelocityHz() <= 0.0) {
      continue;
    }
    CHECK_OK(rig.drv.RunVelocity(60'000)); // the direction it still moves
    backwards += (LargestDrop(rig, 300'000U) > 0) ? 1 : 0;
  }
  CHECK_EQ(backwards, 0);
}

TEST_CASE("T278 SetMaxVelocity keeps VMAX at or above VSTART, and VMAX 0 writes VSTART 0 first") {
  SpiRig rig(MoveConfig(500U));
  REQUIRE(rig.Up());
  const int before = rig.WireOf().transfers;
  CHECK_ERR(rig.drv.SetMaxVelocity(499U), Error::InvalidArgument); // below VSTART (page 40)
  CHECK_EQ(rig.WireOf().transfers, before);
  CHECK_OK(rig.drv.SetMaxVelocity(500U));
  CHECK_EQ(rig.ChipOf().Reg(m::VMAX), 500U);
  CHECK_OK(rig.drv.MoveTo(1'000'000));
  rig.Run(200'000);
  CHECK_OK(rig.drv.SetMaxVelocity(0U)); // a stop: VSTART 0, then VMAX 0 (page 84)
  CHECK_EQ(rig.ChipOf().Reg(m::VSTART), 0U);
  CHECK_EQ(rig.ChipOf().Reg(m::VMAX), 0U);
  rig.Run(2'000'000);
  CHECK(rig.ChipOf().Standstill());
}

namespace {
/// Hz of a velocity register value and Hz/s of an acceleration register value at the model's
/// 12 MHz clock (page 40).
double VelocityHzOf(uint32_t v) {
  return static_cast<double>(v) * 12'000'000.0 / 16'777'216.0;
}
double AccelHzOf(uint32_t a) {
  return static_cast<double>(a) * 144e12 / 2'199'023'255'552.0;
}
/// The documented run-back bound of `RunVelocity()` (inc/tmc5160.hpp) for `t_s` seconds.
double RunBackBound(const RampProfile& r, double t_s) {
  const double vstart = VelocityHzOf(r.vstart);
  const double a1 = AccelHzOf(r.a1);
  const double amax = AccelHzOf(r.amax);
  const double a = ((r.v1 > 0U) && (r.vstart < r.v1)) ? std::max(a1, amax) : amax;
  const double v = vstart + (a * t_s);
  return (vstart * t_s) + ((a * t_s * t_s) / 2.0) + ((v * v) / (2.0 * amax));
}
} // namespace

TEST_CASE("T279 the run-back of RunVelocity during a pause stays within the documented bound") {
  // One 8-byte UART frame at 9600 baud is 8.3 ms. V1 0 runs the run-back at AMAX (page 40).
  const uint32_t xfer_us = 8'300U;
  std::array<RampProfile, 5> ramps{};
  for (RampProfile& r : ramps) {
    r = MoveConfig(0U).ramp;
    r.zero_wait = 6'000U;
  }
  ramps[0].v1 = 0U; // A1 unused: AMAX all the way
  ramps[0].a1 = 100U;
  ramps[0].amax = 60'000U;
  ramps[0].dmax = 60'000U;
  ramps[0].d1 = 60'000U;
  ramps[2].vstart = 20'000U; // below V1: A1
  ramps[2].vstop = 20'000U;
  ramps[3].vstart = 100'000U; // above V1: AMAX
  ramps[3].vstop = 100'000U;
  ramps[3].vmax = 400'000U;
  ramps[4].a1 = 60'000U; // A1 above AMAX below V1: the run-back starts at A1
  ramps[4].amax = 20'000U;
  ramps[4].v1 = 200'000U;
  ramps[4].zero_wait = 12'000U; // about 0.51 s
  for (const RampProfile& ramp : ramps) {
    Config c = MoveConfig(0U);
    c.ramp = ramp;
    double worst = 0.0;
    const uint32_t first_us = (ramp.zero_wait > 6'000U) ? 400'000U : 0U; // the end of the pause
    const uint32_t last_us = (ramp.zero_wait > 6'000U) ? 700'000U : 300'000U;
    for (uint32_t offset_us = first_us; offset_us < last_us; offset_us += 2'000U) {
      UartRig rig(c);
      REQUIRE(rig.Up());
      rig.WireOf().xfer_us = xfer_us;
      CHECK_OK(rig.drv.MoveTo(50'000'000));
      rig.Run(1'000'000);
      CHECK_OK(rig.drv.Stop(StopKind::HoldTarget));
      for (int i = 0; (i < 40'000) && !rig.ChipOf().ZeroWaitActive(); ++i) {
        rig.Run(100);
      }
      rig.Run(offset_us);
      if (!rig.ChipOf().ZeroWaitActive()) {
        break;
      }
      const int32_t at = rig.ChipOf().Xactual();
      rig.ChipOf().ResetWatch();
      CHECK_OK(rig.drv.RunVelocity(60'000));
      rig.Run(300'000);
      worst = std::max(worst, static_cast<double>(at - rig.ChipOf().min_x));
    }
    const double bound = RunBackBound(ramp, xfer_us * 1e-6);
    CHECK(worst <= (bound + 2.0));
    CHECK(worst >= (0.9 * bound)); // and the bound is not an empty promise
  }
}

TEST_CASE("T280 RunVelocity from hold mode at rest takes the new speed first") {
  // Hold mode at VACTUAL 0 keeps 0: the old VMAX must never apply (a slow request is slow).
  UartRig rig(MoveConfig(0U));
  REQUIRE(rig.Up());
  rig.WireOf().xfer_us = 8'300U;
  CHECK_OK(rig.drv.MoveTo(20'000));
  rig.Run(3'000'000);
  CHECK_OK(rig.drv.SetHomePosition(0)); // hold mode at rest, VMAX still the profile's
  rig.ChipOf().ResetWatch();
  CHECK_OK(rig.drv.RunVelocity(1'000));
  rig.Run(300'000);
  CHECK(rig.ChipOf().max_v <= (VelocityHzOf(1'000U) + 1.0));
  CHECK(rig.ChipOf().VelocityHz() > 0.0);
}

TEST_CASE("T281 the write order of RunVelocity for each starting mode") {
  const auto order = [](SpiRig& rig, int32_t v) {
    rig.ChipOf().ClearWriteLog();
    CHECK_OK(rig.drv.RunVelocity(v));
    std::array<uint8_t, 3> got{rig.ChipOf().write_log[0], rig.ChipOf().write_log[1],
                               rig.ChipOf().write_log[2]};
    rig.ChipOf().write_log_n = 8U;
    return got;
  };
  using Order = std::array<uint8_t, 3>;
  const Order speed_first{m::VMAX, m::VSTART, m::RAMPMODE};
  const Order mode_first{m::RAMPMODE, m::VSTART, m::VMAX};
  const Order from_position{m::RAMPMODE, m::VMAX, m::VSTART};
  SpiRig rig(MoveConfig(0U));
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.MoveTo(50'000'000));
  rig.Run(300'000);
  CHECK(order(rig, 60'000) == from_position); // position mode: no read, the mode first
  rig.Run(300'000);
  CHECK(order(rig, 30'000) == speed_first); // the same velocity mode
  rig.Run(300'000);
  CHECK(order(rig, -30'000) == mode_first); // the other velocity mode
  rig.Run(2'000'000);
  CHECK_OK(rig.drv.Hold());                  // hold mode, running negative
  CHECK(order(rig, -10'000) == speed_first); // the running way
  CHECK_OK(rig.drv.Hold());
  CHECK(order(rig, 10'000) == mode_first); // the other way
}

TEST_CASE("T282 SetMaxVelocity checks the profile's VSTART, and a VMAX of 0 is refused by MoveTo") {
  SpiRig rig(MoveConfig(500U));
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.Stop(StopKind::Immediate)); // VSTART 0 in the chip, 500 in the profile
  CHECK_ERR(rig.drv.SetMaxVelocity(100U), Error::InvalidArgument); // MoveTo writes VSTART 500
  CHECK_OK(rig.drv.SetMaxVelocity(0U));
  CHECK_ERR(rig.drv.SetMaxVelocity(200U), Error::InvalidArgument);
  CHECK_ERR(rig.drv.MoveTo(10'000), Error::Refused); // VMAX 0: it would never arrive
  CHECK_OK(rig.drv.SetMaxVelocity(40'000U));
  CHECK_OK(rig.drv.MoveTo(10'000));
  CHECK_EQ(rig.ChipOf().Reg(m::VMAX), 40'000U);
  CHECK_EQ(rig.ChipOf().Reg(m::VSTART), 500U);
  rig.Run(3'000'000);
  CHECK_EQ(rig.ChipOf().Xactual(), 10'000);
}

TEST_CASE("T283 the model: a lower VMAX in position mode is followed with AMAX, VSTOP only at the "
          "target") {
  // Page 84: lowering VMAX (to 0 included) ramps down with A1 and AMAX; DMAX, D1 and VSTOP belong
  // to the stop at the target (pages 83, 84).
  Config c = MoveConfig(0U);
  c.ramp.v1 = 0U; // AMAX only
  c.ramp.dmax = 2'000U;
  c.ramp.d1 = 2'000U;
  SpiRig rig(c);
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.MoveTo(50'000'000));
  rig.Run(1'000'000);
  const double v = rig.ChipOf().VelocityHz();
  const int32_t from = rig.ChipOf().Xactual();
  CHECK_OK(rig.drv.SetMaxVelocity(0U));
  rig.Run(3'000'000);
  REQUIRE(rig.ChipOf().Standstill());
  const double at_amax = (v * v) / (2.0 * AccelHzOf(c.ramp.amax));
  CHECK(static_cast<double>(rig.ChipOf().Xactual() - from) <= (1.2 * at_amax)); // not DMAX's 10x
  // Velocity mode with a high VSTOP runs down to 0 at AMAX: no jump from VSTOP to 0.
  SpiRig vel(MoveConfig(20'000U));
  REQUIRE(vel.Up());
  CHECK_OK(vel.drv.RunVelocity(60'000));
  vel.Run(1'000'000);
  CHECK_OK(vel.drv.Stop(StopKind::Ramp));
  double last = vel.ChipOf().VelocityHz();
  double jump = 0.0;
  for (int i = 0; i < 20'000; ++i) {
    vel.Run(100);
    jump = std::max(jump, last - vel.ChipOf().VelocityHz());
    last = vel.ChipOf().VelocityHz();
  }
  CHECK(jump <= (1.5 * AccelHzOf(MoveConfig(0U).ramp.amax) * 100e-6));
}

TEST_CASE("T284 a SetMaxVelocity that fails keeps the profile's VMAX and VSTART") {
  SpiRig rig(MoveConfig(500U));
  REQUIRE(rig.Up());
  // VMAX 0 with VSTART 500: VSTART 0 goes out, then the VMAX write fails.
  rig.WireOf().faults.fail_at = rig.WireOf().transfers + 1;
  CHECK_ERR(rig.drv.SetMaxVelocity(0U), Error::Bus);
  rig.WireOf().faults.fail_at = -1;
  CHECK_EQ(rig.ChipOf().Reg(m::VSTART), 0U);
  CHECK_OK(rig.drv.MoveTo(10'000));            // not Refused: the kept VMAX is still the profile's
  CHECK_EQ(rig.ChipOf().Reg(m::VSTART), 500U); // restored by the move
  CHECK_EQ(rig.ChipOf().Reg(m::VMAX), MoveConfig(500U).ramp.vmax);
  rig.Run(3'000'000);
  CHECK_EQ(rig.ChipOf().Xactual(), 10'000);
}

TEST_CASE("T285 MoveBy refuses before it reads, as MoveTo does") {
  SpiRig rig(MoveConfig(0U));
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.SetMaxVelocity(0U));
  const int before = rig.WireOf().transfers;
  CHECK_ERR(rig.drv.MoveBy(1'000), Error::Refused);
  CHECK_EQ(rig.WireOf().transfers, before); // no XACTUAL read
}
