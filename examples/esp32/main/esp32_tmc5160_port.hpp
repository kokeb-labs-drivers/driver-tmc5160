/**
 * @file esp32_tmc5160_port.hpp
 * @brief ESP-IDF ports for the TMC5160 driver: SPI (mode 3) and single-wire UART, both with GPIO
 * for DRV_ENN and DIAG0/DIAG1, esp_rom delays and ESP_LOG.
 *
 * Start from this file for any ESP-IDF target. The pins are in the two pin structs; the defaults
 * match the wiring table in examples/esp32/README.md (ESP32-C6 DevKitC-1). Neither port is verified
 * on hardware.
 */
#ifndef TMC5160_ESP32_TMC5160_PORT_HPP
#define TMC5160_ESP32_TMC5160_PORT_HPP

#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"

#include "tmc5160.hpp"

/// The SPI clock: 1 MHz for bring-up on jumper wires.
inline constexpr int kEsp32Tmc5160ClockHz = 1'000'000;
// The bus requirements of the driver (handbook KB-CPP-123), checked at compile time: mode 3, MSB
// first, the internal-clock SCK limit.
static_assert(kEsp32Tmc5160ClockHz <= static_cast<int>(tmc5160::kMaxSckInternalHz),
              "the default clock is above the TMC5160's f_SCK max");
static_assert(tmc5160::kSpiMode == 3, "the TMC5160 talks SPI mode 3");

namespace esp32_tmc5160_detail {

inline constexpr const char* kTag = "tmc5160";

inline void Log(tmc5160::LogLevel level, const char* message) noexcept {
  const esp_log_level_t esp_level = level == tmc5160::LogLevel::Error     ? ESP_LOG_ERROR
                                    : level == tmc5160::LogLevel::Warning ? ESP_LOG_WARN
                                    : level == tmc5160::LogLevel::Info    ? ESP_LOG_INFO
                                                                          : ESP_LOG_DEBUG;
  esp_log_write(esp_level, kTag, "%s\n", message); // plain text, never a format
}

/// The control pins: DRV_ENN (output, active low on the pin) and the DIAG inputs. -1 leaves a pin
/// unused; a DRV_ENN tied low on the board leaves TOFF 0 as the only switch-off.
struct ControlPins {
  int drv_enn = 4;
  int diag0 = 5;
  int diag1 = -1;
};

class Control {
public:
  explicit Control(const ControlPins& pins) noexcept : pins_(pins) {}

  /// Configure the pins. DRV_ENN is driven high (driver disabled) before its output is enabled, so
  /// configuring the pin never enables the power stage for an instant (handbook KB-HW-018).
  bool Configure() noexcept {
    if (pins_.drv_enn >= 0) {
      gpio_config_t out{};
      out.pin_bit_mask = 1ULL << static_cast<unsigned>(pins_.drv_enn);
      out.mode = GPIO_MODE_OUTPUT;
      if ((gpio_set_level(static_cast<gpio_num_t>(pins_.drv_enn), 1) != ESP_OK) ||
          (gpio_config(&out) != ESP_OK)) {
        return false;
      }
    }
    for (const int pin : {pins_.diag0, pins_.diag1}) {
      if (pin >= 0) {
        gpio_config_t in{};
        in.pin_bit_mask = 1ULL << static_cast<unsigned>(pin);
        in.mode = GPIO_MODE_INPUT;
        if (gpio_config(&in) != ESP_OK) {
          return false;
        }
      }
    }
    return true;
  }

  bool Set(tmc5160::CtrlPin pin, tmc5160::GpioSignal signal) const noexcept {
    if (pin != tmc5160::CtrlPin::DrvEnn) {
      return false;
    }
    if (pins_.drv_enn < 0) {
      return true; // DRV_ENN tied low on the board: nothing to drive, TOFF alone switches the
                   // bridge
    }
    // DRV_ENN is active low: ACTIVE (driver enabled) is level 0.
    const uint32_t level = signal == tmc5160::GpioSignal::ACTIVE ? 0U : 1U;
    return gpio_set_level(static_cast<gpio_num_t>(pins_.drv_enn), level) == ESP_OK;
  }

