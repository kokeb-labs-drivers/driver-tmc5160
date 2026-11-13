# Contract

What the TMC5160 driver promises, what it needs, and the rules a caller follows.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Analog Devices (Trinamic) TMC5160A-DS 1.18:

    - Power-up reset: 25 Power-Up Reset, p. 119

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

The driver inherits the [Kokeb Labs driver contract](https://github.com/kokeb-labs/.github/blob/main/standards/driver.md);
this page states only what is specific to it. A change to anything here is a breaking change, listed under
**Breaking** in the [changelog](changelog.md).

## Promises

- The constructor is `noexcept`, stores the configuration, does no I/O and cannot fail.
- `EnsureInitialized()` is the only way to start the driver (there is no `Begin()`). Its first call validates the stored
  configuration (`InvalidArgument`, nothing sent), brings the bus up, quiesces the chip (TOFF 0, DRV_ENN inactive),
  reads IOIN and checks the version byte `0x30` (`DeviceIdMismatch`) and the strap (`ModeMismatch`), clears GSTAT and then
  writes every register from the configuration. On any failure the driver stays uninitialised with the power stage off
  and the next call starts again from the beginning; once it has succeeded, later calls return `Ok` with no I/O.
- Every call that touches the chip returns `Error::NotInitialized` before `EnsureInitialized()` succeeds. No call
  initialises or resets the chip as a side effect.
- **The current never exceeds the declared motor.** Every current register is derived from the declared motor and sense
  resistor; a request above the ceiling returns `LimitExceeded` and sends nothing. A bus failure between two writes never
  leaves a current above the ceiling.
- **Every dangerous write is guarded.** A guarded call returns an error and writes nothing; an `Ack` in the
  configuration lifts exactly one guard ([Guards](guards.md)).
- **A reset is never driven into a motor.** After a chip reset the write-only registers are at their reset values; the
  driver returns `UnexpectedReset`, becomes uninitialised, and `Enable()` refuses (reading GSTAT first) until
  `EnsureInitialized()` has rewritten everything. The way out is never closed: `Disable()`, `Stop()` of every kind,
  `Hold()` and `Deinitialize()` still send their writes after a reset report. One guard serves every write that can
  switch the bridge on: `Enable()` and any chopper write while the driver is enabled (`SetChopper()`, `SetCurrent()`,
  `SetStandstill()` and the other setters that carry CHOPCONF) read GSTAT first and, on UART, check that the interface
  counter equals the number of writes the driver sent. `ClearFaults()` and `ClearGlobalStatus()` read GSTAT before
  they clear it, so a clear never hides a reset; `Deinitialize()` switches the bridge off and reports a reset it
  finds.
- **A UART write can be lost without a reply.** The chip answers no write, so a datagram that arrives damaged is
  dropped silently. The driver sends each write once and learns of a loss only from the interface counter: a switching
  off write (`Disable()`, `Stop(Emergency)`, the first write of start-up, `ClearFaults()`) is always followed by an
  IFCNT read and returns `VerifyFailed` when the counter missed it, whatever `Verify` the caller passed (one extra
  transfer); every other UART write is unconfirmed unless the caller asks for `Verify::Counter` or `ReadBack`. A
  mismatch is reported, never resynchronised: the driver does not adopt the chip's counter, so a lost write cannot
  be hidden by the next check.
- **A stalled axis stays stopped.** With `stop_on_stall` the chip holds the stop until RAMP_STAT.event_stop_sg is
  cleared. `Service()` and `ClearRampEvents()` never clear that bit (`ClearRampEvents()` returns `Refused` for a mask
  that contains it); only `ReleaseStall()` does, after it has written Hold, so the axis does not restart towards the
  old target. `SetStopOnStall(false)` and `SetSwitches()` do not release it either: with the event latched they end the
  ramp at the present position before SW_MODE is written.
- **`InvalidArgument` means nothing was sent.** Arguments are validated before the first transfer, including the
  node address of `ProgramNode()` (254 needs `multi_node`), the table of `SetMicrostepTable()` and the sum of
  `MoveBy()`; a `MoveBy()` whose target overflows is `LimitExceeded` after one read.
- **A start-up ends a running ramp.** In the motion-controller mode `EnsureInitialized()` reads RAMP_STAT and writes
  position mode with XTARGET = XACTUAL before it rewrites the ramp registers and SW_MODE, so a chip that stayed
  powered while the MCU restarted ramps down instead of turning on. It may still be decelerating: it stops past the
  captured XTARGET, waits TZEROWAIT and runs back to it (page 83 c). `Enable()` returns `Busy` until one RAMP_STAT
  read shows vzero set, t_zerowait_active clear and, in position mode with VMAX not 0, position_reached set (page 44):
  vzero alone is also set at the instant the ramp turns back (TZEROWAIT 0, or the end of a pause with VSTART 0).
  XTARGET is written before position mode, so the chip never heads for the target of the last session. A stall stop
  of the last session is neither released nor hidden (`BeginReport::stall_latched`). On SPI, `Enable()` also reads
  IOIN and checks VERSION (a stuck line reads zeros).
- **A switch-off ends the ramp too.** `Disable()`, `Stop(Emergency)`, `ClearFaults()` and `Deinitialize()` write
  VSTART 0, VMAX 0 and velocity mode after DRV_ENN and TOFF 0, and `Enable()` returns `Busy` while RAMP_STAT.vzero
  is clear, a TZEROWAIT pause runs or a position move with a speed has not reached its target, so a bridge that is
  switched on again never resumes an old move. `SetHomePosition()`, `SetStealthChop()`, `EnterDirectMode()` and
  `ReleaseStall()` use the same check. A position move that a switch or a stall stop holds short of its target is
  `Busy` until `Stop()` of any kind or `Hold()` ends it (velocity or hold mode, or XTARGET = XACTUAL), so a homing
  loop that polls `SetHomePosition()` after a position move never waits forever once it stops the move.
  A `Stop()` that returns Ok leaves the ramp generator commanded to stop (velocity mode with VMAX 0, or position mode
  with XTARGET = XACTUAL for `HoldTarget`),
  also when the direct-mode state is in doubt (after a failed GCONF write or a failed start-up): XTARGET is then
  written 0 by the last write (VMAX 0 and velocity mode go first, so a chip in position mode is not sent to 0).
  `Hold()` keeps the speed. A reset report in the middle of a stop or switch-off sequence is returned only after its
  last write.
- **A move starts in its own direction.** `MoveTo()` writes XTARGET, then position mode, then VSTART and VMAX: after a
  stop the chip is in velocity mode with VMAX 0, where XTARGET is ignored, and position mode with VMAX 0 does not move,
  so the motor never runs in the old velocity direction or towards an older target first. `Stop(HoldTarget)` likewise
  writes XTARGET = XACTUAL before position mode; it decelerates past that position and, after TZEROWAIT, runs back to it
  (page 83 c). `RunVelocity()` writes the mode first from the other velocity mode and from position mode (no read
  there), VMAX first in the same velocity mode, and from hold mode reads VACTUAL (VMAX first at rest or running the
  asked way). A position ramp in its TZEROWAIT pause or turning back can start its run-back before the mode write
  arrives: at most VSTART*t + a*t^2/2 + (VSTART + a*t)^2/(2*AMAX) microsteps, t the time to the accepted mode write (one
  8-byte UART frame is 8.3 ms at 9600 baud), a = max(A1, AMAX) when V1 > 0 and VSTART < V1, else AMAX (page 40).
  TZEROWAIT longer than t avoids it (page 83 c). From position mode the old VMAX also applies for that one write. The
  bound counts from the call; a run-back that already moves before the call keeps its speed for that write as well, and
  t is at least one frame time (the port and the line add to it).
- **VMAX stays at or above VSTART.** `SetMaxVelocity()` refuses 0 < VMAX < VSTART (the chip's or the profile's, which
  the next move writes again) with `InvalidArgument` (page 40) and writes VSTART 0 before VMAX 0 (page 84). VMAX 0 is
  kept for later moves: `MoveTo()` and `MoveBy()` return `Refused` until `SetMaxVelocity()` or `SetRamp()` sets a
  speed, so a caller waiting for an arrival never waits on a move that cannot start.
- **SPI writes are not verified unless asked.** SPI has no integrity check on the wire and `Config::verify` is `None`
  by default: a corrupted write at start-up is not seen before `Enable()`. `Enable()` therefore sends GLOBALSCALER and
  IHOLD_IRUN again with `Verify::Echo` before it switches the bridge on (the two that set the coil current). Ask for
  `Verify::Echo` (or `ReadBack`) on the calls where it matters, or set `Config::verify`; the current registers cannot
  be read back (write-only), so `Echo` is the check.
- **Direct mode starts from zero.** `EnterDirectMode()` writes XTARGET = 0 before it sets the direct-mode bit, so a
  stale target cannot drive the coils. A direct-mode write that failed leaves the mode in doubt: motion calls are
  `Refused` and `Stop()` and `Hold()` write XTARGET = 0 until a later enter or exit succeeds. `Stop()` and `Hold()` in
  direct mode de-energise the coils (XTARGET = 0) instead of writing XACTUAL there.
- **A line stuck at one level is reported.** `ReadInputs()` and `Service()` return `Bus` for a reply the chip cannot
  produce (an SPI reply of all zeros, an IOIN whose VERSION is not 0x30 is `DeviceIdMismatch`).
- **Verify semantics.** A call sends only the traffic the datasheet needs. A check that costs more traffic is asked for
  per call with a trailing `Verify` argument: `None`, `Echo`, `ReadBack`, `Counter`. A failed check returns
  `VerifyFailed`.
- Reading a write-only register returns `Refused`; the shadow is the answer.
- Writes are not retried (a UART write is lost silently, see above); UART reads are retried twice.
- Every port failure propagates as `Error::Bus` or `Timeout`; no getter returns a stale or zero value after a failed read.
- Every wait is bounded (counts of `DelayUs()`).
- A missing `DelayUs()` is a compile error; a missing `GpioSet()` makes `Enable()` write TOFF only; a missing `Log()`
  logs nothing. A misdeclared hook is a compile error.
- `Deinitialize()` quiesces the chip once a start-up has begun, even after a reset report, and stays owed (the
  next `Disable()` still reaches the chip) if its write failed. A driver that never started sends nothing.

## Depends on

| Dependency | Version | Why |
| --- | --- | --- |
| A C++20 compiler | GCC 10, Clang 13 or later | Concepts constrain the port |
| CMake | 3.16 or later | Generates `tmc5160_version.h` |
| Your port | | The bus, within the bus requirements; the delay; optionally DRV_ENN at its idle level before the first transfer, DIAG pins, the SPI clock and logging ([Porting](porting.md)) |

Nothing else: no RTOS, no SDK, no other repository of the org.

## Threading

The driver is not thread-safe and takes no lock. Use each driver object from one task at a time, or guard every call on
it with one mutex; the layer that owns the driver owns that lock, and the bus lock when several chips share a bus.
Nothing may run in an ISR: every call can block for a bus transfer or a delay. Separate objects share no state.

## Errors

`tmc5160::Error` inside `tmc5160::Result<T>`. Values 0 to 9 are the shared base set; this driver returns every one of
them, plus five chip-specific values.

| `Error` | Value | Returned when |
| --- | --- | --- |
| `Ok` | 0 | The call succeeded |
| `NotInitialized` | 1 | `EnsureInitialized()` has not succeeded, or `Deinitialize()` ran since |
| `InvalidArgument` | 2 | A `Config` field or argument is out of range (`Validate()`); nothing was sent |
| `Bus` | 3 | A port hook returned `false` |
| `Timeout` | 4 | The chip did not answer (UART) or a bounded wait ran out |
| `DeviceIdMismatch` | 5 | IOIN.VERSION is not `0x30`: another chip, no chip or a wiring fault |
| `Crc` | 6 | The CRC-8 of a UART reply does not match (never produced on SPI) |
| `Busy` | 7 | The motor is moving: the call needs standstill |
| `DeviceFault` | 8 | The driver reports an error (GSTAT.drv_err): a short or an overtemperature |
| `Unsupported` | 9 | The hook, bus or mode the call needs is absent |
| `UnexpectedReset` | 16 | The chip reset: every write-only register is lost |
| `VerifyFailed` | 17 | A write-back, echo or interface-counter check did not match |
| `Refused` | 18 | A guard refused the call: a missing acknowledgement or an unsafe order |
| `ModeMismatch` | 19 | The SD_MODE strap or the bus does not match `Config::mode` |
| `LimitExceeded` | 20 | The request exceeds a declared motor, board or ramp limit |

The [API guide](api-guide.md#handle-errors) says what to do about each.

## Known limits

- SPI has no integrity check on the wire. A corrupted reply looks like data; `Enable()` reads GSTAT to catch a reset since
  start-up, but a corrupted GSTAT reply can hide the flag. Call `Service()` periodically.
- A MISO line stuck low is noticed by `Service()`, the status, snapshot and standstill reads and `Enable()`: an all-zero
  reply with an all-zero status byte is checked with one IOIN read, and only a wrong VERSION is `Bus` (a live chip can
  have no flag set for a moment); a read of one other register can still return zeros that look like data.
- With `Config::uart.multi_node`, node 254 is allowed because the addressing procedure of the datasheet programs it.
- The chip's automatic tuning of StealthChop is waited for with the time of the A version; a chip with a longer
  autotune leaves the tuning unfinished (A37).

## Memory

No heap, no exceptions, no RTTI, no `std::function`, no `std::string`, no `printf` and no static mutable state. A driver
object holds a reference to the port, a `Config` (288 bytes), the shadow registers and the transfer buffers (two SPI frames
of 40 bytes for a chain of 8 chips and two UART frames of 8 bytes, members of the object, never on the stack, so a port
with DMA can reach them): about 960 bytes on a 64-bit host.

## Platform

Platform-free: the same code runs on any target with a C++20 compiler, including the host tests. The ESP-IDF port in
[`examples/esp32`](examples.md) is an example, not part of the driver.

## Deviations from the standard

None.
