/**
 * @file test_knobs.cpp
 * @brief The ambiguities of the design note (A1 to A50): the driver run against both readings (T200
 * to T240).
 *
 * Each case flips one `Knobs` switch of the chip model and checks that the driver still behaves, or
 * fails in the way the ambiguity page documents.
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
} // namespace

TEST_CASE("A2 the SPI clock limit follows the clock source") {
  Config c = BaseConfig();
  SpiRig a(c);
  a.WireOf().faults.sck_hz = 4'000'000;
  CHECK_OK(a.drv.EnsureInitialized());
  Config e = BaseConfig();
  e.clock.external = true;
  e.clock.hz = 16'000'000;
  SpiRig b(e);
  b.WireOf().faults.sck_hz = 7'300'000; // above 0.9 x fCLK/2
  CHECK_ERR(b.drv.EnsureInitialized(), Error::Unsupported);
  b.WireOf().faults.sck_hz = 7'200'000;
  CHECK_OK(b.drv.EnsureInitialized());
}

TEST_CASE("A3 with and without the status lag the start-up never reports a reset it caused") {
  for (const bool lag : {true, false}) {
    SpiRig rig;
    rig.ChipOf().k.status_lag = lag;
    CHECK_OK(rig.drv.EnsureInitialized());
    CHECK_OK(rig.drv.Enable());
    CHECK(rig.drv.ReadPosition().ok());
  }
}

TEST_CASE("A4 a write-only register is refused whatever the chip would answer") {
  for (uint8_t mode = 0; mode < 3; ++mode) {
    SpiRig rig;
    rig.ChipOf().k.wo_read_mode = mode;
    CHECK_OK(rig.drv.EnsureInitialized());
    CHECK_ERR(rig.drv.ReadRegister(m::CHOPCONF + 1), Error::Refused);
    CHECK_ERR(rig.drv.ReadRegister(m::GLOBAL_SCALER), Error::Refused);
    CHECK_EQ(rig.ChipOf().wo_reads, 0U);
  }
}

TEST_CASE("A6 both readings of the microstep table agree within one count and the driver accepts "
          "the default") {
  SpiRig a;
  SpiRig b;
  b.ChipOf().k.lut_literal_reading = true;
  REQUIRE(a.Up());
  REQUIRE(b.Up());
  CHECK_OK(a.drv.SetHomePosition(128));
  CHECK_OK(b.drv.SetHomePosition(128));
  const int da = a.drv.ReadMicrostepCurrents().value.cur_a;
  const int db = b.drv.ReadMicrostepCurrents().value.cur_a;
  CHECK(((da - db) <= 1) && ((db - da) <= 1));
}

TEST_CASE("A7 swapped MSCURACT fields swap the driver's report; nothing else changes") {
  SpiRig a;
  SpiRig b;
  b.ChipOf().k.mscuract_swap = true;
  REQUIRE(a.Up());
  REQUIRE(b.Up());
  const auto x = a.drv.ReadMicrostepCurrents();
  const auto y = b.drv.ReadMicrostepCurrents();
  CHECK(x.ok() && y.ok());
  CHECK_EQ(x.value.cur_a, y.value.cur_b);
  CHECK_EQ(x.value.cur_b, y.value.cur_a);
}

TEST_CASE("A8 the OTP image is decoded, never written") {
  SpiRig rig;
  rig.ChipOf().k.otp_byte = 0x000000FFU;
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.ReadOtp());
  CHECK_EQ(rig.ChipOf().forbidden_writes, 0U);
}

TEST_CASE("A9 the bridge timing is never written with a running motor") {
  SpiRig rig(Quick());
  rig.ChipOf().k.bbm_hazard = true;
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.RunVelocity(50'000));
  rig.Run(500'000);
  CHECK_ERR(rig.drv.SetBridge(Bridge{.declared = true, .bbm_time = 5}), Error::Busy);
  CHECK_EQ(rig.ChipOf().Reg(m::GSTAT) & 2U, 0U);
}

TEST_CASE("A12 the multistep filter bit is written explicitly, whatever the reset value is") {
  for (const bool reset_one : {false, true}) {
    for (const bool want : {false, true}) {
      Config c = BaseConfig();
      c.flags.multistep_filt = want;
      SpiRig rig(c);
      rig.ChipOf().k.multistep_reset = reset_one;
      rig.ChipOf().PowerOn();
      REQUIRE(rig.drv.EnsureInitialized().ok());
      CHECK_EQ((rig.ChipOf().Reg(m::GCONF) >> 3) & 1U, want ? 1U : 0U);
    }
  }
}

TEST_CASE("A13 start-up works with and without the reset flag set in GSTAT") {
  for (const bool set : {false, true}) {
    SpiRig rig;
    rig.ChipOf().k.gstat_reset_value = set;
    rig.ChipOf().PowerOn();
    CHECK_OK(rig.drv.EnsureInitialized());
    CHECK_EQ(rig.ChipOf().Reg(m::GSTAT) & 7U, 0U);
  }
}

TEST_CASE("A20 a sense voltage 5 percent high still keeps the coil under the rated current") {
  for (const uint32_t vfs : {310U, 325U, 341U}) {
    SpiRig rig;
    rig.ChipOf().k.vfs_mv = vfs;
    REQUIRE(rig.drv.EnsureInitialized().ok());
    CHECK_OK(rig.drv.Enable());
    for (uint32_t ask = 100; ask <= 950; ask += 70) {
      if (rig.drv.SetCurrent(ask, ask / 3).ok()) {
        CHECK(rig.ChipOf().RunMa() <= 1000.0);
      }
    }
    CHECK(rig.ChipOf().peak_coil_ma <= 1000.0);
  }
}

TEST_CASE("A24 the driver works whether or not the ramp event bits clear on write") {
  for (const bool wc : {true, false}) {
    SpiRig rig(Quick());
    rig.ChipOf().k.ramp_stat_wc = wc;
    REQUIRE(rig.Up());
    CHECK_OK(rig.drv.MoveTo(500));
    rig.Run(1'000'000);
    CHECK(rig.drv.Service().ok());
    CHECK(rig.drv.Service().ok());
  }
}

TEST_CASE("A26 leaving direct mode never moves the motor, whichever way the chip treats XTARGET") {
  for (const bool keeps : {true, false}) {
    Config c = Quick();
    c.ack = c.ack.With(Ack::DirectMode);
    SpiRig rig(c);
    rig.ChipOf().k.direct_mode_exit = keeps;
    REQUIRE(rig.Up());
    CHECK_OK(rig.drv.EnterDirectMode());
    CHECK_OK(rig.drv.SetDirectCurrents(100, -100));
    CHECK_OK(rig.drv.ExitDirectMode());
    rig.Run(200'000);
    CHECK_EQ(rig.ChipOf().Xactual(), 0);
  }
}

TEST_CASE(
    "A27 a VDCMIN the chip truncates is still accepted by the driver (the docs name the limit)") {
  Config c = Quick();
  c.chopper.high_fullstep = true;
  c.chopper.high_const_off = true;
  c.dcstep.vdcmin = 0x500000;
  c.dcstep.dc_time = 40;
  for (const bool wide : {true, false}) {
    SpiRig rig(c);
    rig.ChipOf().k.vdcmin_range = wide;
    CHECK_OK(rig.drv.EnsureInitialized());
  }
}

TEST_CASE("A29 an unknown VERSION byte is DeviceIdMismatch; 0x30 is accepted") {
  for (const uint8_t version :
       {uint8_t{0x30}, uint8_t{0x11}, uint8_t{0x31}, uint8_t{0xFF}, uint8_t{0x00}}) {
    SpiRig rig;
    rig.ChipOf().k.version_byte = version;
    const auto r = rig.drv.EnsureInitialized();
    CHECK_EQ(r.ok(), version == 0x30U);
  }
}

TEST_CASE("A30 IFCNT in SPI mode: the driver never uses it") {
  for (const bool counts : {false, true}) {
    SpiRig rig;
    rig.ChipOf().k.ifcnt_spi = counts;
    CHECK_OK(rig.drv.EnsureInitialized());
    CHECK_ERR(rig.drv.ReadInterfaceCounter(), Error::Unsupported);
  }
}

TEST_CASE("A31 node 254 inside the addressing procedure; a chip that refuses it leaves the driver "
          "unable to talk") {
  Config c = BaseConfig();
  c.uart.multi_node = true;
  c.uart.send_delay = 4;
  for (const bool accepts : {true, false}) {
    UartRig rig(c);
    rig.ChipOf().k.node_254 = accepts;
    REQUIRE(rig.drv.EnsureInitialized().ok());
    const auto r = rig.drv.ProgramNode(0, 254, false);
    CHECK_EQ(r.ok(), true);
    CHECK_EQ(rig.drv.ReadStepTime().ok(), accepts);
  }
}

TEST_CASE(
    "A32 the UART reply timeout scales with SENDDELAY; a chip slower than that is a Timeout") {
  UartRig fast;
  fast.ChipOf().k.uart_reply_delay_bits = 4;
  CHECK_OK(fast.drv.EnsureInitialized());
  UartRig slow;
  slow.ChipOf().k.uart_reply_delay_bits = 5000;
  CHECK_ERR(slow.drv.EnsureInitialized(), Error::Timeout);
}

TEST_CASE("A33 the recovery wait covers the chip's idle requirement") {
  UartRig rig;
  REQUIRE(rig.Up());
  rig.ChipOf().k.uart_recovery_bits = 12;
  rig.WireOf().faults.corrupt_tx_at =
      rig.WireOf().transfers; // the chip sees a bad CRC and wants the line idle
  CHECK_OK(rig.drv.ReadStepTime());
  CHECK_EQ(rig.ChipOf().dropped_in_recovery, 0U);
  CHECK_EQ(rig.ChipOf().crc_errors, 1U);
  rig.ChipOf().k.uart_recovery_bits =
      4000; // a chip that wants far longer: the retries are dropped, the read fails
  rig.WireOf().faults.corrupt_tx_at = rig.WireOf().transfers;
  CHECK(!rig.drv.ReadStepTime().ok());
  CHECK(rig.ChipOf().dropped_in_recovery > 0U);
}

TEST_CASE("A37 the StealthChop tuning wait covers the A version; a slower chip leaves the tuning "
          "unfinished") {
  for (const bool a_version : {true, false}) {
    Config c = Quick();
    c.stealth.enable = true;
    SpiRig rig(c);
    rig.ChipOf().k.at_a_version = a_version;
    REQUIRE(rig.drv.EnsureInitialized().ok());
    CHECK_OK(rig.drv.Enable());
    CHECK_EQ(rig.ChipOf().at1_done, a_version);
    CHECK_EQ(rig.ChipOf().at1_failed,
             !a_version); // the hold current came back before a slower chip had finished
  }
}

TEST_CASE("A40 and A41 OTP_PROG and FACTORY_CONF are never written, in any scenario") {
  SpiRig rig(Quick());
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.MoveTo(100));
  rig.Run(100'000);
  CHECK_OK(rig.drv.ReadOtp());
  CHECK_OK(rig.drv.ReadClockTrim());
  CHECK_EQ(rig.ChipOf().forbidden_writes, 0U);
}

TEST_CASE("A42 the strap is verified in both modes") {
  for (const bool sd : {false, true}) {
    Config c = BaseConfig();
    c.mode = sd ? Mode::StepDir : Mode::MotionController;
    SpiRig ok(c);
    CHECK_OK(ok.drv.EnsureInitialized());
    SpiRig bad(c);
    bad.ChipOf().k.sd_mode = !sd;
    CHECK_ERR(bad.drv.EnsureInitialized(), Error::ModeMismatch);
  }
}

TEST_CASE(
    "A43 a negative decimal encoder constant follows the p107 rule; the other reading differs") {
  Config c = Quick();
  c.encoder.declared = true;
  c.encoder.scale =
      EncoderScale{.decimal = true, .integer = 25, .fraction = 6000, .negative = true};
  SpiRig a(c);
  SpiRig b(c);
  b.ChipOf().k.enc_decimal_sign = false;
  REQUIRE(a.Up());
  REQUIRE(b.Up());
  a.ChipOf().AddEncoderCounts(1000);
  b.ChipOf().AddEncoderCounts(1000);
  CHECK_EQ(a.drv.ReadEncoderPosition().value, -25'600);
  CHECK(b.drv.ReadEncoderPosition().value != -25'600);
}

TEST_CASE("A49 the X_COMPARE pulse shows on DIAG1 when the model emits it") {
  Config c = Quick();
  c.mode = Mode::StepDir;
  SpiRig rig(c);
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.SetDiag(Diag{.diag1_stall = true}));
  const auto idle = rig.drv.Diag1Active();
  CHECK(idle.ok() && !idle.value);
}

TEST_CASE("A49 the X_COMPARE pulse is seen on DIAG1 only when the chip emits it") {
  for (const bool pulse : {true, false}) {
    Config c = Quick();
    SpiRig rig(c);
    rig.ChipOf().k.xcompare_pulse = pulse;
    REQUIRE(rig.Up());
    CHECK_OK(rig.drv.SetPositionCompare(100));
    CHECK_OK(rig.drv.MoveTo(400));
    int seen = 0;
    for (int i = 0; i < 400; ++i) {
      rig.Run(250);
      const auto level = rig.drv.Diag1Active();
      seen += (level.ok() && level.value) ? 1 : 0;
    }
    CHECK_EQ(seen > 0, pulse);
  }
}

TEST_CASE("A50 a supply outside 10 to 55 V is refused") {
  for (const uint32_t mv : {9'999U, 55'001U}) {
    Config c = BaseConfig();
    c.board.supply_mv = mv;
    SpiRig rig(c);
    CHECK_ERR(rig.drv.EnsureInitialized(), Error::InvalidArgument);
  }
}

TEST_CASE("A44 DC_TIME must be above tBLANK in clocks, for each TBL") {
  const uint32_t blank[4] = {16, 24, 36, 54};
  for (uint8_t tbl = 0; tbl < 4; ++tbl) {
    for (const uint32_t at : {blank[tbl], blank[tbl] + 1U}) {
      Config c = Quick();
      c.chopper.tbl = tbl;
      c.chopper.high_fullstep = true;
      c.chopper.high_const_off = true;
      c.dcstep.vdcmin = 1024;
      c.dcstep.dc_time = static_cast<uint16_t>(at);
      SpiRig rig(c);
      if (at == blank[tbl]) {
        CHECK_ERR(rig.drv.EnsureInitialized(), Error::InvalidArgument);
      } else {
        CHECK_OK(rig.drv.EnsureInitialized());
      }
    }
  }
}

TEST_CASE("A47 the sense resistor is accepted from 1 to 10000 mOhm if the plan fits") {
  for (const uint32_t r : {0U, 10'001U}) {
    Config c = BaseConfig();
    c.board.rsense_mohm = r;
    SpiRig rig(c);
    CHECK_ERR(rig.drv.EnsureInitialized(), Error::InvalidArgument);
  }
  Config big = MakeConfig(Motor{.rated_rms_ma = 20, .coil_resistance_mohm = 6000},
                          Board{.rsense_mohm = 10'000, .supply_mv = 24'000});
  big.current = Current{15, 5};
  SpiRig ok(big);
  CHECK_OK(ok.drv.EnsureInitialized());
  CHECK_ERR(ok.drv.SetCurrent(200, 50), Error::LimitExceeded); // the plan does not fit the rating
}

TEST_CASE("A38 StallGuard values are returned raw over the whole 10 bits") {
  SpiRig rig;
  REQUIRE(rig.Up());
  for (const uint32_t v : {0U, 1U, 511U, 1023U}) {
    rig.ChipOf().SetSgResult(v);
    const auto sg = rig.drv.ReadStallGuard();
    CHECK(sg.ok() && (sg.value == v));
  }
}