  bool Get(tmc5160::InputPin pin, bool& level) const noexcept {
    const int gpio = pin == tmc5160::InputPin::Diag0
                         ? pins_.diag0
                         : (pin == tmc5160::InputPin::Diag1 ? pins_.diag1 : -1);
    if (gpio < 0) {
      return false;
    }
    level = gpio_get_level(static_cast<gpio_num_t>(gpio)) != 0;
    return true;
  }

private:
  ControlPins pins_;
};

} // namespace esp32_tmc5160_detail

/// Pins and bus settings of the SPI port.
struct Esp32Tmc5160SpiPins {
  spi_host_device_t host = SPI2_HOST;
  int sclk = 6;
  int mosi = 7;
  int miso = 2;
  int cs = 10;
  int clock_hz = kEsp32Tmc5160ClockHz; ///< Up to `tmc5160::kMaxSckInternalHz`; checked at start.
  esp32_tmc5160_detail::ControlPins control{};
};

/// The TMC5160 SPI port on ESP-IDF, with every optional hook.
class Esp32Tmc5160SpiPort : public tmc5160::SpiInterface<Esp32Tmc5160SpiPort> {
public:
  explicit Esp32Tmc5160SpiPort(const Esp32Tmc5160SpiPins& pins = {}) noexcept
      : pins_(pins), control_(pins.control) {}

  bool EnsureInitialized() noexcept {
    if (device_ != nullptr) {
      return true;
    }
    if ((pins_.clock_hz <= 0) || (pins_.clock_hz > static_cast<int>(tmc5160::kMaxSckInternalHz))) {
      ESP_LOGE(esp32_tmc5160_detail::kTag, "SPI clock outside the TMC5160's bus requirements");
      return false;
    }
    spi_bus_config_t bus{};
    bus.mosi_io_num = pins_.mosi;
    bus.miso_io_num = pins_.miso;
    bus.sclk_io_num = pins_.sclk;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = 40; // a chain of up to 8 chips: 5 bytes each
    const esp_err_t bus_err = spi_bus_initialize(pins_.host, &bus, SPI_DMA_DISABLED);
    if ((bus_err != ESP_OK) &&
        (bus_err != ESP_ERR_INVALID_STATE)) { // INVALID_STATE: bus already up
      ESP_LOGE(esp32_tmc5160_detail::kTag, "spi_bus_initialize: %s", esp_err_to_name(bus_err));
      return false;
    }
    spi_device_interface_config_t dev{};
    dev.clock_speed_hz = pins_.clock_hz;
    dev.mode = tmc5160::kSpiMode; // mode 3: the only mode the chip understands
    dev.spics_io_num = pins_.cs;
    dev.queue_size = 1;
    if (spi_bus_add_device(pins_.host, &dev, &device_) != ESP_OK) {
      device_ = nullptr;
      return false;
    }
    return control_.Configure();
  }

  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    if ((device_ == nullptr) || (len == 0)) {
      return false;
    }
    spi_transaction_t t{};
    t.length = len * 8;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    return spi_device_polling_transmit(device_, &t) == ESP_OK;
  }

  void DelayUs(uint32_t us) noexcept {
    esp_rom_delay_us(us);
  }
  bool GpioSet(tmc5160::CtrlPin pin, tmc5160::GpioSignal signal) noexcept {
    return control_.Set(pin, signal);
  }
  bool GpioGet(tmc5160::InputPin pin, bool& level) noexcept {
    return control_.Get(pin, level);
  }
  uint32_t SckHz() const noexcept {
    return static_cast<uint32_t>(pins_.clock_hz);
  }
  void Log(tmc5160::LogLevel level, const char* message) noexcept {
    esp32_tmc5160_detail::Log(level, message);
  }

