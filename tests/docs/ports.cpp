/**
 * @file ports.cpp
 * @brief The ports with every optional hook, as shown on the Porting page. Compiled, never run.
 */
#include <cstddef>
#include <cstdint>

#include "my_port.hpp"
#include "tmc5160.hpp"

bool platform_gpio_write(int pin, bool high) noexcept;
bool platform_gpio_read(int pin, bool& high) noexcept;
void platform_log(const char* line) noexcept;
uint32_t platform_spi_clock_hz() noexcept;

// --8<-- [start:full-port]
class FullPort : public tmc5160::SpiInterface<FullPort> {
public:
  bool EnsureInitialized() noexcept {
    return platform_spi_init();
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    return platform_spi_transfer(tx, rx, len);
  }

  // Required by every call that waits.
  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }

  // Optional: DRV_ENN on a GPIO. ACTIVE means "driver enabled", which is a LOW level on the pin.
  bool GpioSet(tmc5160::CtrlPin pin, tmc5160::GpioSignal signal) noexcept {
    if (pin != tmc5160::CtrlPin::DrvEnn) {
      return false;
    }
    return platform_gpio_write(kDrvEnnGpio, signal == tmc5160::GpioSignal::INACTIVE);
  }

  // Optional: DIAG0 and DIAG1 as inputs. `level` is true for a high pin.
  bool GpioGet(tmc5160::InputPin pin, bool& level) noexcept {
    if (pin == tmc5160::InputPin::Diag0) {
      return platform_gpio_read(kDiag0Gpio, level);
    }
    return false;
  }

  // Optional: the SPI clock in use; the driver checks the datasheet limit of the clock source.
  [[nodiscard]] uint32_t SckHz() const noexcept {
    return platform_spi_clock_hz();
  }

  // Optional: one line per lifecycle event or failure. The driver never formats; the port does.
  void Log(tmc5160::LogLevel level, const char* message) noexcept {
    if (level > tmc5160::LogLevel::Warning) {
      return;
    }
    platform_log(message); // plain text: never use it as a format string
  }

private:
  static constexpr int kDrvEnnGpio = 4;
  static constexpr int kDiag0Gpio = 5;
};
// --8<-- [end:full-port]
