# API guide

The driver's API by task, with code that compiles: start, set the current, move, tune, watch, handle errors and stop.

## The shape of the API

| Group | Calls | Page |
| --- | --- | --- |
| Lifecycle | `EnsureInitialized()`, `IsInitialized()`, `Enable()`, `Disable()`, `Deinitialize()` | below |
| Current | `SetCurrent()` and the declared motor | [Current and limits](current-and-limits.md) |
| Motion | `MoveTo()`, `MoveBy()`, `RunVelocity()`, `Stop()`, `SetRamp()`, `PositionReached()` | [Motion](motion.md) |
| Chopper | `SetChopper()`, `SetStealthChop()`, `SetThresholds()` | [Chopper](chopper.md) |
| Load | `SetStallGuard()`, `SetStopOnStall()`, `ReleaseStall()`, CoolStep | [StallGuard and CoolStep](stallguard-and-coolstep.md) |
| Status | `Service()`, `ReadRampStatus()`, `ClearFaults()` | [Diagnostics and status](diag-and-status.md) |
| Guards | `Ack`, `Verify` | [Guards](guards.md) |

Every call that touches the chip returns `Result<T>`: check `ok()` (or the `bool` conversion) and read `value`, or
read `error`. `Result` is `[[nodiscard]]`, so an unchecked call is a compiler warning.

## Start, enable, stop

```cpp
--8<-- "tests/docs/api_tasks.cpp:lifecycle"
```

The constructor stores the port and the configuration and does no I/O. `EnsureInitialized()` is the one way to start
the chip; it leaves the power stage off. `Enable()` switches it on, and refuses after a chip reset or a driver fault.
`Config{}` is not valid: build one with `MakeConfig()` from the motor and the sense resistor ([Configuration](configuration.md)).

Once `EnsureInitialized()` has succeeded it returns `Ok` at once with no I/O. After a failure the driver stays
uninitialised and the next call starts again. Before it succeeds every call returns `NotInitialized` and sends nothing.

## Set the current

```cpp
--8<-- "tests/docs/api_tasks.cpp:current"
```

## Move

```cpp
--8<-- "tests/docs/api_tasks.cpp:motion"
```

## Quiet running and stall detection

```cpp
--8<-- "tests/docs/api_tasks.cpp:stealth"
```

```cpp
--8<-- "tests/docs/api_tasks.cpp:stall"
```

## Watch the chip

```cpp
--8<-- "tests/docs/api_tasks.cpp:diag"
```

## Dangerous writes

```cpp
--8<-- "tests/docs/api_tasks.cpp:guard"
```

## Handle errors

```cpp
--8<-- "tests/docs/api_tasks.cpp:errors"
```

| Signal | Meaning | What to do |
| --- | --- | --- |
| `Error::Bus` | The port failed a transfer or the bus bring-up | Check the port and the wiring |
| `Error::Timeout` | No UART reply, or a bounded wait ran out | Check the single wire, the node address and the baud rate |
| `Error::Crc` | A UART reply was corrupted | Check the wire and grounding; reads are retried twice, writes never |
| `Error::UnexpectedReset` | The chip reset: write-only registers are lost | `EnsureInitialized()` again |
| `Error::DeviceFault` | Short or overtemperature | Remove the cause, then `ClearFaults()` |
| `Error::Refused` | A guard stopped the call | Change the request, or acknowledge the risk in the configuration |
| `Error::LimitExceeded` | Above the motor, the board or the ramp limit | Change the request |
| `Error::ModeMismatch` | The strap or the bus does not match `Config::mode` | Fix the strap or the configuration |
| `Error::VerifyFailed` | A write-back or interface-counter check failed | Rewrite with `EnsureInitialized()`; check the wire |

Every value, with the calls that return it, is on the [Contract](contract.md#errors) page. `tmc5160::ToString()`
names an error for a log.

## Stop

```cpp
--8<-- "tests/docs/api_tasks.cpp:deinit"
```

After `Deinitialize()` the chip is quiesced and every call returns `NotInitialized` until the next
`EnsureInitialized()`.

Next: [Configuration](configuration.md).
