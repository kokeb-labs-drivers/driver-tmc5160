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

## What the TMC5160A does

<!-- figure: chip-capabilities -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-capabilities.dark.svg">
  <img src="chip-capabilities.svg"
    alt="Capability map of the TMC5160A: Motion controller and homing (supported): Moves to a target or runs
    at a velocity on its own ramp; stops and latches the position at switches; STEP/DIR input
    (supported): Takes step pulses from your MCU instead, interpolated to 256 microsteps by MicroPlyer;
    Motor current (supported): Sets run and hold current from the motor rating and the sense resistor,
    and powers down at standstill; StealthChop2 and SpreadCycle (supported): Silent voltage-mode
    chopping at low speed, cycle-by-cycle current chopping at high speed; StallGuard2, CoolStep and
    DcStep (supported): Senses the load without a sensor: detects a stall, saves current at light load,
    slows under overload; Encoder input (supported): Counts an ABN incremental encoder and flags a
    deviation from the commanded position; Bridge and protection (guarded): Short, overtemperature and
    gate-drive settings; a wrong value can destroy the bridge; SPI chain and UART nodes (supported):
    Several chips on one SPI daisy chain or one single wire UART, each with its own address; OTP
    programming (not covered): One-time programmable defaults in the chip; the driver reads them and
    never programs them.">
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
    Exchange(), GpioSet(CtrlPin::DrvEnn), Transfer().">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/tmc5160_spi_interface.hpp`](../../inc/tmc5160_spi_interface.hpp)
- [`inc/tmc5160_types.hpp`](../../inc/tmc5160_types.hpp)
- [`inc/tmc5160_uart_interface.hpp`](../../inc/tmc5160_uart_interface.hpp)

## TMC5160A: SPI datagram

<!-- figure: chip-frame-spi -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-frame-spi.dark.svg">
  <img src="chip-frame-spi.svg"
    alt="SPI datagram of the TMC5160A, 40 bits, most significant bit first. Write: W (bit 39), address (bits
    38 to 32), data (bits 31 to 0); Read: W (bit 39), address (bits 38 to 32), any value (ignored) (bits
    31 to 0); Reply: SPI_STATUS (bits 39 to 32), data of the previous read (or the previous write) (bits
    31 to 0). The SPI_STATUS field of the Reply row: bit 7 status_stop_r, bit 6 status_stop_l, bit 5
    position_reached, bit 4 velocity_reached, bit 3 standstill, bit 2 sg2, bit 1 driver_error, bit 0
    reset_flag. Below, the SPI mode 3 waveform of one transfer in the same bit grid.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/tmc5160_spi_interface.hpp`](../../inc/tmc5160_spi_interface.hpp)
- [`inc/tmc5160_types.hpp`](../../inc/tmc5160_types.hpp)
- [`inc/tmc5160_uart_interface.hpp`](../../inc/tmc5160_uart_interface.hpp)

## TMC5160A

<!-- figure: chip-hero -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-hero.dark.svg">
  <img src="chip-hero.svg"
    alt="Overview of the TMC5160A, a controller and driver for two-phase bipolar stepper motors: it runs the
    motion profile itself and drives an external MOSFET bridge for coil currents from 1 A to several 10
    A. A MCU talks to it over SPI or UART; it drives MOSFET bridge, then Stepper motor; it reads back
    Coil current, ABN encoder, Reference switches. Headline figures: 10 to 55 V motor and bridge supply;
    256 microsteps per full step; 8 gates for an external MOSFET bridge (two full bridges); 4 MHz SPI
    clock on the internal oscillator; 7 x 7 mm TQFP48 body; also an 8 x 8 mm QFN.">
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

## Wiring: SPI application

<!-- figure: chip-wiring-spi-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-wiring-spi-typical.dark.svg">
  <img src="chip-wiring-spi-typical.svg"
    alt="Wiring of the TMC5160A for the SPI application: MCU SPI SCK to SCK_CFG2 (Transfer()); MCU SPI MOSI
    to SDI_CFG1 (Transfer()); MCU SPI MISO to SDO_CFG0 (Transfer()); MCU GPIO / CS to CSN_CFG3
    (Transfer()); MCU GPIO to DRV_ENN (GpioSet(CtrlPin::DrvEnn)); MCU GPIO (IRQ) to DIAG0_SWN
    (GpioGet(InputPin::Diag0)); SRAH to Power bridge and sense resistors SRAH; SRAL to Power bridge and
    sense resistors SRAL; SRBH to Power bridge and sense resistors SRBH; SRBL to Power bridge and sense
    resistors SRBL. Supplies, grounds and the support parts are listed under the drawing.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)
- [`inc/tmc5160_spi_interface.hpp`](../../inc/tmc5160_spi_interface.hpp)
- [`inc/tmc5160_types.hpp`](../../inc/tmc5160_types.hpp)
- [`inc/tmc5160_uart_interface.hpp`](../../inc/tmc5160_uart_interface.hpp)

## Wiring: UART chain of two nodes

<!-- figure: chip-wiring-uart-chain -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-wiring-uart-chain.dark.svg">
  <img src="chip-wiring-uart-chain.svg"
    alt="Wiring of the TMC5160A for the UART chain of two nodes: MCU UART RX to DIAG1_SWP (Exchange()); MCU
    GPIO to DRV_ENN (GpioSet(CtrlPin::DrvEnn)); SDO_CFG0 to TMC5160 node 2 (supply and ground as node 1)
    NAI. Supplies, grounds and the support parts are listed under the drawing.">
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

## One call through the layers

<!-- figure: one-call-through-the-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="one-call-through-the-layers.dark.svg">
  <img src="one-call-through-the-layers.svg"
    alt="One call through the layers: your code calls SetCurrent(800, 240), the core exchanges frames with
    the chip through your port and returns the result to your code.">
</picture>
<!-- /figure -->

Hardware description: [`docs/model/one-call-through-the-layers.seq.yml`](../model/one-call-through-the-layers.seq.yml)

Drawn from:

- [`docs/model/one-call-through-the-layers.seq.yml`](../model/one-call-through-the-layers.seq.yml)