private:
  Esp32Tmc5160SpiPins pins_;
  esp32_tmc5160_detail::Control control_;
  spi_device_handle_t device_ = nullptr;
};

/// Pins and line settings of the UART port.
struct Esp32Tmc5160UartPins {
  uart_port_t uart = UART_NUM_1;
  int tx = 6;
  int rx = 7;
  int baud = 115'200; ///< The chip adapts to the baud rate from the sync byte (page 27).
  bool shared_wire =
      true; ///< TX and RX are joined: the port reads and drops the echo of its own bytes.
  esp32_tmc5160_detail::ControlPins control{};
};

/// The TMC5160 UART port on ESP-IDF (TX and RX on one wire through a resistor, or two wires).
class Esp32Tmc5160UartPort : public tmc5160::UartInterface<Esp32Tmc5160UartPort> {
public:
  explicit Esp32Tmc5160UartPort(const Esp32Tmc5160UartPins& pins = {}) noexcept
      : pins_(pins), control_(pins.control) {}

  bool EnsureInitialized() noexcept {
    if (up_) {
      return true;
    }
    uart_config_t cfg{};
    cfg.baud_rate = pins_.baud;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;
    if ((uart_param_config(pins_.uart, &cfg) != ESP_OK) ||
        (uart_set_pin(pins_.uart, pins_.tx, pins_.rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) !=
         ESP_OK) ||
        (uart_driver_install(pins_.uart, 256, 0, 0, nullptr, 0) != ESP_OK)) {
      return false;
    }
    up_ = control_.Configure();
    return up_;
  }

  tmc5160::BusStatus Exchange(const uint8_t* tx, std::size_t tx_len, uint8_t* rx,
                              std::size_t rx_len, uint32_t timeout_us) noexcept {
    if (!up_ || (tx == nullptr) || (tx_len == 0)) {
      return tmc5160::BusStatus::Fault;
    }
    static_cast<void>(
        uart_flush_input(pins_.uart)); // anything left from an earlier timeout is stale
    if ((uart_write_bytes(pins_.uart, tx, tx_len) < 0) ||
        (uart_wait_tx_done(pins_.uart, pdMS_TO_TICKS(20)) != ESP_OK)) {
      return tmc5160::BusStatus::Fault;
    }
    const TickType_t wait = pdMS_TO_TICKS((timeout_us + 999U) / 1000U) + 1U;
    uint8_t scratch[8];
    if (pins_.shared_wire) { // the joined line returns our own bytes first
      if ((tx_len > sizeof(scratch)) ||
          (uart_read_bytes(pins_.uart, scratch, tx_len, wait) != static_cast<int>(tx_len))) {
        return tmc5160::BusStatus::Fault;
      }
    }
    if (rx_len == 0) {
      return tmc5160::BusStatus::Ok;
    }
    const int got = uart_read_bytes(pins_.uart, rx, rx_len, wait);
    return got == static_cast<int>(rx_len) ? tmc5160::BusStatus::Ok : tmc5160::BusStatus::Timeout;
  }

  void DelayUs(uint32_t us) noexcept {
    esp_rom_delay_us(us);
  }
  bool GpioSet(tmc5160::CtrlPin pin, tmc5160::GpioSignal signal) noexcept {
    return control_.Set(pin, signal);
  }
  bool GpioGet(tmc5160::InputPin pin, bool& level) noexcept {
    return control_.Get(pin, level);
  }
  void Log(tmc5160::LogLevel level, const char* message) noexcept {
    esp32_tmc5160_detail::Log(level, message);
  }

private:
  Esp32Tmc5160UartPins pins_;
  esp32_tmc5160_detail::Control control_;
  bool up_ = false;
};

#endif // TMC5160_ESP32_TMC5160_PORT_HPP
