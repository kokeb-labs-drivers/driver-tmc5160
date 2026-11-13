/**
 * @file tmc5160_uart_example.cpp
 * @brief The same start-up over the single wire UART: read the interface counter and run a velocity
 * move.
 *
 * Wiring: examples/esp32/README.md (SD_MODE and SPI_MODE low, TX through 1 kOhm and RX on
 * DIAG1_SWP). Every accepted write increments IFCNT; the example logs it so a lost write shows up.
 * Not verified on hardware.
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp32_tmc5160_port.hpp"
#include "tmc5160.hpp"
#include "tmc5160_kconfig.hpp"

namespace {
constexpr const char* kTag = "tmc5160_uart";
}

extern "C" void app_main(void) {
  static Esp32Tmc5160UartPort port; // static: the driver keeps a reference
  static tmc5160::TMC5160<Esp32Tmc5160UartPort> motor(port, tmc5160::KconfigDefaults()); // no I/O

  ESP_LOGI(kTag, "TMC5160 driver %s over UART", tmc5160::GetDriverVersion());
  const auto init = motor.EnsureInitialized();
  if (!init) {
    ESP_LOGE(kTag, "EnsureInitialized failed: %s", tmc5160::ToString(init.error));
    return;
  }
  if (const auto enable = motor.Enable();
      !enable) { // refuses when the interface counter shows lost writes
    ESP_LOGE(kTag, "Enable failed: %s", tmc5160::ToString(enable.error));
    return;
  }
  if (const auto run = motor.RunVelocity(50'000); !run) {
    ESP_LOGE(kTag, "RunVelocity failed: %s", tmc5160::ToString(run.error));
    return;
  }
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(500));
    const auto counter = motor.ReadInterfaceCounter();
    const auto velocity = motor.ReadVelocity();
    if (counter && velocity) {
      ESP_LOGI(kTag, "IFCNT %u, velocity %d", static_cast<unsigned>(counter.value),
               static_cast<int>(velocity.value));
    } else {
      ESP_LOGW(kTag, "read failed: %s",
               tmc5160::ToString(counter ? velocity.error : counter.error));
    }
  }
}
