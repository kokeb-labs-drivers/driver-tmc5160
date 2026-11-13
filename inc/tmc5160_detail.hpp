/**
 * @file tmc5160_detail.hpp
 * @brief Compile-time helpers of the TMC5160 driver: typed integer literals, exact hook matching,
 * typed register fields and enumeration checks.
 * @copyright Copyright (c) 2026 The driver-tmc5160 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Everything here is `constexpr` or used only in unevaluated operands, so it adds no code to the
 * run-time path (handbook C++20 profile, KB-CPP20-006, -008, -020, -021, -025, -026).
 */
#ifndef TMC5160_DETAIL_HPP
#define TMC5160_DETAIL_HPP

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

namespace tmc5160::detail {

/**
 * @brief Reached only when a `_u32` literal does not fit its type: it is never
 * defined and not `constexpr`, so the constant evaluation fails and the build names it.
 */
void LiteralOutOfRange() noexcept;

/**
 * @brief An integer constant of type `uint32_t` (Rule 7.0.6): on targets where `uint32_t` is
 * `unsigned long`, a `U` literal is still `unsigned int`. Out of range does not compile.
 * @param v The literal's value.
 * @return `v` as `uint32_t`.
 */
consteval std::uint32_t operator""_u32(unsigned long long v) noexcept {
  if (v > 0xFFFF'FFFFULL) {
    LiteralOutOfRange();
  }
  return static_cast<std::uint32_t>(v);
}

/**
 * @brief Matches a port hook against exactly one signature, `R(Args...) noexcept`, as a member
 * function of `Port` (`const` or not) (KB-CPP20-006).
 *
 * `ExactHook<Port, Sig>::Take(&Port::Hook)` is well-formed only when `&Port::Hook` names a member
 * function of exactly that type: the address of an overloaded or template hook resolves to the
 * one that matches, and a hook with a narrower or different parameter, another result type or no
 * `noexcept` has no match. No value is converted. Declared only: it appears in
 * requires-expressions, never in evaluated code.
 *
 * @tparam Port The port class.
 * @tparam Sig The hook's function type, `noexcept`; a `Sig` without `noexcept` has no definition.
 */
template <typename Port, typename Sig>
  requires std::is_class_v<Port> && std::is_function_v<Sig>
struct ExactHook;

/// The `noexcept` signatures: the only ones a hook may have.
template <typename Port, typename R, typename... Args>
  requires std::is_class_v<Port>
struct ExactHook<Port, R(Args...) noexcept> {
  /// Takes the address of a non-`const` hook of exactly this signature.
  static void Take(R (Port::*hook)(Args...) noexcept) noexcept;
  /// Takes the address of a `const` hook of exactly this signature.
  static void Take(R (Port::*hook)(Args...) const noexcept) noexcept;
};

/// A class type: what `kAlwaysFalse` depends on, the port or another class of the branch.
template <typename T>
concept ClassType = std::is_class_v<T>;

/// `false` for every class `T`: the condition of a `static_assert` in a branch that must not be
/// instantiated (KB-CPP20-008).
template <ClassType>
inline constexpr bool kAlwaysFalse = false;

/// A scoped or unscoped enumeration: what the enumeration helpers take (KB-CPP20-005, -026).
template <typename E>
concept Enumeration = std::is_enum_v<E>;

/**
 * @brief The value of an enumerator in its underlying type, our stand-in for C++23
 * `std::to_underlying` (KB-CPP20-002, -026).
 */
template <Enumeration E>
[[nodiscard]] constexpr std::underlying_type_t<E> ToUnderlying(E e) noexcept {
  return static_cast<std::underlying_type_t<E>>(e);
}

/**
 * @brief The last enumerator of a contiguous enumeration that starts at 0; specialised next to each
 * enumeration that `IsEnumerator()` checks (KB-CPP20-026).
 */
template <Enumeration E>
struct EnumLast;

/**
 * @brief Whether `e` holds an enumerator of a contiguous enumeration that starts at 0. A value made
 * by `static_cast` from an integer can be anything its underlying type holds (KB-CPP20-025).
 */
template <Enumeration E>
[[nodiscard]] constexpr bool IsEnumerator(E e) noexcept {
  return static_cast<uint32_t>(ToUnderlying(e)) <=
         static_cast<uint32_t>(ToUnderlying(EnumLast<E>::kValue));
}

/**
 * @brief A bit field of a register: `Width` bits from bit `Shift` of a `Reg`. Every shift is a
 * constant below the width and every operand is widened to `uint32_t` once, by construction
 * (KB-CPP20-020, -021).
 */
template <typename Reg, uint32_t Shift, uint32_t Width>
  requires std::unsigned_integral<Reg> && (Width > 0U) && (Width < 32U) &&
           ((Shift + Width) <= static_cast<uint32_t>(std::numeric_limits<Reg>::digits))
struct Field {
  /// The largest value the field holds.
  static constexpr uint32_t kMax = (uint32_t{1} << Width) - 1U;
  /// The field's bits in the register.
  static constexpr Reg kMask = static_cast<Reg>(kMax << Shift);

