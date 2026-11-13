# Troubleshooting

Symptoms, causes and fixes, cheapest check first.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Analog Devices (Trinamic) TMC5160A-DS 1.18:

    - Global status and interface counter: 6.1 General Configuration Registers, p. 33
    - Power-up reset: 25 Power-Up Reset, p. 119

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## EnsureInitialized returns InvalidArgument

The configuration failed `Validate()` before any byte was sent. `Config{}` is not valid: build it with `MakeConfig()`
from the declared motor, the sense resistor and the supply. Check the rated current against the sense resistor
([Current and limits](current-and-limits.md)).

## EnsureInitialized returns Bus

The port reported a failure: `EnsureInitialized()` returned `false` or a transfer did. Check the bus pins in the port,
and with a logic analyser that the first datagram appears on the bus.

## EnsureInitialized returns DeviceIdMismatch

IOIN.VERSION did not read `0x30`.

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| All zeros | MISO not connected, CSN not reaching the chip, or no VCC_IO | Check continuity and the supply |
| All ones | MISO floating high, or an unpowered chip | Check VCC_IO and the pull-up |
| Wrong data | SPI mode other than 3, or CSN released inside a datagram | Use mode 3; keep CSN low for the whole 40 bits |

## EnsureInitialized returns ModeMismatch

The SD_MODE or SPI_MODE strap, as read from IOIN, does not match `Config::mode` or the bus. Fix the strap, or the
configuration ([Hardware](hardware.md)).

## UART reads return Timeout

No reply came back. Check that TX and RX are joined on one wire with a series resistor, that the node address matches
SLAVEADDR (the node is selected by the NAI/NAO chain, [UART protocol](uart-protocol.md)), and that the baud rate
is inside the chip's autodetect range. Raise `SENDDELAY` when several nodes share the wire.

## UART reads return Crc

A reply was corrupted. Reads are retried twice; a persisting `Crc` points at the wire, a missing common ground or
a baud rate the chip mis-detects.

## Enable returns UnexpectedReset

GSTAT.reset was set: the chip reset since start-up, usually a supply dip on VCC_IO. The write-only registers are lost
and the driver is uninitialised; `EnsureInitialized()` rewrites everything.

## Enable returns DeviceFault

GSTAT.drv_err is set: a short to ground or an overtemperature shutdown. Remove the cause, wait for the chip to cool,
then `ClearFaults()` and `Enable()` again.

## Enable returns VerifyFailed (UART)

The interface counter IFCNT does not match the number of writes the driver sent: a write was lost on the wire. Check the
wire, then `EnsureInitialized()`.

## A setter returns LimitExceeded or Refused

The request exceeds the declared motor, the 5 % current margin or a ramp limit, or a guard needs an acknowledgement.
Read the call's page: [Current and limits](current-and-limits.md), [Guards](guards.md).

## The motor does not move

1. Is the power stage on? `Enable()` and the DRV_ENN level.
2. Is the motor current non-zero? `SetCurrent()` and the realised currents it returns.
3. Is the ramp programmed? VMAX and AMAX of zero do not move ([Motion](motion.md)).
4. Is the mode right? In STEP/DIR mode the ramp calls return `Unsupported`.
5. Did a stall stop it? `ReleaseStall()`.

## The motor is noisy or loses steps

Tune the chopper ([Chopper](chopper.md)) and the StealthChop velocity threshold; check StallGuard and CoolStep
thresholds ([StallGuard and CoolStep](stallguard-and-coolstep.md)).
