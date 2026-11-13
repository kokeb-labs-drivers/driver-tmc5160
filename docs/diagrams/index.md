# Figures

<!-- Written by kokeb-docs figures from docs/diagrams/figures/*.py. Do not edit by hand. -->

Every figure on this site is drawn by a Python module from facts extracted from the code, so it is rebuilt,
not redrawn, when the code changes. CI runs `kokeb-docs figures --check` and fails when a
committed figure no longer matches the code. Each figure below lists the files it is drawn from.

## Inside the TMC5160A

<!-- figure: chip-blocks -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-blocks.dark.svg">
  <img src="chip-blocks.svg"
    alt="Block diagram of the TMC5160A: its function blocks, how they connect and which package pins reach
    each block.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/tmc5160_spi_interface.hpp`](../../inc/tmc5160_spi_interface.hpp)
- [`inc/tmc5160_types.hpp`](../../inc/tmc5160_types.hpp)
- [`inc/tmc5160_uart_interface.hpp`](../../inc/tmc5160_uart_interface.hpp)

## TMC5160A: SPI application

<!-- figure: chip-circuit-spi-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-circuit-spi-typical.dark.svg">
  <img src="chip-circuit-spi-typical.svg"
    alt="Schematic of the TMC5160A spi application: the chip with U2 MCU, U3 Power bridge and sense
    resistors, C1 capacitor 22 nF, C2 capacitor 100 nF, C3 capacitor 2.2 µF, C4 capacitor 2.2 µF, C5
    capacitor 470 nF, C6 capacitor 100 nF, C7 capacitor 100 nF, R1 resistor 2.2 Ω, R2 resistor 4.7 kΩ;
    the MCU wires carry the port hooks GpioGet(InputPin::Diag0), GpioSet(CtrlPin::DrvEnn), Transfer().">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/tmc5160_spi_interface.hpp`](../../inc/tmc5160_spi_interface.hpp)
- [`inc/tmc5160_types.hpp`](../../inc/tmc5160_types.hpp)
- [`inc/tmc5160_uart_interface.hpp`](../../inc/tmc5160_uart_interface.hpp)

## TMC5160A: UART chain of two nodes

<!-- figure: chip-circuit-uart-chain -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-circuit-uart-chain.dark.svg">
  <img src="chip-circuit-uart-chain.svg"
    alt="Schematic of the TMC5160A uart chain of two nodes: the chip with U2 MCU, U3 TMC5160 node 2 (supply
    and ground as node 1), R1 resistor 1 kΩ, C1 capacitor 100 nF; the MCU wires carry the port hooks
    GpioGet(InputPin::Diag1), GpioSet(CtrlPin::DrvEnn), Transfer().">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/tmc5160_spi_interface.hpp`](../../inc/tmc5160_spi_interface.hpp)
- [`inc/tmc5160_types.hpp`](../../inc/tmc5160_types.hpp)
- [`inc/tmc5160_uart_interface.hpp`](../../inc/tmc5160_uart_interface.hpp)

## TMC5160A package, TQFP-48

<!-- figure: chip-outline -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-outline.dark.svg">
  <img src="chip-outline.svg"
    alt="Outline of the TMC5160A in TQFP-48 drawn to scale from above: body 7.0 x 7.0 x 1.2 mm, pitch 0.5 mm,
    48 pins and an exposed pad.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/tmc5160_spi_interface.hpp`](../../inc/tmc5160_spi_interface.hpp)
- [`inc/tmc5160_types.hpp`](../../inc/tmc5160_types.hpp)
- [`inc/tmc5160_uart_interface.hpp`](../../inc/tmc5160_uart_interface.hpp)

## TMC5160A pinout, TQFP-48

<!-- figure: chip-pinout -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-pinout.dark.svg">
  <img src="chip-pinout.svg"
    alt="Pinout of the TMC5160A in TQFP-48, top view: 48 pins and an exposed pad, each with its name, its
    electrical type as a colour and the driver hook that uses it.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/tmc5160_spi_interface.hpp`](../../inc/tmc5160_spi_interface.hpp)
- [`inc/tmc5160_types.hpp`](../../inc/tmc5160_types.hpp)
- [`inc/tmc5160_uart_interface.hpp`](../../inc/tmc5160_uart_interface.hpp)

## The layers, one of them yours

<!-- figure: driver-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="driver-layers.dark.svg">
  <img src="driver-layers.svg"
    alt="Layer diagram: your application calls the TMC5160 core, which sends register reads and writes
    through the SPI port contract to the port class you write, which drives the chip; the core and the
    contract are the same on every MCU.">
</picture>
<!-- /figure -->

Figure module: [`docs/diagrams/figures/driver-layers.py`](../../docs/diagrams/figures/driver-layers.py)

Drawn from:

- `public calls` in [`inc/tmc5160.hpp`](../../inc/tmc5160.hpp)
- `SpiInterface` in [`inc/tmc5160_spi_interface.hpp`](../../inc/tmc5160_spi_interface.hpp)