  /// `reg` with the field set to `value` (masked to the field's width); the other bits unchanged.
  [[nodiscard]] static constexpr Reg Insert(Reg reg, uint32_t value) noexcept {
    return static_cast<Reg>((static_cast<uint32_t>(reg) & ~static_cast<uint32_t>(kMask)) |
                            ((value & kMax) << Shift));
  }
  /// The field's value in `reg`.
  [[nodiscard]] static constexpr uint32_t Extract(Reg reg) noexcept {
    return (static_cast<uint32_t>(reg) >> Shift) & kMax;
  }
};

/// Sync nibble and reserved bits of byte 0 of every UART datagram (section 5.1 page 26).
inline constexpr uint8_t kUartSync = 0x05;
/// The address the node puts in byte 1 of a reply (section 5.1 page 27).
inline constexpr uint8_t kUartMasterAddress = 0xFF;
/// Length of a UART write datagram and of a read reply, in bytes.
inline constexpr std::size_t kUartFrameLen = 8;
/// Length of a UART read request, in bytes.
inline constexpr std::size_t kUartRequestLen = 4;

/**
 * @brief CRC-8 of a UART datagram: polynomial x^8 + x^2 + x + 1, initial value 0, input bits
 * least significant first (section 5.2 page 28).
 * @param bytes The datagram without its CRC byte.
 * @return The CRC byte.
 */
[[nodiscard]] constexpr uint8_t Crc8(std::span<const uint8_t> bytes) noexcept {
  uint32_t crc = 0U;
  for (const uint8_t byte : bytes) {
    uint32_t current = byte;
    for (uint32_t bit = 0U; bit < 8U; ++bit) {
      const uint32_t feedback = ((crc >> 7U) ^ current) & 1U;
      crc = (crc << 1U) & 0xFFU;
      crc ^= feedback * 0x07U;
      current >>= 1U;
    }
  }
  return static_cast<uint8_t>(crc);
}

/// A UART datagram of up to eight bytes.
using UartFrame = std::array<uint8_t, kUartFrameLen>;

/**
 * @brief A UART write datagram: sync, node, `0x80 | reg`, four data bytes (high first), CRC.
 * @param node Node address.
 * @param reg Register address.
 * @param value Data.
 * @return The eight bytes.
 */
[[nodiscard]] constexpr UartFrame UartWriteFrame(uint8_t node, uint8_t reg,
                                                 uint32_t value) noexcept {
  UartFrame f{kUartSync,
              node,
              static_cast<uint8_t>(0x80U | (static_cast<uint32_t>(reg) & 0x7FU)),
              static_cast<uint8_t>(value >> 24U),
              static_cast<uint8_t>(value >> 16U),
              static_cast<uint8_t>(value >> 8U),
              static_cast<uint8_t>(value),
              0U};
  f[7] = Crc8(std::span<const uint8_t>{f}.first(7));
  return f;
}

/**
 * @brief A UART read request: sync, node, `reg`, CRC (four bytes; the rest of the frame is 0).
 * @param node Node address.
 * @param reg Register address.
 * @return The frame; the first four bytes are the request.
 */
[[nodiscard]] constexpr UartFrame UartReadFrame(uint8_t node, uint8_t reg) noexcept {
  UartFrame f{kUartSync, node, static_cast<uint8_t>(static_cast<uint32_t>(reg) & 0x7FU), 0U, 0U, 0U,
              0U,        0U};
  f[3] = Crc8(std::span<const uint8_t>{f}.first(3));
  return f;
}

/**
 * @brief The data word of a checked read reply, or nothing.
 * @param reply The eight reply bytes.
 * @param reg The register that was asked for.
 * @param[out] value The data, set only when the reply is valid.
 * @return `0` valid, `1` bad sync or address (framing), `2` CRC mismatch.
 */
[[nodiscard]] constexpr uint32_t ParseUartReply(const UartFrame& reply, uint8_t reg,
                                                uint32_t& value) noexcept {
  if ((reply[0] != kUartSync) || (reply[1] != kUartMasterAddress) ||
      (reply[2] != (static_cast<uint32_t>(reg) & 0x7FU))) {
    return 1U;
  }
  if (Crc8(std::span<const uint8_t>{reply}.first(7)) != reply[7]) {
    return 2U;
  }
  value = (static_cast<uint32_t>(reply[3]) << 24U) | (static_cast<uint32_t>(reply[4]) << 16U) |
          (static_cast<uint32_t>(reply[5]) << 8U) | static_cast<uint32_t>(reply[6]);
  return 0U;
}

// The datagrams of the datasheet's initialization example (section 23.1 page 116).
static_assert(UartWriteFrame(0x01, 0x6C, 0x000100C5U)[7] == 0xD3U);
static_assert(UartWriteFrame(0x01, 0x10, 0x00011405U)[7] == 0xD8U);
static_assert(UartWriteFrame(0x01, 0x26, 0x00001388U)[7] == 0xB4U);
static_assert(UartWriteFrame(0x01, 0x27, 0x00004E20U)[7] == 0x85U);
static_assert(UartWriteFrame(0x01, 0x20, 0x00000001U)[7] == 0xA3U);
static_assert(UartReadFrame(0x01, 0x21)[3] == 0x6BU);
static_assert(UartReadFrame(0x01, 0x22)[3] == 0x25U);

} // namespace tmc5160::detail

#endif // TMC5160_DETAIL_HPP
