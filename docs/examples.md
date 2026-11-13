# Examples

One folder per platform under `examples/`, each with its own port, built by CI on every pull request. The driver
itself has no platform code; [Platforms](platforms.md) lists what exists.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Analog Devices (Trinamic) TMC5160A-DS 1.18:

    - Addressing multiple UART nodes: 5.4 Addressing Multiple Nodes, p. 29
    - Homing with StallGuard: 13.4 Homing with StallGuard, p. 90

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Host

[`examples/host/`](../examples/host/README.md) runs the driver on any PC against a simulated TMC5160 (SPI and
UART), through the host port [`host_tmc5160_port.hpp`](../examples/host/host_tmc5160_port.hpp). No hardware:

```bash
cmake -S examples/host -B build-example && cmake --build build-example && ./build-example/tmc5160_host_quickstart
```

| Program | What it shows |
| --- | --- |
| `quickstart` | Declare the motor, start, enable, move, stop |
| `ramp_profile` | A six-point ramp and a position move |
| `stealthchop` | StealthChop with automatic tuning and the velocity threshold |
| `stallguard` | StallGuard2 reading and the stop on stall |
| `homing_switch` | Homing against a reference switch |
| `homing_stall` | Sensorless homing with StallGuard |
| `encoder` | The incremental encoder and its deviation check |
| `uart_chain` | Several chips on one UART, by node address |
| `spi_chain` | Several chips on one SPI bus, daisy chained |
| `direct_mode` | Direct coil-current mode, with its acknowledgement |
| `diag_interrupt` | The DIAG pins and `Service()` |

The host tests build every program with warnings as errors and run it as a CTest.

## ESP32

ESP-IDF applications; the wiring is in the [example folder's README](../examples/esp32/README.md) and the ports in
[`esp32_tmc5160_port.hpp`](../examples/esp32/main/esp32_tmc5160_port.hpp). The project points `EXTRA_COMPONENT_DIRS`
at the repository root, which is the driver's ESP-IDF component. The ESP32 builds run in CI only.

### The apps

<!-- generated: examples -->
| App | What it shows | Source | CI |
| --- | --- | --- | --- |
| `tmc5160_basic_example` :material-star:{ title="featured" } | Start the TMC5160 over SPI from menuconfig values, move one revolution each way and log the position | [`tmc5160_basic_example.cpp`](../examples/esp32/main/tmc5160_basic_example.cpp) | yes |
| `tmc5160_uart_example` | Start the TMC5160 over the single wire UART, run a velocity move and log the interface counter | [`tmc5160_uart_example.cpp`](../examples/esp32/main/tmc5160_uart_example.cpp) | yes |

Build and flash one (from `examples/esp32`, ESP-IDF environment loaded):

```bash
./scripts/build_app.sh tmc5160_basic_example Release
./scripts/flash_app.sh flash_monitor tmc5160_basic_example Release
```

Source of this table: [`examples/esp32/app_config.yml`](../examples/esp32/app_config.yml).
<!-- /generated -->

### Set up once

```bash
git submodule update --init examples/esp32/scripts   # the ESP-IDF build scripts
cd examples/esp32
. $IDF_PATH/export.sh                                 # ESP-IDF v5.5
./scripts/build_app.sh list                           # every app and build type
```

### What to read in an example

| You want to | Read |
| --- | --- |
| See the lifecycle and every `Result` checked | `tmc5160_basic_example.cpp` |
| Run over UART | `tmc5160_uart_example.cpp` |
| Write an ESP-IDF port | `esp32_tmc5160_port.hpp` |
| Use menuconfig values | `tmc5160_kconfig.hpp` and [`_config/Kconfig`](../_config/Kconfig) |

**Not verified on hardware**: the examples run against the simulated chip; bench steps are in [Testing](testing.md).
