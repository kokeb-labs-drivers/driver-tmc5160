/**
 * @file uart_chain.cpp
 * @brief The UART addressing example of the protocol page, compiled by the host build (never run).
 */
#include "my_port.hpp"
#include "tmc5160.hpp"

// --8<-- [start:uart-nodes]
// Two chips on one line: each driver object has its own node address.
tmc5160::Error StartTwoNodes(MyUartPort& port) {
  tmc5160::Config config =
      tmc5160::MakeConfig(tmc5160::Motor{.rated_rms_ma = 1000, .coil_resistance_mohm = 6000},
                          tmc5160::Board{.rsense_mohm = 100, .supply_mv = 24'000});
  config.uart.multi_node = true; // addresses 0 to 254 are valid
  config.uart.send_delay = 4;    // the reply waits 4 * 8 bit times: needed with several nodes
  tmc5160::Config second = config;
  config.uart.node = 1;
  second.uart.node = 2;
  static tmc5160::TMC5160<MyUartPort> first_motor(port, config);
  static tmc5160::TMC5160<MyUartPort> second_motor(port, second);
  if (auto r = first_motor.EnsureInitialized(); !r) {
    return r.error;
  }
  return second_motor.EnsureInitialized().error;
}
// --8<-- [end:uart-nodes]
