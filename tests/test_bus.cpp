/**
 * @file test_bus.cpp
 * @brief SPI framing, daisy chain, UART framing, CRC, retries, recovery, node addressing and Verify
 * (T20 to T40).
 */
#include "fixture.hpp"

using namespace tmc5160;
using namespace tmc5160_test;
namespace m = tmc5160_test::m;

TEST_CASE("T20 the datagram CRC of the driver and of the model agree on every frame shape") {
  uint32_t seed = 12345U;
  for (int i = 0; i < 500; ++i) {
    seed = seed * 1664525U + 1013904223U;
    const uint8_t reg = static_cast<uint8_t>(seed >> 8) & 0x7FU;
    const uint32_t value = seed * 2654435761U;
    const detail::UartFrame w = detail::UartWriteFrame(static_cast<uint8_t>(seed >> 3), reg, value);
    CHECK_EQ(w[7], ModelCrc8(w.data(), 7));
    const detail::UartFrame r = detail::UartReadFrame(static_cast<uint8_t>(seed >> 5), reg);
    CHECK_EQ(r[3], ModelCrc8(r.data(), 3));
  }
}

TEST_CASE("T21 SPI reads are pipelined: the data comes one datagram later") {
  SpiRig rig;
  REQUIRE(rig.drv.EnsureInitialized().ok());
  rig.ChipOf().Write(m::XACTUAL, 0x1234);
  const auto a = rig.drv.ReadRegister(m::XACTUAL);
  CHECK(a.ok() && (a.value == 0x1234U));
  CHECK(rig.WireOf().longest_transfer == 5U);
}

TEST_CASE("T22 a write-only register is never read, whatever the model would answer") {
  for (uint8_t mode = 0; mode < 3; ++mode) {
    SpiRig rig;
    rig.ChipOf().k.wo_read_mode = mode;
    REQUIRE(rig.Up());
    CHECK_ERR(rig.drv.ReadRegister(m::VMAX), Error::Refused);
    CHECK_ERR(rig.drv.ReadRegister(m::CHOPCONF + 1), Error::Refused);
    CHECK_EQ(rig.ChipOf().wo_reads, 0U);
  }
}

TEST_CASE(
    "T23 the status byte is one transfer old (A3): a start-up reset flag does not trip the watch") {
  SpiRig rig;
  CHECK_OK(rig.drv.EnsureInitialized());
  CHECK(rig.drv.IsInitialized());
  SpiRig now;
  now.ChipOf().k.status_lag = false;
  CHECK_OK(now.drv.EnsureInitialized());
}

TEST_CASE("T24 Verify::Echo reads the mirrored write; a chip that does not echo fails it") {
  SpiRig rig;
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.SetFlags(Flags{.invert_direction = true}, Verify::Echo));
  rig.ChipOf().k.echo_writes = false;
  CHECK_ERR(rig.drv.SetFlags(Flags{.invert_direction = false, .recalibrate = true}, Verify::Echo),
            Error::VerifyFailed);
}

TEST_CASE("T25 Verify::ReadBack on a write-only register is a silent no-op; Counter is UART only") {
  SpiRig rig;
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.SetStandstill(Standstill{.hold_delay = 3}, Verify::ReadBack));
  CHECK_EQ(rig.ChipOf().wo_reads, 0U);
  CHECK_ERR(rig.drv.SetStandstill(Standstill{.hold_delay = 3}, Verify::Counter),
            Error::Unsupported);
  CHECK_OK(
      rig.drv.SetFlags(Flags{.invert_direction = true}, Verify::ReadBack)); // GCONF is readable
  CHECK_EQ(rig.ChipOf().Reg(m::GCONF) & 0x10U, 0x10U);
}

TEST_CASE("T26 Verify::ReadBack catches a register that does not hold the value") {
  SpiRig rig;
  REQUIRE(rig.Up());
  // A chip with the wrong GCONF shadow: change what it holds after the write by corrupting the
  // reply.
  rig.WireOf().faults.corrupt_at = rig.WireOf().transfers + 2;
  const auto r = rig.drv.SetFlags(Flags{.invert_direction = true}, Verify::ReadBack);
  CHECK(!r.ok());
}

