# Microstep table

The 256-entry sine table, how the register words are decoded and how a custom table is checked.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Analog Devices (Trinamic) TMC5160A-DS 1.18:

    - Sine-wave look-up table: 18 Sine-Wave Look-up Table, p. 105

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

The sequencer drives the coil currents from a table of 256 entries of one quarter of a sine wave. It is stored as eight
32-bit words MSLUT0 to MSLUT7 of 1-bit increments, with the segment boundaries in MSLUTSEL and the two start values in
MSLUTSTART (section 18.2, page 105).

## Decoding (A6)

The datasheet says on page 47 that bit n is "the difference between entry x and x+1" and on page 105 that the table
starts at START_SIN and steps up. Decoding the default words with the page 105 rule and comparing them with the formula
on page 49, `round(248 * sin(2 pi i / 1024 + pi / 1024)) - 1`, matches all 256 entries only when bit n is the
increment into entry n. The driver decodes that way; the model has a switch for the literal reading, and a test shows
both readings agree within one count. START_SIN90 is a separate field for entry 256.

## A custom table

`SetMicrostepTable()` accepts a table only when MSCNT was just read as 0, and checks it before writing: the decoded wave
must stay within -248 to 247, the segments must be ordered (X1 below X2 below X3), and START_SIN90 must be consistent.
A bad table returns `Refused` and writes nothing. DcStep needs the default table.

A table written in a previous session is not rewritten by `EnsureInitialized()` when MSCNT is not 0.

## Reading the position in the table

`ReadMicrostepCounter()` returns MSCNT (0 to 1023); `ReadMicrostepCurrents()` decodes MSCURACT into the two signed 9-bit
coil currents (A7). MSCNT equals XACTUAL modulo 1024 at 256 microsteps.
