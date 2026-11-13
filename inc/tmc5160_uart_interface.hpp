// clang-format off: the table below must keep one row per line.
/**
 * @file tmc5160_uart_interface.hpp
 * @brief The UART port of the TMC5160 driver: the CRTP base, the `UartPort` concept and the bus
 * requirements.
 * @copyright Copyright (c) 2026 The driver-tmc5160 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * The chip talks on one wire (SWP, optionally SWN for a differential line), 8N1, least significant
 * bit first, at a baud rate it detects from the sync nibble of every datagram (section 5, page 26).
 * Derive from `tmc5160::UartInterface<Self>` and implement:
 *
 * | Hook | Contract |
 * | --- | --- |
 * | `bool EnsureInitialized() noexcept` | Bring the UART up on the first call; `true` at once afterwards |
 * | `BusStatus Exchange(const uint8_t* tx, std::size_t tx_len, uint8_t* rx, std::size_t rx_len, uint32_t timeout_us) noexcept` | Send `tx_len` bytes, then receive exactly `rx_len` bytes (0 for a write). The port removes the echo of its own bytes on a shared single wire and handles transceiver direction |
 */
// clang-format on
#ifndef TMC5160_UART_INTERFACE_HPP
#define TMC5160_UART_INTERFACE_HPP

#include <concepts>
#include <cstddef>
#include <cstdint>

#include "tmc5160_detail.hpp"
#include "tmc5160_types.hpp"

namespace tmc5160 {

/**
 * @name Bus requirements
 * What the TMC5160 needs from the UART (section 5, page 26). @{
 */
/// Data bits of a frame: 8N1, one start bit and one stop bit.
inline constexpr uint8_t kUartDataBits = 8;
/// A gap of more than this many bit times between start bits resets the chip's receiver (page 26).
inline constexpr uint32_t kUartGapResetBits = 63;
/// The reply starts after SENDDELAY * 8 bit times (page 27); the port's timeout adds this margin in
/// bit times.
inline constexpr uint32_t kUartReplyMarginBits = 64;
/// Highest baud rate: fCLK / 16 (page 26); 750 kBaud with the internal 12 MHz clock.
inline constexpr uint32_t kMaxBaudRate = 750'000;
/// Last request byte to the first reply byte, at most: SENDDELAY 15 is 120 bit times, 12.5 ms at
/// the lowest baud rate of 9600 (page 27).
inline constexpr uint32_t kTurnaroundUs = 12'500;
/** @} */

/**
 * @brief CRTP base of the platform UART port.
 * @tparam Derived Your port class.
 */
template <typename Derived>
class UartInterface {
public:
  /** @brief Ports are not copied: the driver holds a reference. */
  UartInterface(const UartInterface&) = delete;
  /** @brief Ports are not copied: the driver holds a reference. */
  UartInterface& operator=(const UartInterface&) = delete;

protected:
  /** @brief Only the derived port is constructed. */
  UartInterface() noexcept = default;
  /** @brief Destroyed through the derived port only. */
  ~UartInterface() noexcept = default;
};

/**
 * @brief Satisfied by a class that derives from `UartInterface<Port>` and implements the required
 * hooks with exactly the types above.
 * @tparam Port The port class.
 */
template <typename Port>
concept UartPort = std::derived_from<Port, UartInterface<Port>> && requires {
  detail::ExactHook<Port, bool() noexcept>::Take(&Port::EnsureInitialized);
  detail::ExactHook<Port, BusStatus(const uint8_t*, std::size_t, uint8_t*, std::size_t,
                                    uint32_t) noexcept>::Take(&Port::Exchange);
};

} // namespace tmc5160

#endif // TMC5160_UART_INTERFACE_HPP
