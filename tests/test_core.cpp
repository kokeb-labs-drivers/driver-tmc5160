/**
 * @file test_core.cpp
 * @brief The pure parts: register table, word builders, units, validation, current plans, advice
 * (T120 to T160).
 */
#include <array>
#include <string_view>

#include "fixture.hpp"

using namespace tmc5160;
using namespace tmc5160_test;
namespace m = tmc5160_test::m;

namespace {
struct Mutation {
  const char* name;
  void (*apply)(Config&);
};

/// Each of these must make `Validate()` fail.
constexpr std::array kInvalid{
    Mutation{"no motor", [](Config& c) { c.motor.rated_rms_ma = 0; }},
    Mutation{"motor max above range", [](Config& c) { c.motor.max_rms_ma = 200'000; }},
    Mutation{"zero full steps", [](Config& c) { c.motor.fullsteps_per_rev = 0; }},
    Mutation{"no sense resistor", [](Config& c) { c.board.rsense_mohm = 0; }},
    Mutation{"supply low", [](Config& c) { c.board.supply_mv = 9'000; }},
    Mutation{"supply high", [](Config& c) { c.board.supply_mv = 56'000; }},
    Mutation{"supply max below supply", [](Config& c) { c.board.supply_max_mv = 20'000; }},
    Mutation{"zero run", [](Config& c) { c.current.run_ma = 0; }},
    Mutation{"hold delay", [](Config& c) { c.standstill.hold_delay = 16; }},
    Mutation{"internal clock changed", [](Config& c) { c.clock.hz = 13'000'000; }},
    Mutation{"external clock low",
             [](Config& c) {
               c.clock.external = true;
               c.clock.hz = 3'000'000;
             }},
    Mutation{"external clock high",
             [](Config& c) {
               c.clock.external = true;
               c.clock.hz = 19'000'000;
             }},
    Mutation{"node", [](Config& c) { c.uart.node = 254; }},
    Mutation{"send delay with several nodes",
             [](Config& c) {
               c.uart.multi_node = true;
               c.uart.send_delay = 1;
             }},
    Mutation{"baud low", [](Config& c) { c.uart.baud = 8000; }},
    Mutation{"baud high", [](Config& c) { c.uart.baud = 800'000; }},
    Mutation{"chain zero", [](Config& c) { c.spi.chain_length = 0; }},
    Mutation{"chain long", [](Config& c) { c.spi.chain_length = 9; }},
    Mutation{"chain index", [](Config& c) { c.spi.chain_index = 1; }},
    Mutation{"toff 0", [](Config& c) { c.chopper.toff = 0; }},
    Mutation{"toff 16", [](Config& c) { c.chopper.toff = 16; }},
    Mutation{"toff 1 with tbl 0",
             [](Config& c) {
               c.chopper.toff = 1;
               c.chopper.tbl = 0;
             }},
    Mutation{"tbl 4", [](Config& c) { c.chopper.tbl = 4; }},
    Mutation{"tpfd", [](Config& c) { c.chopper.tpfd = 16; }},
    Mutation{"hstrt", [](Config& c) { c.chopper.hstrt = 8; }},
    Mutation{"hend", [](Config& c) { c.chopper.hend = 16; }},
    Mutation{"hysteresis sum",
             [](Config& c) {
               c.chopper.hstrt = 7;
               c.chopper.hend = 15;
             }},
    Mutation{"tfd", [](Config& c) { c.chopper.tfd = 16; }},
    Mutation{"offset", [](Config& c) { c.chopper.offset = 16; }},
    Mutation{"pwm freq", [](Config& c) { c.stealth.freq = 4; }},
    Mutation{"pwm reg 0 with autoscale", [](Config& c) { c.stealth.reg = 0; }},
    Mutation{"pwm reg 16", [](Config& c) { c.stealth.reg = 16; }},
    Mutation{"pwm lim", [](Config& c) { c.stealth.lim = 16; }},
    Mutation{"power down delay with autoscale",
             [](Config& c) {
               c.stealth.enable = true;
               c.standstill.power_down_delay = 1;
             }},
    Mutation{"tpwmthrs range", [](Config& c) { c.thresholds.pwm = 0x100000; }},
    Mutation{"tcoolthrs range", [](Config& c) { c.thresholds.cool = 0x100000; }},
    Mutation{"thigh range", [](Config& c) { c.thresholds.high = 0x100000; }},
    Mutation{"cool above pwm",
             [](Config& c) {
               c.thresholds.pwm = 10;
               c.thresholds.cool = 20;
             }},
    Mutation{"high above cool",
             [](Config& c) {
               c.thresholds.cool = 10;
               c.thresholds.high = 20;
             }},
    Mutation{"coolstep without TCOOLTHRS", [](Config& c) { c.cool.semin = 3; }},
    Mutation{"stall stop without TCOOLTHRS", [](Config& c) { c.switches.stop_on_stall = true; }},
    Mutation{"semin", [](Config& c) { c.cool.semin = 16; }},
    Mutation{"semax", [](Config& c) { c.cool.semax = 16; }},
    Mutation{"seup", [](Config& c) { c.cool.seup = 4; }},
    Mutation{"sedn", [](Config& c) { c.cool.sedn = 4; }},
    Mutation{"sgt low", [](Config& c) { c.stall.threshold = -65; }},
    Mutation{"sgt high", [](Config& c) { c.stall.threshold = 64; }},
    Mutation{"vdcmin low", [](Config& c) { c.dcstep.vdcmin = 100; }},
    Mutation{"vdcmin high", [](Config& c) { c.dcstep.vdcmin = 0x800000; }},
    Mutation{"dc time", [](Config& c) { c.dcstep.dc_time = 1024; }},
    Mutation{"dcstep without chopper mode",
             [](Config& c) {
               c.dcstep.vdcmin = 512;
               c.dcstep.dc_time = 10;
             }},
    Mutation{"bbm time", [](Config& c) { c.bridge.bbm_time = 25; }},
    Mutation{"bbm clks", [](Config& c) { c.bridge.bbm_clks = 16; }},
    Mutation{"drive strength", [](Config& c) { c.bridge.drive_strength = 4; }},
    Mutation{"sense filter", [](Config& c) { c.bridge.sense_filter = 4; }},
    Mutation{"slow mosfet", [](Config& c) { c.bridge.mosfet_switching_ns = 1000; }},
    Mutation{"s2vs low", [](Config& c) { c.protection.s2vs_level = 3; }},
    Mutation{"s2g low", [](Config& c) { c.protection.s2g_level = 1; }},
    Mutation{"s2g low at high supply",
             [](Config& c) {
               c.board.supply_max_mv = 54'000;
               c.protection.s2g_level = 6;
             }},
    Mutation{"short filter", [](Config& c) { c.protection.filter = 4; }},
    Mutation{"vstart", [](Config& c) { c.ramp.vstart = 0x40000; }},
    Mutation{"a1", [](Config& c) { c.ramp.a1 = 0x10000; }},
    Mutation{"v1", [](Config& c) { c.ramp.v1 = 0x100000; }},
    Mutation{"amax", [](Config& c) { c.ramp.amax = 0x10000; }},
    Mutation{"dmax", [](Config& c) { c.ramp.dmax = 0x10000; }},
    Mutation{"d1 0", [](Config& c) { c.ramp.d1 = 0; }},
    Mutation{"vstop 0", [](Config& c) { c.ramp.vstop = 0; }},
    Mutation{"zero wait", [](Config& c) { c.ramp.zero_wait = 0x10000; }},
    Mutation{"vmax above limit", [](Config& c) { c.ramp.vmax = (1U << 23); }},
    Mutation{"vmax below vstart",
             [](Config& c) {
               c.ramp.vstart = 300;
               c.ramp.vmax = 100;
             }},
    Mutation{"vmax above the declared ceiling", [](Config& c) { c.max_velocity_reg = 1000; }},
    Mutation{"diag StepDir function in MC mode", [](Config& c) { c.diag.diag0_otpw = true; }},
    Mutation{"diag MC function in StepDir mode",
             [](Config& c) {
               c.mode = Mode::StepDir;
               c.diag.diag0_step_output = true;
             }},
    Mutation{"diag1 exclusive",
             [](Config& c) {
               c.diag.diag1_steps_skipped = true;
               c.diag.diag1_dir_output = true;
             }},
    Mutation{"soft stop with stall stop",
             [](Config& c) {
               c.thresholds.cool = 10;
               c.switches.soft_stop = true;
               c.switches.stop_on_stall = true;
             }},
    Mutation{"encoder deviation", [](Config& c) { c.encoder.deviation = 0x100000; }},
    Mutation{"encoder fraction binary",
             [](Config& c) {
               c.encoder.scale.fraction = 65'535;
               c.encoder.scale.decimal = true;
             }},
    Mutation{"custom table",
             [](Config& c) {
               c.custom_table = true;
               c.table.start_sin90 = 1;
             }},
    Mutation{"table segments",
             [](Config& c) {
               c.custom_table = true;
               c.table.x1 = 0;
             }},
    Mutation{"mode", [](Config& c) { c.mode = static_cast<Mode>(7); }},
    Mutation{"verify", [](Config& c) { c.verify = static_cast<Verify>(9); }},
    Mutation{"microsteps", [](Config& c) { c.chopper.mres = static_cast<Microsteps>(12); }},
    Mutation{"chopper mode", [](Config& c) { c.chopper.mode = static_cast<ChopperMode>(3); }},
    Mutation{"ot level", [](Config& c) { c.bridge.ot_level = static_cast<OtLevel>(5); }},
    Mutation{"freewheel", [](Config& c) { c.stealth.freewheel = static_cast<Freewheel>(9); }},
};

/// Each of these must stay valid.
constexpr std::array kValid{
    Mutation{"external clock 16 MHz",
             [](Config& c) {
               c.clock.external = true;
               c.clock.hz = 16'000'000;
             }},
    Mutation{"node 254 with several nodes",
             [](Config& c) {
               c.uart.multi_node = true;
               c.uart.node = 254;
               c.uart.send_delay = 2;
             }},
    Mutation{"chain of eight",
             [](Config& c) {
               c.spi.chain_length = 8;
               c.spi.chain_index = 7;
             }},
    Mutation{"toff 1 tbl 2",
             [](Config& c) {
               c.chopper.toff = 1;
               c.chopper.tbl = 2;
             }},
    Mutation{"constant off time",
             [](Config& c) {
               c.chopper.mode = ChopperMode::ConstOffTime;
               c.chopper.hstrt = 7;
               c.chopper.hend = 15;
             }},
    Mutation{"vmax at the limit", [](Config& c) { c.ramp.vmax = (1U << 23) - 512U; }},
    Mutation{"dcstep set up",
             [](Config& c) {
               c.chopper.high_fullstep = true;
               c.chopper.high_const_off = true;
               c.dcstep.vdcmin = 512;
               c.dcstep.dc_time = 40;
             }},
    Mutation{"StepDir with diag",
             [](Config& c) {
               c.mode = Mode::StepDir;
               c.diag.diag0_error = true;
             }},
    Mutation{"supply 55 V with s2g 12",
             [](Config& c) {
               c.board.supply_mv = 55'000;
               c.protection.s2g_level = 12;
             }},
};
} // namespace

TEST_CASE("T120 the base configuration is valid and the empty one is not") {
  CHECK_EQ(Validate(BaseConfig()), Error::Ok);
  CHECK_EQ(Validate(Config{}), Error::InvalidArgument);
}

TEST_CASE("T121 every invalid mutation is rejected by Validate (table-driven)") {
  for (const Mutation& mu : kInvalid) {
    Config c = BaseConfig();
    mu.apply(c);
    if (Validate(c) == Error::Ok) {
      std::printf("    accepted: %s\n", mu.name);
    }
    CHECK(Validate(c) != Error::Ok);
  }
}

TEST_CASE("T122 every valid mutation stays valid") {
  for (const Mutation& mu : kValid) {
    Config c = BaseConfig();
    mu.apply(c);
    if (Validate(c) != Error::Ok) {
      std::printf("    rejected: %s\n", mu.name);
    }
    CHECK_EQ(Validate(c), Error::Ok);
  }
}

TEST_CASE("T123 the driver refuses an invalid configuration at start-up and sends nothing") {
  for (const Mutation& mu : kInvalid) {
    Config c = BaseConfig();
    mu.apply(c);
    SpiRig rig(c);
    const auto r = rig.drv.EnsureInitialized();
    CHECK(!r.ok());
    CHECK_EQ(rig.WireOf().transfers, 0);
    CHECK(!rig.drv.IsInitialized());
  }
}

TEST_CASE("T124 acknowledgements: each guard needs exactly its own bit") {
  struct Case {
    void (*apply)(Config&);
    Ack ack;
  };
  const Case cases[] = {
      {[](Config& c) {
         c.protection.declared = true;
         c.protection.disable_s2g = true;
       },
       Ack::ShortProtectionOff},
      {[](Config& c) {
         c.protection.declared = true;
         c.protection.disable_s2vs = true;
       },
       Ack::ShortProtectionOff},
      {[](Config& c) {
         c.bridge.declared = true;
         c.bridge.ot_level = OtLevel::C120;
       },
       Ack::LowOtLevel},
      {[](Config& c) {
         c.stealth.enable = true;
         c.switches.stop_left = true;
       },
       Ack::HardStopWithStealthChop},
      {[](Config& c) {
         c.ramp.vstart = 100;
         c.ramp.vstop = 10;
       },
       Ack::VstartAboveVstop},
  };
  for (const Case& cs : cases) {
    Config c = BaseConfig();
    cs.apply(c);
    CHECK_EQ(CheckAcks(c), Error::Refused);
    c.ack = c.ack.With(cs.ack);
    CHECK_EQ(CheckAcks(c), Error::Ok);
  }
}

TEST_CASE("T125 the register table: access, masks and the two views of address 4") {
  CHECK(reg::FindReadable(reg::kIoin) != nullptr);
  CHECK(reg::FindWritable(reg::kIoin) != nullptr); // OUTPUT shares the address
  CHECK(reg::FindWritable(reg::kIoin)->is_output);
  CHECK(reg::FindReadable(reg::kVmax) == nullptr);
  CHECK(reg::FindWritable(reg::kVmax) != nullptr);
  CHECK(reg::FindWritable(reg::kOtpProg) == nullptr);
  CHECK(reg::FindWritable(reg::kFactoryConf) == nullptr);
  CHECK(reg::FindReadable(reg::kFactoryConf) != nullptr);
  CHECK(reg::FindReadable(0x7F) == nullptr);
  CHECK(reg::FindWritable(0x7F) == nullptr);
  CHECK_EQ(reg::ResetValue(reg::kChopconf), 0x10410150U);
  CHECK_EQ(reg::ResetValue(reg::kPwmconf), 0xC40C001EU);
}

TEST_CASE("T126 the driver and the model agree on every register's writability") {
  for (const reg::RegInfo& row : reg::kTable) {
    Chip chip;
    const uint32_t before = chip.reserved_bit_writes;
    if ((row.access == reg::Access::Forbidden) || (row.access == reg::Access::ReadOnly)) {
      continue;
    }
    chip.Write(row.address, row.mask);
    CHECK_EQ(chip.reserved_bit_writes,
             before); // the driver's mask never sets a bit the model calls reserved
  }
}

TEST_CASE("T127 current arithmetic matches the datasheet vectors") {
  // 100 mOhm, GLOBALSCALER 256, CS 32: 325 mV / 100 mOhm / sqrt(2) = 2298 mA
  CHECK((units::RmsMa(100, 256, 32) >= 2297) && (units::RmsMa(100, 256, 32) <= 2299));
  CHECK_EQ(units::StealthChopLowerLimitMa(2, 0, 24'000, 6000) > 0U, true);
  CHECK_EQ(units::StealthChopLowerLimitMa(2, 0, 24'000, 0), 0U);
}

TEST_CASE("T128 velocity and acceleration conversions round-trip within one register step") {
  for (uint32_t hz = 100; hz < 200'000; hz += 1777) {
    const auto reg_value = units::VelocityRegFromHz(hz, 12'000'000);
    REQUIRE(reg_value.ok());
    const uint32_t back = units::HzFromVelocityReg(reg_value.value, 12'000'000);
    CHECK((back <= hz) && ((hz - back) <= (1U + (12'000'000U / 16'777'216U))));
  }
  CHECK(!units::VelocityRegFromHz(0xFFFFFFFFU, 12'000'000).ok());
  const auto a = units::AccelRegFromHzPerS(100'000, 12'000'000);
  CHECK(a.ok());
  CHECK(units::HzPerSFromAccelReg(a.value, 12'000'000) <= 100'000U);
  CHECK(!units::AccelRegFromHzPerS(0xFFFFFFFFU, 12'000'000).ok());
  CHECK_EQ(units::StepTimeFromVelocity(0), 0xFFFFFU);
  CHECK_EQ(units::StepTimeFromVelocity(1U << 24), 1U);
  CHECK_EQ(units::VelocityFromStepTime(0), 0U);
  CHECK_EQ(units::VelocityFromStepTime(1), 1U << 24);
}

TEST_CASE("T129 timing helpers use the datasheet formulas") {
  CHECK_EQ(units::ToffClocks(0), 24U);
  CHECK_EQ(units::ToffClocks(3), 24U + 96U);
  CHECK(units::OffTimeNs(3, 12'000'000) > 0U);
  CHECK(units::PowerDownUs(10, 12'000'000) > units::PowerDownUs(1, 12'000'000));
  CHECK(units::HoldDelayUs(15, 12'000'000) > units::HoldDelayUs(1, 12'000'000));
  CHECK(units::ZeroWaitUs(100, 12'000'000) > 0U);
  CHECK(units::StealthChopAt1Us(12'000'000) > 100'000U);
  CHECK(units::RampSteps(100'000, 20'000) > 0U);
  CHECK(units::RampTimeUs(100'000, 20'000, 12'000'000) > 0U);
  CHECK(units::DcStepVelocity(1024, 12'000'000) > 0U);
}

TEST_CASE("T130 the encoder scale helper reproduces the datasheet examples") {
  const auto s = units::EncoderScaleFor(51'200, 4000, true, false);
  REQUIRE(s.ok());
  CHECK_EQ(units::EncConstWord(s.value), (12U << 16) | 8000U);
  EncoderScale neg{.decimal = true, .integer = 25, .fraction = 6000, .negative = true};
  CHECK_EQ(units::EncConstWord(neg), 0xFFE60FA0U); // page 107: -25.6
  EncoderScale bin{.decimal = false, .integer = 1, .fraction = 0, .negative = true};
  CHECK_EQ(units::EncConstWord(bin), 0xFFFF0000U);
  CHECK(!units::EncoderScaleFor(0, 10, false, false).ok());
  CHECK(!units::EncoderScaleFor(1'000'000'000, 1, false, false).ok());
}

TEST_CASE("T131 Advise reports the soft guidance") {
  Config c = BaseConfig();
  c.chopper.toff = 1;
  c.chopper.tbl = 3;
  c.ramp.vstop = 5;
  c.stealth.lim = 3;
  c.stealth.freq = 1;
  c.clock.external = true;
  c.clock.hz = 8'000'000;
  const uint32_t bits = Advise(c);
  CHECK((bits & static_cast<uint32_t>(Advice::ToffBelow2)) != 0U);
  CHECK((bits & static_cast<uint32_t>(Advice::TblOutside)) != 0U);
  CHECK((bits & static_cast<uint32_t>(Advice::VstopBelow10)) != 0U);
  CHECK((bits & static_cast<uint32_t>(Advice::PwmLimBelow5)) != 0U);
  CHECK((bits & static_cast<uint32_t>(Advice::PwmFrequency)) != 0U);
  CHECK((bits & static_cast<uint32_t>(Advice::ExternalClock)) != 0U);
  CHECK_EQ(Advise(BaseConfig()) & static_cast<uint32_t>(Advice::ToffBelow2), 0U);
}

TEST_CASE("T132 error names and the version getters") {
  const Error all[] = {Error::Ok,
                       Error::InvalidArgument,
                       Error::NotInitialized,
                       Error::Bus,
                       Error::Crc,
                       Error::Timeout,
                       Error::Busy,
                       Error::Unsupported,
                       Error::DeviceFault,
                       Error::DeviceIdMismatch,
                       Error::UnexpectedReset,
                       Error::VerifyFailed,
                       Error::Refused,
                       Error::ModeMismatch,
                       Error::LimitExceeded};
  for (const Error e : all) {
    CHECK(!std::string_view{ToString(e)}.empty());
    CHECK(std::string_view{ToString(e)} != "Unknown");
  }
  CHECK(std::string_view{ToString(static_cast<Error>(99))} == "Unknown");
  CHECK(!std::string_view{GetDriverVersion()}.empty());
}

TEST_CASE("T133 the microstep table check accepts the default and rejects a damaged one") {
  MicrostepTable t;
  CHECK_EQ(CheckTable(t), Error::Ok);
  t.words[3] ^= 1U << 7;
  CHECK_EQ(CheckTable(t), Error::InvalidArgument);
  MicrostepTable u;
  u.x2 = 100;
  CHECK_EQ(CheckTable(u), Error::InvalidArgument);
  MicrostepTable v;
  v.words.fill(0xFFFFFFFFU); // every step +2: runs out of range
  CHECK_EQ(CheckTable(v), Error::InvalidArgument);
}

TEST_CASE("T134 the status decoders place every field") {
  const DriverStatus d =
      detail::DecodeDriver((1U << 31) | (1U << 27) | (1U << 25) | (5U << 16) | 0x155U);
  CHECK(d.standstill && d.s2g_a && d.over_temp);
  CHECK_EQ(d.sg_result, 0x155U);
  CHECK_EQ(d.cs_actual, 5U);
  const GlobalStatus g = detail::DecodeGlobal(7U);
  CHECK(g.reset && g.drv_err && g.uv_cp);
  const RampStatus r = detail::DecodeRamp(0x2000U | 0x400U | 0x200U);
  CHECK(r.stall && r.velocity_zero && r.position_reached);
  const MicrostepCurrents mc = detail::DecodeMscuract(0x01FF01FFU);
  CHECK((mc.cur_a == -1) && (mc.cur_b == -1));
}

TEST_CASE("T135 the word builders put every field where the datasheet says") {
  Config c = BaseConfig();
  c.chopper.toff = 5;
  c.chopper.hstrt = 3;
  c.chopper.hend = 4;
  c.chopper.tbl = 1;
  c.chopper.mres = Microsteps::M32;
  c.chopper.interpolate = false;
  c.chopper.double_edge = true;
  const uint32_t w = detail::ChopconfWord(c, c.chopper.toff);
  CHECK_EQ(w & 0xFU, 5U);
  CHECK_EQ((w >> 4) & 7U, 3U);
  CHECK_EQ((w >> 7) & 0xFU, 4U);
  CHECK_EQ((w >> 15) & 3U, 1U);
  CHECK_EQ((w >> 24) & 0xFU, 3U);
  CHECK_EQ((w >> 28) & 1U, 0U);
  CHECK_EQ((w >> 29) & 1U, 1U);
  CHECK_EQ(detail::ChopconfWord(c, 0U) & 0xFU, 0U);
}

TEST_CASE("T136 FindWritable rejects every forbidden or read-only address") {
  for (const reg::RegInfo& row : reg::kTable) {
    if ((row.access == reg::Access::Forbidden) || (row.access == reg::Access::ReadOnly)) {
      if (row.address != reg::kIoin) {
        CHECK(reg::FindWritable(row.address) == nullptr);
      }
    }
  }
}