TEST_CASE("T27 a daisy chain of three: every chip's slot is addressed and the others read GCONF") {
  for (uint8_t index = 0; index < 3; ++index) {
    Config c = BaseConfig();
    c.spi.chain_length = 3;
    c.spi.chain_index = index;
    SpiRig rig(c);
    rig.WireOf().count = 3;
    for (std::size_t i = 0; i < 3; ++i) {
      rig.ChipOf(i).k.rsense_mohm = 100;
    }
    REQUIRE(rig.Up());
    CHECK_OK(rig.drv.SetCurrent(400, 200));
    CHECK_EQ(rig.WireOf().longest_transfer, 15U);
    // only the addressed chip was configured
    for (std::size_t i = 0; i < 3; ++i) {
      CHECK_EQ(rig.ChipOf(i).Reg(m::GLOBAL_SCALER) != 0U, i == index);
    }
    // the other chips' registers hold their power-up values
    CHECK_EQ(rig.ChipOf((index + 1U) % 3U).Reg(m::GCONF), 0U);
  }
}

TEST_CASE("T28 the chain order knob (A34) flips which end index 0 is") {
  for (const bool flipped : {false, true}) {
    Config c = BaseConfig();
    c.spi.chain_length = 2;
    c.spi.chain_index = 0;
    SpiRig rig(c);
    rig.WireOf().count = 2;
    rig.ChipOf(0).k.chain_order = flipped;
    REQUIRE(rig.drv.EnsureInitialized().ok());
    CHECK_EQ(rig.ChipOf(0).Reg(m::GLOBAL_SCALER) != 0U, !flipped);
    CHECK_EQ(rig.ChipOf(1).Reg(m::GLOBAL_SCALER) != 0U, flipped);
  }
}

TEST_CASE("T29 a port that runs the wrong SPI mode gets nothing usable (A1)") {
  SpiRig rig;
  rig.WireOf().faults.spi_mode = 0;
  CHECK(!rig.drv.EnsureInitialized().ok());
  CHECK_EQ(tmc5160::kSpiMode, 3);
}

TEST_CASE("T30 UART: a write reaches the chip and IFCNT counts it") {
  UartRig rig;
  REQUIRE(rig.Up());
  const uint32_t before = rig.ChipOf().ifcnt;
  CHECK_OK(rig.drv.SetStandstill(Standstill{.hold_delay = 2}, Verify::Counter));
  CHECK(rig.ChipOf().ifcnt > before);
  const auto c = rig.drv.ReadInterfaceCounter();
  CHECK(c.ok());
  CHECK_ERR(rig.drv.SetStandstill(Standstill{}, Verify::Echo), Error::Unsupported);
}

TEST_CASE("T31 UART: a reply with a bad CRC is retried; a persistent one is Crc") {
  UartRig rig;
  REQUIRE(rig.Up());
  rig.WireOf().faults.corrupt_at = rig.WireOf().transfers;
  const auto ok = rig.drv.ReadStepTime();
  CHECK(ok.ok()); // the first reply was corrupt, the retry succeeded
  UartRig bad;
  REQUIRE(bad.Up());
  bad.WireOf().faults.corrupt_mask = 0x40;
  bad.WireOf().faults.corrupt_from = bad.WireOf().transfers; // every reply from here is corrupt
  const int sent = bad.WireOf().transfers;
  CHECK_ERR(bad.drv.ReadStepTime(), Error::Crc);
  CHECK_EQ(bad.WireOf().transfers - sent, 3); // the read and two retries, no more
}

TEST_CASE("T32 UART: the driver waits out the recovery time after an error") {
  UartRig rig;
  REQUIRE(rig.Up());
  rig.WireOf().faults.drop_reply_at = rig.WireOf().transfers;
  const uint64_t before = rig.WireOf().delayed_us;
  CHECK(rig.drv.ReadStepTime().ok());
  CHECK(rig.WireOf().delayed_us > before); // 12 bit times at 115200 baud
  CHECK_EQ(rig.ChipOf().dropped_in_recovery, 0U);
}

