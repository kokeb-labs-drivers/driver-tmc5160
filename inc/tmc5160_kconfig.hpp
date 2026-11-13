/**
 * @file tmc5160_kconfig.hpp
 * @brief Optional ESP-IDF layer: a `Config` built from the `CONFIG_TMC5160_*` Kconfig options.
 * @copyright Copyright (c) 2026 The driver-tmc5160 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Include this header only where you want menuconfig to declare the motor and the power stage; the
 * driver never includes it and no driver signature depends on it. Under ESP-IDF the values come
 * from `sdkconfig.h`. Without Kconfig `KconfigDefaults()` equals `Config{}`, which `Validate()`
 * rejects: the motor and the sense resistor have to be declared (design note, section 2.5).
 *
 * Kconfig writes a bool option set to "n" as an absent macro, so an absent
 * `CONFIG_TMC5160_STEALTHCHOP` means off in both cases.
 *
 * One configuration per program: the inline function below depends on the `CONFIG_` macros, so
 * every translation unit of one program must see the same values (ESP-IDF has one `sdkconfig.h` per
 * build; each host test is its own program). No driver or library header includes this one
 * (handbook KB-CPP20-018).
 */
#ifndef TMC5160_KCONFIG_HPP
#define TMC5160_KCONFIG_HPP

#ifdef __has_include
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif
#endif

#include "tmc5160_types.hpp"

namespace tmc5160 {

/**
 * @brief The configuration menuconfig selected.
 * @return `Config{}` when `CONFIG_TMC5160_MOTOR_RATED_MA` is not defined (no Kconfig); otherwise
 *         `MakeConfig()` of the declared motor and board with the other options applied. Check it
 * with `Validate()`.
 */
constexpr Config KconfigDefaults() noexcept {
#if defined(CONFIG_TMC5160_MOTOR_RATED_MA) && defined(CONFIG_TMC5160_RSENSE_MOHM) &&               \
    defined(CONFIG_TMC5160_SUPPLY_MV)
  Motor motor{};
  motor.rated_rms_ma = static_cast<uint32_t>(CONFIG_TMC5160_MOTOR_RATED_MA);
#ifdef CONFIG_TMC5160_MOTOR_MAX_MA
  motor.max_rms_ma = static_cast<uint32_t>(CONFIG_TMC5160_MOTOR_MAX_MA);
#endif
  Board board{};
  board.rsense_mohm = static_cast<uint32_t>(CONFIG_TMC5160_RSENSE_MOHM);
  board.supply_mv = static_cast<uint32_t>(CONFIG_TMC5160_SUPPLY_MV);
  Config c = MakeConfig(motor, board);
#if defined(CONFIG_TMC5160_RUN_MA) && (CONFIG_TMC5160_RUN_MA > 0)
  c.current.run_ma = static_cast<uint32_t>(CONFIG_TMC5160_RUN_MA);
  c.current.hold_ma = (c.current.run_ma * 3U) / 10U;
#endif
#if defined(CONFIG_TMC5160_HOLD_MA) && (CONFIG_TMC5160_HOLD_MA > 0)
  c.current.hold_ma = static_cast<uint32_t>(CONFIG_TMC5160_HOLD_MA);
#endif
#ifdef CONFIG_TMC5160_MRES
  c.chopper.mres = static_cast<Microsteps>(CONFIG_TMC5160_MRES);
#endif
#if defined(CONFIG_TMC5160_STEALTHCHOP) && CONFIG_TMC5160_STEALTHCHOP
  c.stealth.enable = true;
#endif
#ifdef CONFIG_TMC5160_UART_BAUD
  c.uart.baud = static_cast<uint32_t>(CONFIG_TMC5160_UART_BAUD);
#endif
#ifdef CONFIG_TMC5160_UART_NODE
  c.uart.node = static_cast<uint8_t>(CONFIG_TMC5160_UART_NODE);
#endif
#ifdef CONFIG_TMC5160_SPI_CHAIN_LENGTH
  c.spi.chain_length = static_cast<uint8_t>(CONFIG_TMC5160_SPI_CHAIN_LENGTH);
#endif
#ifdef CONFIG_TMC5160_SPI_CHAIN_INDEX
  c.spi.chain_index = static_cast<uint8_t>(CONFIG_TMC5160_SPI_CHAIN_INDEX);
#endif
#if defined(CONFIG_TMC5160_VERIFY_STARTUP) && CONFIG_TMC5160_VERIFY_STARTUP
  c.verify = Verify::ReadBack;
#endif
  return c;
#else
  return Config{};
#endif
}

} // namespace tmc5160

#endif // TMC5160_KCONFIG_HPP
