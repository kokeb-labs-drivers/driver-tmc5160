/**
 * @file spi_chain.cpp
 * @brief The SPI daisy-chain example of the protocol page, compiled by the host build (never run).
 */
#include "my_port.hpp"
#include "tmc5160.hpp"

// --8<-- [start:spi-chain]
// Three chips in one chain: index 0 is the chip whose SDI is on MOSI.
tmc5160::Error StartChain(MyPort& port) {
  tmc5160::Config config =
      tmc5160::MakeConfig(tmc5160::Motor{.rated_rms_ma = 1000, .coil_resistance_mohm = 6000},
                          tmc5160::Board{.rsense_mohm = 100, .supply_mv = 24'000});
  config.spi.chain_length = 3;
  static tmc5160::TMC5160<MyPort> chip_a(port, [&] {
    auto c = config;
    c.spi.chain_index = 0;
    return c;
  }());
  static tmc5160::TMC5160<MyPort> chip_b(port, [&] {
    auto c = config;
    c.spi.chain_index = 1;
    return c;
  }());
  if (auto r = chip_a.EnsureInitialized(); !r) {
    return r.error;
  }
  return chip_b.EnsureInitialized().error; // every transfer is 15 bytes: 5 per chip
}
// --8<-- [end:spi-chain]
