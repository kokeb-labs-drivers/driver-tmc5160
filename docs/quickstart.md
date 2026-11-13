# Quickstart

From a bare chip to a moving motor: wire it, write the port, declare the motor, move it.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Analog Devices (Trinamic) TMC5160A-DS 1.18:

    - Package, pins and signal descriptions: 2 Pin Assignments, p. 11
    - Quick configuration guide: 22 Quick Configuration Guide, p. 111
    - Initialization examples: 23.1 Initialization Examples, p. 116

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## 1. Wire it

SPI is the default bus. The pins are in [Hardware](hardware.md); the minimum is SCK, MOSI, MISO, CSN, VCC_IO, ground,
the motor supply and the bridge, with `SPI_MODE` high, `SD_MODE` low and `CLK` low for the internal 12 MHz clock.
Wire DRV_ENN to a GPIO: the driver then switches the power stage in `Enable()` and `Disable()`. For UART, tie
`SPI_MODE` and `SD_MODE` low and use the single wire on DIAG1_SWP ([UART protocol](uart-protocol.md)).

Before the first power-up: the sense resistor must be the one you declare, and the motor's rated current must be the one
on its data sheet. The driver derives every current register from those two numbers.

## 2. Write the port

The port is a class with `EnsureInitialized()`, `Transfer()` and `DelayUs()`. [Porting](porting.md) has the contract.

```cpp
--8<-- "tests/docs/my_port.hpp:port"
```

## 3. Declare the motor and move it

```cpp
--8<-- "tests/docs/quickstart.cpp:quickstart"
```

What each step does:

1. `MakeConfig()` fills a `Config` with the datasheet's reset values, a run current of 85 percent of the motor's
   ceiling and a hold current of 30 percent of the run current. `Config{}` alone is not usable: it has no motor and no sense
   resistor, so `Validate()` rejects it.
2. `EnsureInitialized()` brings the bus up, reads IOIN to check the strap and the version byte (`0x30`), clears the
   reset flag, writes every register in a safe order with the driver off, and optionally verifies the writes
   (`Config::verify`). On a failure it leaves the chip with the driver off.
3. `Enable()` reads the global status first: after a chip reset or a driver fault it refuses (`UnexpectedReset`,
   `DeviceFault`), because the write-only registers would be at their reset values. On UART it also checks that the
   interface counter counted every write.
4. `MoveTo()` writes XTARGET in position mode. Poll `PositionReached()`; `Stop()`, `Hold()` and `Disable()` end the move.

## 4. Try it without hardware

The host examples run the same calls against a simulated chip: [Examples](examples.md).

## If it does not work

[Troubleshooting](troubleshooting.md) lists each error with its usual cause. The first two to check: `DeviceIdMismatch`
(wrong chip, wrong SPI mode, MISO stuck) and `ModeMismatch` (the strap does not match the bus).
