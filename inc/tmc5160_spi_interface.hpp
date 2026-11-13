// clang-format off: the table below must keep one row per line.
/**
 * @file tmc5160_spi_interface.hpp
 * @brief The SPI port of the TMC5160 driver: the CRTP base, the `SpiPort` concept and the bus
 * requirements.
 * @copyright Copyright (c) 2026 The driver-tmc5160 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * The driver never calls an SDK. It calls hooks on a class you derive from
 * `tmc5160::SpiInterface<Self>`:
 *
 * | Hook | Contract |
 * | --- | --- |
 * | `bool EnsureInitialized() noexcept` | Bring the bus up on the first call; `true` at once afterwards |
 * | `bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept` | One full-duplex transfer, CSN low for all `len` bytes, SPI mode 3, MSB first; `len` is 5 x the chain length; both buffers are given |
 *
 * The port moves raw bytes: it never retries, never interprets a datagram and never configures the
 * chip. `DelayUs`, `GpioSet`, `GpioGet`, `SckHz` and `Log` are optional hooks described in `tmc5160.hpp`.
 */
// clang-format on
#ifndef TMC5160_SPI_INTERFACE_HPP
#define TMC5160_SPI_INTERFACE_HPP

#include <concepts>
#include <cstddef>
#include <cstdint>

#include "tmc5160_detail.hpp"
#include "tmc5160_types.hpp"

namespace tmc5160 {

/**
 * @name Bus requirements
 * What the TMC5160 needs from the SPI bus (section 4.3, page 25). A port checks its settings
 * against these with `static_assert` (handbook KB-CPP-123).
 * @{
 */
/// SPI mode: SCK idles high, data is latched on the rising edge ("usually referred to as mode 3").
inline constexpr uint8_t kSpiMode = 3;
/// Each byte is clocked most significant bit first.
inline constexpr bool kMsbFirst = true;
/// Highest SCK frequency with the internal clock, 4 MHz; with an external clock fCLK/2 less 10
/// percent.
inline constexpr uint32_t kMaxClockHz = 4'000'000;
/// A datagram is 40 bits; the whole datagram (5 bytes per chip in a chain) is one CSN low period.
inline constexpr uint32_t kDatagramBytes = 5;
/// CSN high time between datagrams is more than 2 clocks + 10 ns: 200 ns covers the slowest clock.
inline constexpr uint32_t kMinCsHighNs = 200;
/// SCK high to CSN high, t_CH before CSN high, minimum: more than one clock period + 10 ns (page
/// 25).
inline constexpr uint32_t kCsHoldNs = 100;
/// CSN low to the first SCK edge, t_CC, minimum.
inline constexpr uint32_t kCsSetupNs = 10;
/// `true`: the chip needs the 40 bits of a datagram without a CSN pulse in between.
inline constexpr bool kContinuousWord = true;
/** @} */

/**
 * @brief CRTP base of the platform SPI port.
 *
 * Derive `class MyPort : public tmc5160::SpiInterface<MyPort>` and implement the hooks in this
 * file's description. The base declares no hook itself (a forwarding hook would turn a missing
 * override into infinite recursion) and no data.
 *
 * @tparam Derived Your port class.
 */
template <typename Derived>
class SpiInterface {
public:
  /** @brief Ports are not copied: the driver holds a reference. */
  SpiInterface(const SpiInterface&) = delete;
  /** @brief Ports are not copied: the driver holds a reference. */
  SpiInterface& operator=(const SpiInterface&) = delete;

protected:
  /** @brief Only the derived port is constructed. */
  SpiInterface() noexcept = default;
  /** @brief Destroyed through the derived port only. */
  ~SpiInterface() noexcept = default;
};

/**
 * @brief Satisfied by a class that derives from `SpiInterface<Port>` and implements the required
 * hooks with exactly the types above, `noexcept` (KB-CPP20-006).
 * @tparam Port The port class.
 */
template <typename Port>
concept SpiPort = std::derived_from<Port, SpiInterface<Port>> && requires {
  detail::ExactHook<Port, bool() noexcept>::Take(&Port::EnsureInitialized);
  detail::ExactHook<Port, bool(const uint8_t*, uint8_t*, std::size_t) noexcept>::Take(
      &Port::Transfer);
};

} // namespace tmc5160

#endif // TMC5160_SPI_INTERFACE_HPP