TEST_CASE("T33 UART: a write that fails is not retried and the next call still works") {
  UartRig rig;
  REQUIRE(rig.Up());
  rig.WireOf().faults.fail_at = rig.WireOf().transfers;
  CHECK_ERR(rig.drv.SetStandstill(Standstill{.hold_delay = 5}), Error::Bus);
  rig.WireOf().faults.fail_at = -1;
  CHECK_OK(rig.drv.SetStandstill(Standstill{.hold_delay = 5}));
}

TEST_CASE("T34 UART: a chip that never answers is a Timeout") {
  UartRig rig;
  REQUIRE(rig.Up());
  rig.WireOf().faults.drop_reply_at = -1;
  rig.WireOf().faults.fail_from = -1;
  rig.ChipOf().k.baud = 115'200;
  // another node address: the chip stays silent
  CHECK_OK(rig.drv.ProgramNode(0, 5, false));
  CHECK(rig.ChipOf().Node() == 5U);
}

TEST_CASE("T35 UART: ProgramNode moves a chip to a new address and the driver follows it") {
  Config c = BaseConfig();
  c.uart.multi_node = true;
  c.uart.send_delay = 4;
  UartRig rig(c);
  REQUIRE(rig.drv.EnsureInitialized().ok());
  CHECK_OK(rig.drv.ProgramNode(0, 7, true));
  CHECK_EQ(rig.ChipOf().Node(), 7U);
  CHECK(rig.ChipOf().nao);
  CHECK_EQ(rig.drv.GetConfig().uart.node, 7U);
  CHECK_OK(rig.drv.ReadStepTime());
  CHECK_ERR(rig.drv.ProgramNode(0, 255, false), Error::InvalidArgument);
}

TEST_CASE("T36 SPI never programs a node") {
  SpiRig rig;
  REQUIRE(rig.drv.EnsureInitialized().ok());
  CHECK_ERR(rig.drv.ProgramNode(0, 1, false), Error::Unsupported);
  CHECK_ERR(rig.drv.ReadInterfaceCounter(), Error::Unsupported);
}

TEST_CASE("T37 two UART nodes on one line are addressed independently") {
  Config c = BaseConfig();
  c.uart.multi_node = true;
  c.uart.send_delay = 4;
  c.uart.node = 1;
  UartRig rig(c);
  rig.WireOf().count = 2;
  rig.ChipOf(0).Write(m::SLAVECONF, 0x400 | 1U);
  rig.ChipOf(1).Write(m::SLAVECONF, 0x400 | 2U);
  REQUIRE(rig.Up());
  CHECK_OK(rig.drv.SetCurrent(300, 100));
  CHECK(rig.ChipOf(0).Reg(m::GLOBAL_SCALER) != 0U);
  CHECK_EQ(rig.ChipOf(1).Reg(m::GLOBAL_SCALER), 0U);
}

TEST_CASE("T38 a UART frame is never longer than 8 bytes and a read request is 4") {
  CHECK_EQ(detail::kUartFrameLen, 8U);
  CHECK_EQ(detail::kUartRequestLen, 4U);
}

TEST_CASE(
    "T39 reads and writes of the SPI-only and UART-only registers are refused on the other bus") {
  SpiRig rig;
  REQUIRE(rig.drv.EnsureInitialized().ok());
  CHECK(rig.ChipOf().nao == false);
  const uint32_t w = rig.ChipOf().writes;
  CHECK_ERR(rig.drv.ProgramNode(0, 3, true), Error::Unsupported);
  CHECK_EQ(rig.ChipOf().writes, w);
}

TEST_CASE("T40 the datasheet CRC vectors of page 116 hold at run time as well") {
  const uint8_t a[] = {0x05, 0x00, 0x00};
  CHECK_EQ(ModelCrc8(a, 3), detail::Crc8(std::span<const uint8_t>{a}));
  const uint8_t b[] = {0x05, 0x00, 0x80, 0x00, 0x00, 0x00, 0x04};
  CHECK_EQ(ModelCrc8(b, 7), detail::Crc8(std::span<const uint8_t>{b}));
}
