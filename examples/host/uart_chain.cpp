/**
 * @file uart_chain.cpp
 * @brief Two TMC5160 on one UART line: give each its address, then talk to each.
 *
 * Out of reset every chip answers at address 0. `ProgramNode()` writes NODECONF through the old
 * address and switches the NAO output that starts the next chip's addressing (page 29).
 */
#include <cstdio>

#include "host_tmc5160_port.hpp"

int main() {
  HostUartPort port;
  port.wire.count = 2;
  tmc5160::Config config = ExampleConfig();
  config.uart.multi_node = true;
  config.uart.send_delay = 4;

  // Each chip gets its own address first. On hardware that is ProgramNode() through address 0 one
  // chip at a time (NAO chaining, page 29); the model starts with the addresses already written.
  port.wire.ChipAt(0).Write(tmc5160_test::m::SLAVECONF, 0x400U | 1U);
  port.wire.ChipAt(1).Write(tmc5160_test::m::SLAVECONF, 0x400U | 2U);
  tmc5160::Config c1 = config;
  c1.uart.node = 1;
  tmc5160::Config c2 = config;
  c2.uart.node = 2;
  tmc5160::TMC5160<HostUartPort> first(port, c1);
  tmc5160::TMC5160<HostUartPort> second(port, c2);
  EXAMPLE_CHECK(first.EnsureInitialized());
  EXAMPLE_CHECK(second.EnsureInitialized());

  EXAMPLE_CHECK(first.SetCurrent(400, 120));
  EXAMPLE_CHECK(second.SetCurrent(600, 180));
  const auto a = first.ReadInterfaceCounter();
  const auto b = second.ReadInterfaceCounter();
  EXAMPLE_CHECK(a);
  EXAMPLE_CHECK(b);
  std::printf("node 1 IFCNT %u, node 2 IFCNT %u\n", static_cast<unsigned>(a.value),
              static_cast<unsigned>(b.value));
  return 0;
}
