/**
 * @file test_mutants.cpp
 * @brief Tests that pin behaviour a mutation of the driver would change: reply validity on UART,
 * lost writes, the order of the current registers, limits, register bit positions and the stall
 * stop (T200 to T245; the stop and standstill tests continue in test_stops.cpp).
 */
#include <algorithm>
#include <bit>
#include <cstdint>

#include "fixture.hpp"

using namespace tmc5160;
using namespace tmc5160_test;
namespace m = tmc5160_test::m;

TEST_CASE("T200 UART: a reply that keeps failing its CRC is Crc after three attempts") {
  UartRig rig;
  REQUIRE(rig.Up());
  const int before = rig.WireOf().transfers;
  rig.WireOf().faults.corrupt_from = before;
  CHECK_ERR(rig.drv.ReadStepTime(), Error::Crc);
  CHECK_EQ(rig.WireOf().transfers - before, 3); // one try and two retries
}

TEST_CASE("T201 UART: a reply for another register or another address is refused") {
  for (const uint8_t index : {uint8_t{1}, uint8_t{2}}) { // master address, register
    UartRig rig;
    REQUIRE(rig.Up());
    const int before = rig.WireOf().transfers;
    rig.WireOf().faults.patch_from = before;
    rig.WireOf().faults.patch_index = index;
    rig.WireOf().faults.patch_xor = 0x01;
    CHECK_ERR(rig.drv.ReadStepTime(), Error::Bus);
    CHECK_EQ(rig.WireOf().transfers - before, 3);
  }
}

TEST_CASE("T202 UART: a lost write is caught by Enable and never hidden by a later check") {
  UartRig rig;
  REQUIRE(rig.drv.EnsureInitialized().ok());
  rig.WireOf().faults.corrupt_tx_at = rig.WireOf().transfers; // the chip drops the next request
  CHECK_OK(rig.drv.SetCurrent(500, 150));                     // UART has no reply: it looks fine
  CHECK_ERR(rig.drv.Enable(), Error::VerifyFailed);
  CHECK(!rig.drv.IsEnabled());
  CHECK(!rig.drv.IsInitialized());
}

TEST_CASE("T203 UART: a Verify::Counter mismatch is reported again by the next Enable") {
  UartRig rig;
  REQUIRE(rig.drv.EnsureInitialized().ok());
  rig.WireOf().faults.corrupt_tx_at = rig.WireOf().transfers;
  CHECK_ERR(rig.drv.SetCurrent(500, 150, Verify::Counter), Error::VerifyFailed);
  CHECK_ERR(rig.drv.Enable(), Error::VerifyFailed); // the counter is not re-synchronised
  CHECK(rig.ChipOf().peak_coil_ma <= 1000.0);
}

TEST_CASE("T204 UART: Disable and Stop(Emergency) prove that TOFF = 0 arrived") {
  {
    UartRig rig;
    REQUIRE(rig.Up());
    rig.WireOf().faults.corrupt_tx_at = rig.WireOf().transfers;
    CHECK_ERR(rig.drv.Disable(), Error::VerifyFailed);
    CHECK(!rig.drv.IsEnabled());
    CHECK(!rig.ChipOf().DrvEnnActive()); // the pin dropped first
  }
  {
    UartRig rig;
    REQUIRE(rig.Up());
    rig.WireOf().faults.corrupt_tx_at = rig.WireOf().transfers;
    CHECK_ERR(rig.drv.Stop(StopKind::Emergency), Error::VerifyFailed);
    CHECK(!rig.drv.IsEnabled());
  }
}

TEST_CASE("T205 an emergency stop drops DRV_ENN on both buses") {
  SpiRig spi;
  REQUIRE(spi.Up());
  CHECK(spi.ChipOf().DrvEnnActive());
  CHECK_OK(spi.drv.Stop(StopKind::Emergency));
  CHECK(!spi.ChipOf().DrvEnnActive());
  UartRig uart;
  REQUIRE(uart.Up());
  CHECK_OK(uart.drv.Stop(StopKind::Emergency));
  CHECK(!uart.ChipOf().DrvEnnActive());
}

