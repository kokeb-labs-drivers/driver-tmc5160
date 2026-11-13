# SPI protocol

The 40-bit datagram, the pipelined read, the status byte, write verification and the daisy chain.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Analog Devices (Trinamic) TMC5160A-DS 1.18:

    - SPI datagram, status byte and timing: 4 SPI Interface, p. 23
    - SPI daisy chain: 4.1 SPI Datagram Structure, p. 23

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Datagram

Every transfer is 40 bits, MSB first, with CSN low for the whole datagram: one address byte (the write flag in bit 7
and a seven-bit address), then 32 data bits (section 4.1, page 23). The reply has the same shape: the SPI status byte,
then 32 data bits.

| Bit | Status byte (section 4.1.2, page 24) |
| --- | --- |
| 7 | status_stop_r |
| 6 | status_stop_l |
| 5 | position_reached |
| 4 | velocity_reached |
| 3 | standstill |
| 2 | sg2 |
| 1 | driver_error (GSTAT bit 1) |
| 0 | reset_flag (GSTAT bit 0) |

## Pipelined reads

A read takes two datagrams: the first carries the address, the second returns the data (and may carry anything, so the
driver sends another read of the same address). After a write, the next reply mirrors the data just written. The
datasheet's worked example is replayed by the tests: reading XACTUAL sends `0x2100000000` twice; writing VMAX
`0xA700ABCDEF` returns the status byte and the previous read data.

The status byte is latched at the end of an access and shown with the next transfer, so it is one transfer old (A3).
The driver never acts on a status byte as a result of the same call.

## Mode 3

The chip samples on the rising edge of SCK, with SCK idle high: SPI mode 3 (section 4.3, page 25; A1). The port's
`SckHz()` hook, when present, is checked against 4 MHz on the internal clock or `fCLK / 2` with an external one, with a
10 percent margin (A2). The examples run at 1 MHz.

A transfer of any other length than 5 bytes per chip is not committed by the chip; a clock that never idles high shifts
garbage, which the version check at start-up turns into `DeviceIdMismatch`.

## Write verification

| `Verify` | Extra traffic | Detects |
| --- | --- | --- |
| `None` | none | nothing |
| `Echo` | one transfer | the next reply must mirror the data just written |
| `ReadBack` | one or two transfers | a readable register that differs from the write |
| `Counter` | UART only | IFCNT counted the write |

Verification is per call, with a trailing `Verify` argument. SPI has no integrity check on the wire: a corrupted reply
looks like data. `Enable()` reads GSTAT to catch a brown-out reset that happened since start-up, but a corrupted GSTAT
reply can hide the flag; call `Service()` periodically.

## Daisy chain

With `chain_length` n and `chain_index` k (0 is the chip whose SDI is on MOSI, A34), a transfer is 5n bytes and this
chip's 5 bytes sit at offset `5 * (n - 1 - k)`. Every other slot carries a read of GCONF, which changes nothing.
Several `TMC5160` objects share one port object.

```cpp
--8<-- "tests/docs/spi_chain.cpp:spi-chain"
```

## Stuck lines

A MISO held low or high reads as a status byte and data of all zeros or all ones. At start-up both fail the version
check (`DeviceIdMismatch`). Later, all ones has the reset flag set and the driver reports `UnexpectedReset`; all zeros
looks like valid data with no flags. `Service()`, `ReadSnapshot()`, `ReadDriverStatus()`, `ReadStallGuard()` and the
standstill check therefore read IOIN when every word and the status byte are 0: a VERSION other than 0x30 is `Bus`, a
right one is a live chip that has no flag set at that moment. A single read of any other register is not checked.
A floating MISO reads as either.
Stuck and floating lines are open work for hardware verification.
