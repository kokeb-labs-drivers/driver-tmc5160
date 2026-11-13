/**
 * @file tmc5160_basic_example.cpp
 * @brief Start the TMC5160 from the menuconfig values, move back and forth, and log the position.
 *
 * Wiring: examples/esp32/README.md. Set the motor current, sense resistor and supply voltage in
 * menuconfig first: without them the configuration is `Config{}`, which `Validate()` rejects. The
 * motor moves 1 revolution (51200 microsteps at 256 microsteps per full step, 200 full steps) each
 * way. Not verified on hardware.
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp32_tmc5160_port.hpp"
#include "tmc5160.hpp"
#include "tmc5160_kconfig.hpp"

namespace {
constexpr const char* kTag = "tmc5160_example";
}

extern "C" void app_main(void) {
  static Esp32Tmc5160SpiPort port; // static: the driver keeps a reference
  static tmc5160::TMC5160<Esp32Tmc5160SpiPort> motor(port, tmc5160::KconfigDefaults()); // no I/O

  ESP_LOGI(kTag, "TMC5160 driver %s", tmc5160::GetDriverVersion());
  const auto init = motor.EnsureInitialized();
  if (!init) {
    ESP_LOGE(kTag, "EnsureInitialized failed: %s", tmc5160::ToString(init.error));
    return;
  }
  const auto enable = motor.Enable();
  if (!enable) {
    ESP_LOGE(kTag, "Enable failed: %s", tmc5160::ToString(enable.error));
    return;
  }

  int32_t target = 51'200;
  while (true) {
    const auto move = motor.MoveTo(target);
    if (!move) {
      ESP_LOGW(kTag, "MoveTo failed: %s", tmc5160::ToString(move.error));
      if (move.error == tmc5160::Error::UnexpectedReset) {
        // The chip lost its registers: configure it again, then switch the bridge on.
        const auto again = motor.EnsureInitialized();
        const auto on = again ? motor.Enable() : again;
        if (!on) {
          ESP_LOGE(kTag, "restart failed: %s", tmc5160::ToString(on.error));
        }
      }
    } else {
      target = -target;
    }
    for (int i = 0; i < 20; ++i) {
      vTaskDelay(pdMS_TO_TICKS(250));
      const auto position = motor.ReadPosition();
      if (position) {
        ESP_LOGI(kTag, "position %d", static_cast<int>(position.value));
      }
      const auto reached = motor.PositionReached();
      if (reached && reached.value) {
        break;
      }
    }
  }
}
