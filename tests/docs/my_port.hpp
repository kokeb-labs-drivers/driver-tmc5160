/**
 * @file my_port.hpp
 * @brief The SPI and UART ports shown on the Quickstart and Porting pages, compiled by the host
 * build.
 *
 * Every platform call goes through the `platform_*` functions declared below; a real port replaces
 * them with its SDK (ESP-IDF, STM32 HAL, Zephyr, Linux spidev).
 */
#ifndef TMC5160_MY_PORT_HPP
#define TMC5160_MY_PORT_HPP

#include <cstddef>
#include <cstdint>

#include "tmc5160_spi_interface.hpp"
#include "tmc5160_uart_interface.hpp"

// Your SDK's calls. Declared only: the docs build checks that the ports compile.
bool platform_spi_init() noexcept; // mode 3, MSB first, 4 MHz or less
bool platform_spi_transfer(const uint8_t* tx, uint8_t* rx,
                           std::size_t len) noexcept; // CS low for all bytes
bool platform_uart_init() noexcept;                   // 8N1, 9600 to 500000 baud
tmc5160::BusStatus platform_uart_exchange(const uint8_t* tx, std::size_t tx_len, uint8_t* rx,
                                          std::size_t rx_len, uint32_t timeout_us) noexcept;
void platform_delay_us(uint32_t us) noexcept; // busy-wait or sleep

// --8<-- [start:port]
class MyPort : public tmc5160::SpiInterface<MyPort> {
public:
  // Bring the bus up once; later calls return true at once.
  bool EnsureInitialized() noexcept {
    if (!ready_) {
      ready_ = platform_spi_init();
    }
    return ready_;
  }

  // One transfer, CS low for all `len` bytes (5 bytes per chip in a chain). `rx` is never nullptr.
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    return platform_spi_transfer(tx, rx, len);
  }

  // Wait at least `us` microseconds: the ramp-down waits, the tuning wait and the UART recovery
  // depend on it.
  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }

private:
  bool ready_ = false;
};
// --8<-- [end:port]

// --8<-- [start:uart-port]
class MyUartPort : public tmc5160::UartInterface<MyUartPort> {
public:
  bool EnsureInitialized() noexcept {
    if (!ready_) {
      ready_ = platform_uart_init();
    }
    return ready_;
  }

  // Send `tx_len` bytes, then receive exactly `rx_len` (0 for a write). On a shared single wire the
  // port reads and drops the echo of its own bytes first. Timeout: no (complete) reply within
  // `timeout_us`.
  tmc5160::BusStatus Exchange(const uint8_t* tx, std::size_t tx_len, uint8_t* rx,
                              std::size_t rx_len, uint32_t timeout_us) noexcept {
    return platform_uart_exchange(tx, tx_len, rx, rx_len, timeout_us);
  }

  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }

private:
  bool ready_ = false;
};
// --8<-- [end:uart-port]

#endif // TMC5160_MY_PORT_HPP