TEST_CASE("T206 a failed write between the two current registers never exceeds both currents") {
  // A 2 A motor: the plan moves GLOBALSCALER as well as IRUN, so the write order matters.
  const Config big = MakeConfig(Motor{.rated_rms_ma = 2000, .coil_resistance_mohm = 3000},
                                Board{.rsense_mohm = 100, .supply_mv = 24'000});
  const uint32_t asks[] = {60, 500, 1200, 1800};
  for (const uint32_t from : asks) {
    for (const uint32_t to : asks) {
      if (from == to) {
        continue;
      }
      SpiRig target(big);
      REQUIRE(target.Up());
      REQUIRE(target.drv.SetCurrent(to, to / 3).ok());
      target.Run(1000);
      const double new_ma = std::max(target.ChipOf().RunMa(), target.ChipOf().HoldMa());
      SpiRig rig(big);
      REQUIRE(rig.Up());
      REQUIRE(rig.drv.SetCurrent(from, from / 3).ok());
      rig.Run(1000);
      const double old_ma = std::max(rig.ChipOf().RunMa(), rig.ChipOf().HoldMa());
      rig.ChipOf().peak_coil_ma = 0.0;
      rig.WireOf().faults.fail_at = rig.WireOf().transfers + 1; // the second register fails
      CHECK_ERR(rig.drv.SetCurrent(to, to / 3), Error::Bus);
      rig.Run(1000);
      CHECK(rig.ChipOf().peak_coil_ma <= (std::max(old_ma, new_ma) + 0.5));
    }
  }
}

TEST_CASE("T206b the register that lowers the current is written first") {
  const Config big = MakeConfig(Motor{.rated_rms_ma = 2000, .coil_resistance_mohm = 3000},
                                Board{.rsense_mohm = 100, .supply_mv = 24'000});
  // Rising: the new scaler alone gives less than the new IRUN alone, so the scaler goes first.
  SpiRig up(big);
  REQUIRE(up.Up());
  REQUIRE(up.drv.SetCurrent(60, 20).ok());
  const uint32_t gs_old = up.ChipOf().Reg(m::GLOBAL_SCALER);
  const uint32_t ih_old = up.ChipOf().Reg(m::IHOLD_IRUN);
  up.WireOf().faults.fail_at = up.WireOf().transfers + 1;
  CHECK_ERR(up.drv.SetCurrent(1800, 600), Error::Bus);
  CHECK_EQ(up.ChipOf().Reg(m::IHOLD_IRUN), ih_old);
  CHECK(up.ChipOf().Reg(m::GLOBAL_SCALER) != gs_old);
  // Falling: the new IRUN goes first and the scaler stays old.
  SpiRig down(big);
  REQUIRE(down.Up());
  REQUIRE(down.drv.SetCurrent(1800, 600).ok());
  const uint32_t gs_hi = down.ChipOf().Reg(m::GLOBAL_SCALER);
  const uint32_t ih_hi = down.ChipOf().Reg(m::IHOLD_IRUN);
  down.WireOf().faults.fail_at = down.WireOf().transfers + 1;
  CHECK_ERR(down.drv.SetCurrent(60, 20), Error::Bus);
  CHECK_EQ(down.ChipOf().Reg(m::GLOBAL_SCALER), gs_hi);
  CHECK(down.ChipOf().Reg(m::IHOLD_IRUN) != ih_hi);
}

TEST_CASE("T207 the current ceiling is the rated current less the 5 percent tolerance") {
  SpiRig rig;
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.SetCurrent(952, 300));
  for (const uint32_t ask : {953U, 960U, 1000U, 1040U}) {
    const uint32_t before = rig.ChipOf().writes;
    CHECK_ERR(rig.drv.SetCurrent(ask, 100), Error::LimitExceeded);
    CHECK_EQ(rig.ChipOf().writes, before);
  }
}

TEST_CASE("T208 a negative velocity reads back negative over its full 24 bits") {
  SpiRig rig;
  REQUIRE(rig.Up());
  RampProfile ramp = rig.drv.GetConfig().ramp;
  ramp.amax = 65'535;
  REQUIRE(rig.drv.SetRamp(ramp).ok());
  CHECK_OK(rig.drv.RunVelocity(-6'000'000)); // above 2^22: bit 23 and bit 22 differ
  rig.Run(3'000'000);
  const auto v = rig.drv.ReadVelocity();
  REQUIRE(v.ok());
  CHECK(v.value < -5'900'000);
  CHECK(v.value > -6'100'000);
}

TEST_CASE("T209 Service never releases a stall stop; only ReleaseStall does") {
  Config c = BaseConfig();
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
  const auto events = rig.drv.Service();
  REQUIRE(events.ok());
  CHECK(events.value.ramp.event_stop_stall);
  rig.Run(1'000'000);
  CHECK_EQ(rig.ChipOf().VelocityHz(), 0.0); // still stopped
  const auto still = rig.drv.ReadRampStatus();
  REQUIRE(still.ok());
  CHECK(still.value.event_stop_stall);
  CHECK_ERR(rig.drv.ClearRampEvents(0x40), Error::Refused);
  CHECK_OK(rig.drv.ReleaseStall());
  CHECK_EQ(rig.ChipOf().Reg(m::RAMPMODE), 3U); // hold: the old target is gone
  rig.Run(1'000'000);
  CHECK_EQ(rig.ChipOf().VelocityHz(), 0.0);
  const auto released = rig.drv.ReadRampStatus();
  REQUIRE(released.ok());
  CHECK(!released.value.event_stop_stall);
}

TEST_CASE("T210 a short at start-up is reported and never started over") {
  SpiRig rig;
  rig.ChipOf().InjectShort();
  CHECK_ERR(rig.drv.EnsureInitialized(), Error::DeviceFault);
  CHECK(!rig.drv.IsInitialized());
}

TEST_CASE("T211 hysteresis start plus end is limited to 18 register counts") {
  SpiRig rig;
  REQUIRE(rig.Up());
  Chopper c;
  c.hstrt = 7;
  c.hend = 11;
  CHECK_OK(rig.drv.SetChopper(c));
  c.hend = 12;
  CHECK_ERR(rig.drv.SetChopper(c), Error::InvalidArgument);
}

TEST_CASE("T212 a custom microstep table is written only at MSCNT = 0") {
  SpiRig rig;
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.MoveTo(3000));
  rig.Run(300'000);
  const auto mscnt = rig.drv.ReadMicrostepCounter();
  REQUIRE(mscnt.ok());
  REQUIRE(mscnt.value != 0U);
  const uint32_t before = rig.ChipOf().writes;
  CHECK_ERR(rig.drv.SetMicrostepTable(rig.drv.GetConfig().table), Error::Refused);
  CHECK_EQ(rig.ChipOf().writes, before);
}

TEST_CASE("T212b a start-up with a custom table is refused while the motor is mid-turn") {
  SpiRig rig;
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.MoveTo(3000));
  rig.Run(300'000);
  Config c = BaseConfig();
  c.custom_table = true;
  TMC5160<FullSpiPort> again(rig.port, c);
  CHECK_ERR(again.EnsureInitialized(), Error::Refused);
}

TEST_CASE("T213 UART: a failed write waits out the recovery time and is not retried") {
  UartRig rig;
  REQUIRE(rig.Up());
  const uint64_t before = rig.WireOf().delayed_us;
  const int sent = rig.WireOf().transfers;
  rig.WireOf().faults.fail_at = sent;
  CHECK_ERR(rig.drv.SetCurrent(500, 150), Error::Bus);
  CHECK_EQ(rig.WireOf().transfers - sent, 1);
  CHECK((rig.WireOf().delayed_us - before) >= 100U); // 12 bit times at 115200 baud
}

TEST_CASE("T214 StealthChop needs a run current of at least 8 counts of IRUN") {
  SpiRig rig;
  REQUIRE(rig.Up());
  const auto plan = rig.drv.SetCurrent(300, 90); // above the lower limit, below 8 counts
  REQUIRE(plan.ok());
  REQUIRE(plan.value.irun < 8U);
  StealthChop s;
  s.enable = true;
  CHECK_ERR(rig.drv.SetStealthChop(s), Error::Refused);
  REQUIRE(rig.drv.SetCurrent(800, 240).ok());
  CHECK_OK(rig.drv.SetStealthChop(s));
}

TEST_CASE("T215 DcStep, encoder and bridge fields land on their register bits") {
  SpiRig rig;
  REQUIRE(rig.Up());
  Chopper ch;
  ch.high_fullstep = true;
  ch.high_const_off = true;
  REQUIRE(rig.drv.SetChopper(ch).ok());
  CHECK_OK(rig.drv.SetDcStep(DcStep{.vdcmin = 1024, .dc_time = 100, .dc_sg = 5}));
  CHECK_EQ(rig.ChipOf().Reg(m::DCCTRL), (5U << 16U) | 100U);
  Encoder e;
  e.n_event.repeat = true;
  CHECK_OK(rig.drv.SetEncoder(e));
  CHECK_EQ(rig.ChipOf().Reg(m::ENCMODE) & 0x30U, 0x10U); // clr_cont
  e.n_event.repeat = false;
  e.n_event.once = true;
  CHECK_OK(rig.drv.SetEncoder(e));
  CHECK_EQ(rig.ChipOf().Reg(m::ENCMODE) & 0x30U, 0x20U); // clr_once
  Bridge b;
  b.declared = true;
  b.ot_level = OtLevel::C143;
  REQUIRE(rig.drv.Disable().ok());
  CHECK_OK(rig.drv.SetBridge(b));
  CHECK_EQ((rig.ChipOf().Reg(m::DRV_CONF) >> 16U) & 3U, 1U);
}

TEST_CASE("T216 a stuck-low MISO is noticed by Service and ReadInputs after start-up") {
  SpiRig rig;
  REQUIRE(rig.Up());
  rig.WireOf().faults.stuck = 1;
  CHECK_ERR(rig.drv.Service(), Error::Bus);
  CHECK_ERR(rig.drv.ReadInputs(), Error::DeviceIdMismatch);
}

TEST_CASE("T217 InvalidArgument sends nothing: ProgramNode, SetMicrostepTable") {
  UartRig rig;
  REQUIRE(rig.Up());
  const int sent = rig.WireOf().transfers;
  CHECK_ERR(rig.drv.ProgramNode(0, 254, false), Error::InvalidArgument); // needs multi_node
  CHECK_EQ(rig.WireOf().transfers, sent);
  MicrostepTable bad = rig.drv.GetConfig().table;
  bad.x1 = 0;
  CHECK_ERR(rig.drv.SetMicrostepTable(bad), Error::InvalidArgument);
  CHECK_EQ(rig.WireOf().transfers, sent);
}

TEST_CASE("T218 direct mode starts from a zero target") {
  Config c = BaseConfig();
  c.ack.bits = static_cast<uint32_t>(Ack::DirectMode);
  SpiRig rig(c);
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.MoveTo(5000));
  rig.Run(5000);
  CHECK_OK(rig.drv.Stop(StopKind::Immediate));
  rig.Run(2'000'000);
  CHECK_OK(rig.drv.EnterDirectMode());
  const auto target = rig.drv.ReadTarget();
  REQUIRE(target.ok());
  CHECK_EQ(target.value, 0);
}

TEST_CASE("T219 a change of the chopper while enabled is checked against an unseen reset") {
  for (int bus = 0; bus < 2; ++bus) {
    UartRig uart;
    SpiRig spi;
    const bool is_spi = bus == 1;
    if (is_spi) {
      REQUIRE(spi.Up());
      spi.ChipOf().PowerOn();
      CHECK_ERR(spi.drv.SetChopper(Chopper{}), Error::UnexpectedReset);
      CHECK(spi.ChipOf().peak_coil_ma <= 1000.0);
    } else {
      REQUIRE(uart.Up());
      uart.ChipOf().PowerOn();
      CHECK_ERR(uart.drv.SetChopper(Chopper{}), Error::UnexpectedReset);
      CHECK(uart.ChipOf().peak_coil_ma <= 1000.0);
    }
  }
}

TEST_CASE("T220 a switching-off write is never refused for the verification the caller named") {
  UartRig uart;
  REQUIRE(uart.Up());
  CHECK_OK(uart.drv.Disable(Verify::Echo)); // Echo is an SPI mode: the UART counter is used
  CHECK_EQ(uart.ChipOf().Reg(m::CHOPCONF) & 0xFU, 0U);
  CHECK(!uart.drv.IsEnabled());
  SpiRig spi;
  REQUIRE(spi.Up());
  CHECK_OK(spi.drv.Disable(Verify::Counter)); // Counter is a UART mode: the SPI echo is used
  CHECK_EQ(spi.ChipOf().Reg(m::CHOPCONF) & 0xFU, 0U);
}

TEST_CASE(
    "T221 a start-up that enables the bridge and fails leaves it off, whichever transfer fails") {
  for (int bus = 0; bus < 2; ++bus) {
    for (int n = 0; n < 160; ++n) {
      Config c = BaseConfig();
      c.begin_enabled = true;
      c.stealth.enable = true;
      UartRig uart(c);
      SpiRig spi(c);
      const bool is_spi = bus == 1;
      Wire& wire = is_spi ? spi.WireOf() : uart.WireOf();
      Chip& chip = is_spi ? spi.ChipOf() : uart.ChipOf();
      wire.faults.fail_at = n;
      const bool ok = is_spi ? spi.drv.EnsureInitialized().ok() : uart.drv.EnsureInitialized().ok();
      const bool enabled = is_spi ? spi.drv.IsEnabled() : uart.drv.IsEnabled();
      const bool started = is_spi ? spi.drv.IsInitialized() : uart.drv.IsInitialized();
      if (!ok) {
        CHECK(!enabled);
        CHECK(!started);
        CHECK_EQ(chip.Reg(m::CHOPCONF) & 0xFU, 0U);
      }
    }
  }
}

TEST_CASE("T222 on UART a lost first write of start-up is an error, not a started driver") {
  UartRig rig;
  rig.WireOf().faults.corrupt_tx_at = 1; // transfer 0 reads IFCNT, transfer 1 is the TOFF 0 write
  CHECK_ERR(rig.drv.EnsureInitialized(), Error::VerifyFailed);
  CHECK(!rig.drv.IsInitialized());
  CHECK(!rig.drv.IsEnabled());
}

namespace {

template <typename R>
void StallThenCheck(R& rig) {
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.RunVelocity(60'000));
  rig.Run(1'000'000);
  rig.ChipOf().InjectStall(true);
  rig.Run(20'000);
  REQUIRE(rig.ChipOf().VelocityHz() == 0.0);
  rig.ChipOf().InjectStall(false);
}
} // namespace

TEST_CASE("T223 switching the stall stop off never releases a stalled motor") {
  SpiRig rig(StallConfig());
  StallThenCheck(rig);
  CHECK_OK(rig.drv.SetStopOnStall(false));
  rig.Run(500'000);
  CHECK_EQ(rig.ChipOf().VelocityHz(), 0.0); // the old target is not resumed
  SpiRig uart_like(StallConfig());
  StallThenCheck(uart_like);
  Switches s = StallConfig().switches;
  s.stop_on_stall = false;
  s.stop_left = true;
  CHECK_OK(uart_like.drv.SetSwitches(s));
  uart_like.Run(500'000);
  CHECK_EQ(uart_like.ChipOf().VelocityHz(), 0.0);
}

TEST_CASE("T224 a start-up stops a ramp the last session left running") {
  for (int bus = 0; bus < 2; ++bus) {
    Config c = StallConfig();
    c.switches.stop_on_stall = false;
    SpiRig spi(c);
    UartRig uart(c);
    const bool is_spi = bus == 1;
    Chip& chip = is_spi ? spi.ChipOf() : uart.ChipOf();
    const auto elapse = [is_spi, &spi, &uart](uint32_t us) {
      if (is_spi) {
        spi.Run(us);
      } else {
        uart.Run(us);
      }
    };
    REQUIRE((is_spi ? spi.Up() : uart.Up()));
    CHECK_OK(is_spi ? spi.drv.RunVelocity(60'000) : uart.drv.RunVelocity(60'000));
    elapse(1'000'000);
    REQUIRE(chip.VelocityHz() > 1000.0);
    CHECK_OK(is_spi ? spi.drv.Deinitialize() : uart.drv.Deinitialize());
    REQUIRE((is_spi ? spi.drv.EnsureInitialized().ok() : uart.drv.EnsureInitialized().ok()));
    // The chip is still ramping down: the bridge is not energised into it.
    CHECK_ERR((is_spi ? spi.drv.Enable() : uart.drv.Enable()), Error::Busy);
    elapse(2'000'000);
    CHECK_OK(is_spi ? spi.drv.Enable() : uart.drv.Enable());
    elapse(2'000'000);
    CHECK_EQ(chip.VelocityHz(), 0.0); // nobody asked for motion
  }
}

TEST_CASE("T225 a stall stop of the last session is not released by a start-up") {
  SpiRig rig(StallConfig());
  StallThenCheck(rig);
  CHECK_OK(rig.drv.Deinitialize());
  CHECK_OK(rig.drv.EnsureInitialized());
  CHECK(rig.drv.GetBeginReport().stall_latched);
  CHECK_OK(rig.drv.Enable());
  rig.Run(500'000);
  CHECK_EQ(rig.ChipOf().VelocityHz(), 0.0);
  CHECK((rig.ChipOf().RampEvents() & (1U << 6)) != 0U); // still latched until ReleaseStall()
  CHECK_OK(rig.drv.ReleaseStall());
  CHECK_EQ(rig.ChipOf().RampEvents() & (1U << 6), 0U);
}

TEST_CASE("T226 an IFCNT mismatch found while the bridge is on switches it off") {
  UartRig rig;
  REQUIRE(rig.Up());
  rig.WireOf().faults.corrupt_tx_at = rig.WireOf().transfers; // the first write is lost, unseen
  CHECK_OK(rig.drv.SetStandstill(Standstill{.hold_delay = 3}));
  CHECK_ERR(rig.drv.SetChopper(Chopper{}), Error::VerifyFailed);
  CHECK(!rig.drv.IsEnabled());
  CHECK_EQ(rig.ChipOf().Reg(m::CHOPCONF) & 0xFU, 0U);
  CHECK(!rig.ChipOf().DrvEnnActive());
}

TEST_CASE("T227 a failed Enable write drops DRV_ENN again") {
  UartRig rig;
  REQUIRE(rig.drv.EnsureInitialized().ok());
  // Enable: GSTAT, IFCNT and RAMP_STAT reads, then the CHOPCONF write fails.
  rig.WireOf().faults.fail_at = rig.WireOf().transfers + 3;
  CHECK_ERR(rig.drv.Enable(), Error::Bus);
  CHECK(!rig.ChipOf().DrvEnnActive());
  CHECK(!rig.drv.IsEnabled());
}

TEST_CASE("T228 ReleaseStall refuses while the motor is still turning") {
  Config c = StallConfig();
  c.switches.stop_on_stall = false;
  SpiRig rig(c);
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.RunVelocity(60'000));
  rig.Run(1'000'000);
  CHECK_ERR(rig.drv.ReleaseStall(), Error::Busy);
  CHECK((rig.ChipOf().RampEvents() & (1U << 6)) == 0U);
}

TEST_CASE("T229 the UART counter survives a restart and more than 255 writes") {
  UartRig rig;
  REQUIRE(rig.Up());
  for (int i = 0; i < 100; ++i) {
    CHECK_OK(rig.drv.SetStandstill(Standstill{.hold_delay = static_cast<uint8_t>(i % 16)}));
  }
  CHECK_OK(rig.drv.Deinitialize());
  REQUIRE(rig.drv.EnsureInitialized().ok()); // IFCNT is not 0 here
  CHECK_OK(rig.drv.Enable());
  for (int i = 0; i < 100; ++i) {
    CHECK_OK(rig.drv.SetStandstill(Standstill{.hold_delay = static_cast<uint8_t>(i % 16)}));
  }
  CHECK_OK(rig.drv.Disable()); // verified by IFCNT after about 600 writes
}

TEST_CASE("T230 SENDDELAY 0 gets the timeout of SENDDELAY 1") {
  uint32_t limit[2] = {0, 0};
  for (uint8_t delay = 0; delay < 2; ++delay) {
    for (uint32_t bits = 0; bits < 2000; bits += 4) {
      Config c = BaseConfig();
      c.uart.send_delay = delay;
      UartRig rig(c);
      rig.ChipOf().k.uart_reply_delay_bits = bits;
      if (!rig.drv.EnsureInitialized().ok()) {
        break;
      }
      limit[delay] = bits;
    }
  }
  CHECK(limit[1] > 0U);
  CHECK_EQ(limit[0], limit[1]);
}

TEST_CASE("T231 on SPI Enable does not switch the bridge on for a stuck-low MISO") {
  SpiRig rig;
  REQUIRE(rig.drv.EnsureInitialized().ok());
  rig.WireOf().faults.stuck = 1;
  CHECK_ERR(rig.drv.Enable(), Error::DeviceIdMismatch); // the IOIN check, not a later read
  CHECK(!rig.drv.IsEnabled());
  CHECK(!rig.ChipOf().DrvEnnActive());
}

TEST_CASE("T232 the GCONF and CHOPCONF words pack every flag at its bit") {
  Config c = BaseConfig();
  CHECK_EQ(detail::GconfWord(c, false) & 0x1FFU, 0U);
  c.flags.invert_direction = true;
  CHECK_EQ(detail::GconfWord(c, false) & (1U << 4), 1U << 4);
  c = BaseConfig();
  c.diag.diag0_step_output = true; // either source sets bit 7
  CHECK((detail::GconfWord(c, false) & (1U << 7)) != 0U);
  c = BaseConfig();
  c.diag.diag0_stall = true;
  CHECK((detail::GconfWord(c, false) & (1U << 7)) != 0U);
  c = BaseConfig();
  c.diag.diag1_dir_output = true;
  CHECK((detail::GconfWord(c, false) & (1U << 8)) != 0U);
  c = BaseConfig();
  c.diag.diag1_stall = true;
  CHECK((detail::GconfWord(c, false) & (1U << 8)) != 0U);
  CHECK_EQ(detail::GconfWord(BaseConfig(), true) & (1U << 16), 1U << 16);

  Config k = BaseConfig();
  k.chopper.mode = ChopperMode::ConstOffTime;
  k.chopper.tfd = 9;
  k.chopper.offset = 5;
  k.chopper.no_comparator = true;
  const uint32_t w = detail::ChopconfWord(k, 3U);
  CHECK_EQ((w >> 4) & 7U, 1U);   // tfd low three bits
  CHECK_EQ((w >> 7) & 15U, 5U);  // sine wave offset
  CHECK((w & (1U << 11)) != 0U); // tfd bit 3
  CHECK((w & (1U << 12)) != 0U); // no comparator
  CHECK((w & (1U << 14)) != 0U); // chm
  k.chopper.tfd = 1;
  k.chopper.no_comparator = false;
  const uint32_t v = detail::ChopconfWord(k, 3U);
  CHECK_EQ(v & ((1U << 11) | (1U << 12)), 0U);
  const Config s = BaseConfig();
  const uint32_t spread = detail::ChopconfWord(s, 3U);
  CHECK_EQ((spread >> 4) & 7U, 5U);  // hstrt
  CHECK_EQ((spread >> 7) & 15U, 2U); // hend
  CHECK_EQ(spread & ((1U << 11) | (1U << 12) | (1U << 14)), 0U);
  const units::CurrentPlan plan{};
  const detail::WordContext ctx{s, plan, false, false, false};
  CHECK_EQ(detail::WordFor(ctx, 0x7FU), 0U); // an address with no encoder
}

namespace {
template <typename R>
bool InsideDriver(const R& rig, const uint8_t* p) {
  const auto base = std::bit_cast<std::uintptr_t>(&rig.drv);
  const auto at = std::bit_cast<std::uintptr_t>(p);
  return (at >= base) && (at < (base + sizeof(rig.drv)));
}
} // namespace

TEST_CASE("T233 the transfer buffers are members of the driver, not stack arrays") {
  SpiRig spi;
  REQUIRE(spi.Up());
  CHECK(InsideDriver(spi, spi.WireOf().last_tx));
  CHECK(InsideDriver(spi, spi.WireOf().last_rx));
  UartRig uart;
  REQUIRE(uart.Up());
  CHECK_OK(uart.drv.ReadStepTime());
  CHECK(InsideDriver(uart, uart.WireOf().last_tx));
  CHECK(InsideDriver(uart, uart.WireOf().last_rx)); // the UART write has no reply buffer use
  CHECK_OK(uart.drv.SetStandstill(Standstill{.hold_delay = 2}));
  CHECK(InsideDriver(uart, uart.WireOf().last_tx));
}

namespace {
/// A running motor, the bridge on, then one SPI reply whose status byte reports a reset.
void ResetReported(SpiRig& rig) {
  CHECK(rig.Up());
  CHECK_OK(rig.drv.RunVelocity(60'000));
  rig.Run(500'000);
  rig.WireOf().faults.stuck = 2; // MISO held high for one call: status bit 0 reads as set
  CHECK_ERR(rig.drv.Service(), Error::UnexpectedReset);
  rig.WireOf().faults.stuck = 0;
  CHECK(!rig.drv.IsInitialized());
  CHECK(!rig.ChipOf().DrvEnnActive()); // a status byte is never believed with the pin raised
}
} // namespace

TEST_CASE("T234 after a reset report the switch-off, the stops and "
          "Deinitialize still reach the chip") {
  {
    SpiRig rig(StallConfig());
    ResetReported(rig);
    CHECK_OK(rig.drv.Disable());
    CHECK(!rig.ChipOf().DrvEnnActive());
    CHECK_EQ(rig.ChipOf().Reg(m::CHOPCONF) & 0xFU, 0U);
  }
  {
    SpiRig rig(StallConfig());
    ResetReported(rig);
    CHECK_OK(rig.drv.Stop(StopKind::Emergency));
    CHECK(!rig.ChipOf().DrvEnnActive());
    CHECK_EQ(rig.ChipOf().Reg(m::CHOPCONF) & 0xFU, 0U);
  }
  {
    SpiRig rig(StallConfig());
    ResetReported(rig);
    CHECK_OK(rig.drv.Deinitialize());
    CHECK(!rig.ChipOf().DrvEnnActive());
    CHECK_EQ(rig.ChipOf().Reg(m::CHOPCONF) & 0xFU, 0U);
  }
  for (const StopKind kind : {StopKind::Ramp, StopKind::Immediate, StopKind::HoldTarget}) {
    SpiRig rig(StallConfig());
    ResetReported(rig);
    CHECK_OK(rig.drv.Stop(kind));
    rig.Run(3'000'000);
    CHECK_EQ(rig.ChipOf().VelocityHz(), 0.0); // the chip stopped, not only a write was sent
  }
  {
    SpiRig rig(StallConfig());
    ResetReported(rig);
    CHECK_OK(rig.drv.Hold());
    CHECK_EQ(rig.ChipOf().Reg(m::RAMPMODE), 3U);
  }
  SpiRig never; // a driver that never started has nothing to switch off
  const int sent = never.WireOf().transfers;
  CHECK_ERR(never.drv.Disable(), Error::NotInitialized);
  CHECK_ERR(never.drv.Stop(StopKind::Emergency), Error::NotInitialized);
  CHECK_OK(never.drv.Deinitialize());
  CHECK_EQ(never.WireOf().transfers, sent);
}

TEST_CASE("T235 a Deinitialize whose switch-off write failed is an error and "
          "stays owed") {
  SpiRig rig;
  REQUIRE(rig.Up());
  rig.WireOf().faults.fail_at = rig.WireOf().transfers;
  CHECK_ERR(rig.drv.Deinitialize(), Error::Bus);
  CHECK_ERR(rig.drv.Disable(),
            Error::Ok); // the duty is still owed: this one reaches the chip
  CHECK_EQ(rig.ChipOf().Reg(m::CHOPCONF) & 0xFU, 0U);
  CHECK(!rig.drv.IsEnabled());
}

TEST_CASE("T236 Stop in direct mode zeroes the coil currents and never writes "
          "XACTUAL there") {
  Config c = BaseConfig();
  c.ack = c.ack.With(Ack::DirectMode);
  for (const StopKind kind : {StopKind::Ramp, StopKind::Immediate, StopKind::HoldTarget}) {
    SpiRig rig(c);
    REQUIRE(rig.Up());
    CHECK_OK(rig.drv.SetHomePosition(0x00FF00FF)); // a XACTUAL that reads as coil currents 255/255
    CHECK_OK(rig.drv.EnterDirectMode());
    CHECK_OK(rig.drv.SetDirectCurrents(100, -100));
    CHECK_EQ(static_cast<uint32_t>(rig.ChipOf().Xtarget()),
             ((0x1FFU & static_cast<uint32_t>(-100)) << 16U) | 100U);
    CHECK_OK(rig.drv.Stop(kind));
    CHECK_EQ(rig.ChipOf().Xtarget(), 0);
    CHECK_EQ(rig.ChipOf().Reg(m::VMAX), 0U);
    CHECK_EQ(rig.ChipOf().Reg(m::RAMPMODE), 1U); // a velocity-mode stop, in either mode
  }
  SpiRig hold(c);
  REQUIRE(hold.Up());
  CHECK_OK(hold.drv.EnterDirectMode());
  CHECK_OK(hold.drv.SetDirectCurrents(50, 50));
  CHECK_OK(hold.drv.Hold());
  CHECK_EQ(hold.ChipOf().Xtarget(), 0);
  CHECK_EQ(hold.ChipOf().Reg(m::RAMPMODE), 3U); // Hold keeps the (zero) state
}

TEST_CASE("T237 a direct-mode write that failed leaves the mode in doubt and "
          "motion refused") {
  Config c = BaseConfig();
  c.ack = c.ack.With(Ack::DirectMode);
  SpiRig rig(c);
  REQUIRE(rig.Up());
  // Enter: reads RAMP_STAT (2 transfers), writes RAMPMODE and XTARGET, then GCONF fails.
  rig.WireOf().faults.fail_at = rig.WireOf().transfers + 4;
  CHECK_ERR(rig.drv.EnterDirectMode(), Error::Bus);
  rig.WireOf().faults.fail_at = -1;
  CHECK_ERR(rig.drv.MoveTo(1000), Error::Refused);
  CHECK_OK(rig.drv.Stop(StopKind::HoldTarget));
  CHECK_EQ(rig.ChipOf().Xtarget(), 0);
  CHECK_OK(rig.drv.ExitDirectMode());
  CHECK_OK(rig.drv.MoveTo(1000));
}

TEST_CASE("T238 a stuck-low MISO is never read as a driver status, a snapshot "
          "or a standstill") {
  Config c = BaseConfig();
  c.ack = c.ack.With(Ack::DirectMode);
  SpiRig rig(c);
  REQUIRE(rig.Up());
  rig.WireOf().faults.stuck = 1;
  CHECK_ERR(rig.drv.ReadDriverStatus(), Error::Bus);
  CHECK_ERR(rig.drv.ReadStallGuard(), Error::Bus);
  CHECK_ERR(rig.drv.ReadSnapshot(), Error::Bus);
  CHECK_ERR(rig.drv.EnterDirectMode(),
            Error::Bus); // zero speed read from a dead line
  rig.WireOf().faults.stuck = 0;
  CHECK_OK(rig.drv.ReadDriverStatus());
  CHECK_OK(rig.drv.ReadSnapshot());
}

TEST_CASE("T239 a reset report clears the enabled state and a running move is "
          "not stopped by "
          "switching off a stop that never fired") {
  SpiRig rig(StallConfig());
  ResetReported(rig);
  CHECK(!rig.drv.IsEnabled());
  SpiRig run(StallConfig());
  REQUIRE(run.Up());
  CHECK_OK(run.drv.RunVelocity(60'000));
  run.Run(1'000'000);
  REQUIRE(run.ChipOf().VelocityHz() > 1000.0);
  CHECK_OK(run.drv.SetStopOnStall(false)); // no latched stall event: the move goes on
  run.Run(200'000);
  CHECK(run.ChipOf().VelocityHz() > 1000.0);
  Switches s = StallConfig().switches;
  s.stop_on_stall = false;
  s.stop_left = true;
  CHECK_OK(run.drv.SetSwitches(s));
  CHECK_OK(run.drv.SetSwitches(Switches{.stop_left = false, .stop_right = false}));
  run.Run(200'000);
  CHECK(run.ChipOf().VelocityHz() > 1000.0);
}

TEST_CASE("T240 switching off a reference stop that fired does not resume the "
          "old target") {
  for (int side = 0; side < 2; ++side) {
    Config c = StallConfig();
    c.switches.stop_on_stall = false;
    c.switches.stop_left = (side == 0);
    c.switches.stop_right = (side == 1);
    SpiRig rig(c);
    REQUIRE(rig.Up());
    CHECK_OK(rig.drv.RunVelocity(side == 0 ? -60'000 : 60'000));
    rig.Run(1'000'000);
    rig.ChipOf().SetReferenceSwitches(side == 0, side == 1);
    rig.Run(300'000);
    REQUIRE(rig.ChipOf().VelocityHz() == 0.0);
    rig.ChipOf().SetReferenceSwitches(false, false);
    Switches off = c.switches;
    off.stop_left = false;
    off.stop_right = false;
    CHECK_OK(rig.drv.SetSwitches(off));
    rig.Run(500'000);
    CHECK_EQ(rig.ChipOf().VelocityHz(), 0.0);
  }
}

TEST_CASE("T241 Stop(Emergency) in STEP/DIR mode switches the bridge off") {
  Config c = BaseConfig();
  c.mode = Mode::StepDir;
  SpiRig rig(c);
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.Stop(StopKind::Emergency));
  CHECK(!rig.ChipOf().DrvEnnActive());
  CHECK_EQ(rig.ChipOf().Reg(m::CHOPCONF) & 0xFU, 0U);
}

TEST_CASE("T242 direct mode is refused while the motor moves and left in "
          "position at the end") {
  Config c = BaseConfig();
  c.ack = c.ack.With(Ack::DirectMode);
  SpiRig mov(c);
  REQUIRE(mov.Up());
  CHECK_OK(mov.drv.RunVelocity(60'000));
  mov.Run(500'000);
  CHECK_ERR(mov.drv.EnterDirectMode(), Error::Busy);
  SpiRig rig(c);
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.SetHomePosition(5000));
  CHECK_OK(rig.drv.EnterDirectMode());
  CHECK_OK(rig.drv.SetDirectCurrents(100, 100));
  rig.ChipOf().Write(m::RAMPMODE,
                     0U); // something left the chip in position mode
  CHECK_OK(rig.drv.ExitDirectMode());
  CHECK_EQ(rig.ChipOf().Reg(m::RAMPMODE), 3U); // Hold
  CHECK_EQ(rig.ChipOf().Xtarget(), 5000);      // the motor must not jump
}

TEST_CASE("T243 the UART write counter ignores writes to other nodes") {
  UartRig rig;
  REQUIRE(rig.Up());
  CHECK_ERR(rig.drv.ProgramNode(1, 5, false),
            Error::Ok); // two writes to a node that is not ours
  CHECK_OK(rig.drv.SetCurrent(300, 100, Verify::Counter));
}

TEST_CASE("T244 on SPI Enable proves GLOBALSCALER and IHOLD_IRUN before the "
          "bridge is switched on") {
  bool caught = false;
  for (int k = 0; (k < 16) && !caught; ++k) {
    SpiRig rig;
    REQUIRE(rig.drv.EnsureInitialized().ok());
    rig.WireOf().faults.corrupt_at = rig.WireOf().transfers + k;
    const auto r = rig.drv.Enable();
    if (r.error == Error::VerifyFailed) {
      caught = true;
      CHECK(!rig.ChipOf().DrvEnnActive());
      CHECK(!rig.drv.IsEnabled());
    }
  }
  CHECK(caught);
}

TEST_CASE("T245 a stop after a start-up that failed on a chip left in direct mode zeroes XTARGET") {
  int zeroed = 0;
  for (int k = 0; k < 60; ++k) {
    SpiRig rig;
    rig.ChipOf().Write(m::GCONF, 1U << 16U); // the last session ended in direct mode
    rig.ChipOf().Write(m::XTARGET, 0x00FF00FFU);
    rig.WireOf().faults.fail_from = rig.WireOf().transfers + k;
    if (rig.drv.EnsureInitialized().ok()) {
      continue;
    }
    rig.WireOf().faults.fail_from = -1;
    if (rig.drv.Stop(StopKind::Ramp).ok() && ((rig.ChipOf().Reg(m::GCONF) & (1U << 16U)) != 0U)) {
      CHECK_EQ(rig.ChipOf().Xtarget(), 0);
      ++zeroed;
    }
  }
  CHECK(zeroed > 0);
}
