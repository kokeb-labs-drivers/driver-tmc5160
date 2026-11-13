# UART protocol

The single wire UART datagram, the CRC-8, the interface counter, node addressing and error recovery.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Analog Devices (Trinamic) TMC5160A-DS 1.18:

    - UART datagram, node address and SENDDELAY: 5.1 Datagram Structure, p. 26
    - UART CRC-8: 5.2 CRC Calculation, p. 28
    - Addressing multiple UART nodes: 5.4 Addressing Multiple Nodes, p. 29

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Datagram

Eight bits, no parity, one stop bit, least significant bit first. Each datagram starts with the sync nibble `1010`, so
byte 0 is `0x05`, then the node address, the register address with the write flag, four data bytes (high byte first)
and a CRC-8 (section 5.1, page 26).

| Frame | Bytes | Content |
| --- | --- | --- |
| Write | 8 | sync, node, `0x80` OR register, data[4], CRC |
| Read request | 4 | sync, node, register, CRC |
| Read reply | 8 | sync, `0xFF`, register, data[4], CRC |

The chip detects the baud rate from the sync nibble of every datagram: 9000 baud at the lowest, up to `fCLK / 16`.
A gap of more than 63 bit times between start bits resets the receiver; after an error the line must stay idle for at
least 12 bit times (page 26). The driver waits 12 bit times at the declared baud after a failed exchange (A33).

## CRC-8

The polynomial is `x^8 + x^2 + x + 1` (`0x07`), the initial value 0, bits LSB first, over every byte before the CRC
(section 5.2, page 28). The seven example datagrams of page 116 (CRC `0xD3`, `0xD8`, `0xB4`, `0x85`, `0xA3`, `0x6B`,
`0x25`) are test vectors: the tests check them against both the driver's CRC and a separately written model CRC.
A reply is checked for the sync nibble, the master address, the requested register and the CRC.

## Writes and the interface counter

A write has no reply. Each accepted write increments IFCNT (page 33), so `Verify::Counter` reads IFCNT and checks it
counted the write. `Enable()` compares IFCNT with the number of writes the driver sent: a lost write means a register the
driver thinks it wrote is still at its reset value, and `Enable()` quiesces the chip and returns `VerifyFailed`.
Writes are not retried; reads are retried twice (`kUartReadRetries`), because a read has no side effect.

## SENDDELAY and timeouts

The reply starts after SENDDELAY times 8 bit times (default 8; 0 and 1 are equal; at least 2 with several nodes). The
port's `timeout_us` is that delay plus a margin of 64 bit times at the declared baud (A32).

## Several nodes

```cpp
--8<-- "tests/docs/uart_chain.cpp:uart-nodes"
```

NODEADDR is 0 to 253; the chain procedure on page 29 programs node 254 for the first chip, so `multi_node` allows 254
(A31). `ProgramNode(from, to, nao_level)` writes NODECONF through the old address and sets the NAO output that opens
the next chip's addressing. After it the driver follows the chip to its new address. UART needs `SPI_MODE` and `SD_MODE`
low; `SWP` carries the signal and `SWN` can stay open.

## Hardware notes

The two UART pins are DIAG1_SWP and DIAG0_SWN, so DIAG outputs are not available in UART mode. With a shared
transmit and receive wire the port must read and drop the echo of its own bytes ([Porting](porting.md)).
