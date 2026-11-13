---
hide:
  - navigation
  - toc
---

<div class="kb-hero" markdown>
<span class="kb-kicker">Driver</span>

# TMC5160 Driver

<p>A C++20 driver for the TMC5160 and TMC5160A stepper motor controller and driver. It starts the chip safely,
moves the motor in physical units, and refuses the register writes that can burn out a motor or a bridge, on any MCU,
over SPI or UART.</p>

[Quickstart](quickstart.md){ .md-button .md-button--primary }
[API guide](api-guide.md){ .md-button }
[Source on GitHub](https://github.com/kokeb-labs-drivers/driver-tmc5160){ .md-button }
</div>

The driver has no platform code. You give it a small class that brings the bus up and moves bytes; it does the rest:
the start-up sequence, the datagram framing and CRC, the current arithmetic, the ramp registers, the guards and the
timeouts. Nothing here has run on a chip yet: it is tested against an independent model of the TMC5160 (see
[Testing and CI](testing.md)).

<div class="kb-chip" markdown>
<div class="kb-chip__head">
<span class="kb-chip__kicker">Stepper motor controller and driver</span>
<span class="kb-chip__name">TMC5160 / TMC5160A</span>
<span class="kb-chip__vendor">Analog Devices (Trinamic)</span>
</div>

Function
:   Ramp generator, sequencer with a 256-entry microstep table, StealthChop2, SpreadCycle, StallGuard2, CoolStep,
    DcStep, encoder input, gate drivers for two external MOSFET bridges

Interface
:   SPI mode 3 (also a daisy chain), or single wire UART with CRC-8 (also several nodes); STEP/DIR mode

Power
:   Motor supply 10 V to 55 V, I/O supply 3.0 V to 5.25 V, sense threshold 325 mV

Package
:   TQFP-48 with exposed pad (7 x 7 mm); QFN-WA variant
</div>

## What you get

- **One object per chip.** `TMC5160<YourPort>` takes a reference to your port and does no I/O until
  `EnsureInitialized()`. See [API guide](api-guide.md).
- **A current that cannot exceed the motor.** You declare the motor and the sense resistor; every register is derived
  from them. See [Current and limits](current-and-limits.md).
- **A guard for every dangerous write.** What the driver refuses, and the explicit acknowledgement that lifts a guard
  for one call. See [Guards and acknowledgements](guards.md).
- **Errors you can act on.** Every call returns `Result<T>` with an `Error` such as `Bus`, `Timeout`, `Refused` or
  `UnexpectedReset`. See [API guide](api-guide.md#handle-errors).
- **Both buses.** SPI and UART ports, daisy chains and multi-node lines, STEP/DIR mode. See
  [SPI protocol](spi-protocol.md) and [UART protocol](uart-protocol.md).
- **No heap, no exceptions, no RTTI.** The host tests build with `-fno-exceptions -fno-rtti`. See
  [Contract](contract.md).
- **Where the datasheet is unclear, a decision.** Fifty ambiguities are listed with the reading the driver takes.
  See [Ambiguities](ambiguities.md).

## Datasheet

<!-- generated: datasheet-card -->
| Document | Revision | Get it |
| --- | --- | --- |
| Datasheet: TMC5160 / TMC5160A Datasheet (Analog Devices (Trinamic) TMC5160A-DS) | 1.18, 2023-03-01 | [vendor page](https://www.analog.com/media/en/technical-documentation/data-sheets/TMC5160A_datasheet_rev1.18.pdf) |

Written against these revisions; the SHA-256 of each file is in [`docs/datasheets.yml`](datasheets.yml). Where each part
of the driver is in the datasheet: [Datasheet map](datasheet.md).
<!-- /generated -->

## Where to go next

<div class="grid cards" markdown>

-   :material-rocket-launch-outline: **[Quickstart](quickstart.md)**

    ---

    Wire the chip, write the port, declare the motor and move it in about 20 lines.

-   :material-chip: **[Hardware](hardware.md)**

    ---

    Pins, bus settings, timing and power.

-   :material-shield-check-outline: **[Guards and acknowledgements](guards.md)**

    ---

    What is refused, why, and how to lift a guard for one call.

-   :material-file-document-check-outline: **[Contract](contract.md)**

    ---

    What the driver promises, what it needs, threading and errors.

-   :material-code-braces: **[API guide](api-guide.md)**

    ---

    Start, set the current, move, tune and diagnose, task by task.

-   :material-swap-horizontal: **[Porting](porting.md)**

    ---

    Implement the SPI or UART port for your MCU.

</div>
