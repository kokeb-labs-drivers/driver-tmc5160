# TMC5160 ESP32 examples

ESP-IDF applications that run the TMC5160 driver on an ESP32-C6, built by CI on every pull request. They are not
verified on hardware: nothing here has run on a chip.

## Wiring over SPI (ESP32-C6 DevKitC-1)

| ESP32-C6 | TMC5160 | Notes |
| --- | --- | --- |
| GPIO6 | SCK_CFG2 | SPI2, mode 3, 1 MHz in the example (up to 4 MHz on the internal clock) |
| GPIO7 | SDI_CFG1 (MOSI) | |
| GPIO2 | SDO_CFG0 (MISO) | |
| GPIO10 | CSN_CFG3 | Driven by spi_master, one transfer per frame |
| GPIO4 | DRV_ENN | Set in `Esp32Tmc5160SpiPins::control`; high (driver off) before the pin becomes an output |
| GPIO5 | DIAG0_SWN | Input, with the pull-up of 47 kOhm or less the datasheet asks for |
| 3V3 | VCC_IO | 100 nF to GND at the pin |
| GND | GNDD, GNDA, pad | `SPI_MODE` high, `SD_MODE` low, `CLK` low (internal clock), `TST_MODE` low |

The motor supply (VS, 10 V to 55 V), the bridge and the sense resistors follow the datasheet's sample circuit. Set the
motor current, the sense resistor and the supply in menuconfig before the first run: the defaults (1 A motor, 100 mOhm,
24 V) are only a starting point, and the driver derives every current register from them.

## Wiring over UART

`SPI_MODE` and `SD_MODE` low. ESP32 TX goes through 1 kOhm to DIAG1_SWP, ESP32 RX goes to DIAG1_SWP directly, and
DIAG0_SWN stays open. The port reads and drops the echo of its own bytes (`shared_wire = true`); with separate TX and
RX wires set it to `false`.

## Build and flash

```bash
git submodule update --init scripts      # the ESP-IDF build scripts
. $IDF_PATH/export.sh                     # ESP-IDF v5.5
./scripts/build_app.sh list
./scripts/build_app.sh tmc5160_basic_example Debug
./scripts/flash_app.sh flash_monitor tmc5160_basic_example Debug
```

`idf.py menuconfig` shows "TMC5160 stepper driver" under Component config: the motor's rated current, the sense
resistor, the supply voltage, the run and hold current and the microstep resolution that `tmc5160::KconfigDefaults()`
returns.

## Expected output

```text
I (312) tmc5160_example: TMC5160 driver 0.1.0
tmc5160: initialised
I (820) tmc5160_example: position 0
```

An `EnsureInitialized failed: DeviceIdMismatch` line means IOIN.VERSION did not read 0x30: check CSN, the SPI mode and
the MISO wire first. `ModeMismatch` means the SD_MODE or SPI_MODE strap does not match the bus the example uses.

## Files

| File | What it is |
| --- | --- |
| [`main/tmc5160_basic_example.cpp`](main/tmc5160_basic_example.cpp) | The SPI example app |
| [`main/tmc5160_uart_example.cpp`](main/tmc5160_uart_example.cpp) | The UART example app |
| [`main/esp32_tmc5160_port.hpp`](main/esp32_tmc5160_port.hpp) | The ESP-IDF ports: SPI, UART, DRV_ENN, DIAG, delay, log |
| [`CMakeLists.txt`](CMakeLists.txt) | The ESP-IDF project; `EXTRA_COMPONENT_DIRS` points at the repository root, which is the component |
| [`app_config.yml`](app_config.yml) | The app matrix for the scripts, CI and the docs |
