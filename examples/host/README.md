# TMC5160 host examples

The driver on your PC, with no hardware. The host ports in [`host_tmc5160_port.hpp`](host_tmc5160_port.hpp) connect
the real driver to a simulated TMC5160 (the register model the host tests use). Each program is one topic and exits
with 0 when everything worked.

| Program | Shows |
| --- | --- |
| `quickstart` | Declare a motor, start the driver, move to a position |
| `ramp_profile` | Ramp registers from physical units (steps per second) |
| `stealthchop` | StealthChop, the tuning wait and the SpreadCycle threshold |
| `stallguard` | StallGuard2, CoolStep, stop on stall, `ReleaseStall()` |
| `homing_switch` | Homing on a reference switch (the application owns the procedure) |
| `homing_stall` | Sensorless homing against a hard stop |
| `encoder` | Encoder scale, position and deviation |
| `uart_chain` | Two chips on one UART line, each with its own address |
| `spi_chain` | Three chips in one SPI daisy chain |
| `direct_mode` | Coil currents set directly |
| `diag_interrupt` | DIAG0 as an error output, then `Service()` |

## Build and run

From the repository root, with CMake 3.16+ and a C++20 compiler:

```bash
cmake -S examples/host -B build-example
cmake --build build-example
./build-example/tmc5160_host_quickstart
```

The host tests build this folder as well and run every program as the CTest `tmc5160_host_<name>`. Built on its own,
the folder also makes `tmc5160_host_example`, the quickstart under the name the CI job runs.
