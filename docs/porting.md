# Porting

Run the driver on a new MCU by writing one class: the SPI port or the UART port.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Analog Devices (Trinamic) TMC5160A-DS 1.18:

    - SPI datagram, status byte and timing: 4 SPI Interface, p. 23
    - UART datagram, node address and SENDDELAY: 5.1 Datagram Structure, p. 26

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

The driver never calls your SDK. It calls hooks on a class you derive from `tmc5160::SpiInterface<Self>` or
`tmc5160::UartInterface<Self>` (in [`inc/tmc5160_spi_interface.hpp`](../inc/tmc5160_spi_interface.hpp) and
[`inc/tmc5160_uart_interface.hpp`](../inc/tmc5160_uart_interface.hpp)). The base uses CRTP and declares no hook itself;
the driver template is constrained by the `SpiPort` or `UartPort` concept, which checks each hook's exact signature and
`noexcept`, so calls are resolved at compile time (no virtual functions, nothing to allocate) and a missing or
misdeclared hook is a compile error at the point of use. A port is one bus or the other, never both.

## What you implement

| Method | Required | Contract |
| --- | --- | --- |
| `bool EnsureInitialized() noexcept` | yes | Bring the bus up on the first call; return `true` at once afterwards. `false` makes the driver's `EnsureInitialized()` return `Bus` |
| `bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept` | SPI | One full-duplex transfer of `len` bytes with CSN low for all of them; `len` is 5 times the chain length; both buffers are given. `false` on any bus error |
| `BusStatus Exchange(const uint8_t* tx, std::size_t tx_len, uint8_t* rx, std::size_t rx_len, uint32_t timeout_us) noexcept` | UART | Send `tx_len` bytes, then receive exactly `rx_len` (0 for a write). On a shared single wire read and drop the echo of your own bytes. `Ok`, `Timeout` or `Fault` |
| `void DelayUs(uint32_t us) noexcept` | yes, for every wait | Wait at least `us` microseconds |
| `bool GpioSet(CtrlPin, GpioSignal) noexcept` | no | Drive DRV_ENN (and the DcStep inputs); `false` if the pin write failed |
| `bool GpioGet(InputPin, bool& level) noexcept` | no | Read DIAG0, DIAG1 or the DcStep output; `false` if the read failed |
| `uint32_t SckHz() const noexcept` | no | The SPI clock in use, checked against the chip's limit |
| `void Log(LogLevel, const char* message) noexcept` | no | Print one line; `message` is plain text, never a format string |

## The SPI port

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

Rules the transfer must follow:

- **One call, one frame.** Keep CSN low for all `len` bytes; the chip decodes the datagram when CSN rises. For a chain,
  `len` is 5 bytes per chip. Never split a datagram into several transfers.
- **The bus requirements below**: mode 3, MSB first, up to `kMaxClockHz`. Start at 1 MHz on jumper wires.
- **Raw bytes only.** Do not interpret, retry or reorder bytes; the driver owns the framing and the retries.
- **Return `false` for a bus error.** The driver turns it into `Error::Bus` and never ignores it.
- **Bounded time-outs.** A stuck bus fails the call; it does not block forever.

## The UART port

```cpp
--8<-- "tests/docs/my_port.hpp:uart-port"
```

Rules the exchange must follow:

- **Flush first.** Discard bytes left over from an earlier timeout before sending.
- **Echo.** With TX and RX joined on one wire, read `tx_len` bytes of echo and drop them before the reply.
- **Timeout.** Wait at most `timeout_us` for the whole reply; return `Timeout` for none or a short one, `Fault` for a
  driver or hardware error. The driver adds the SENDDELAY and a margin of 64 bit times to the timeout.
- **A write has `rx_len` 0.** Return `Ok` when every byte was sent.
- **Baud rate.** Any value from 9000 up to `fCLK / 16`; declare it in `Config::uart.baud`.

## Bus requirements

The driver states what the TMC5160 needs from the bus as `constexpr` values (handbook KB-CPP-123). Check your port's
settings against them with `static_assert`, as both example ports do, so a port that clocks too fast or in the wrong
mode does not compile:

| Constant | Value | Meaning |
| --- | --- | --- |
| `kSpiMode` | `3` | SCK idles high, data latched on the rising edge (section 4.3) |
| `kMsbFirst` | `true` | Most significant bit first |
| `kMaxClockHz` | `4'000'000` | f_SCK max with the internal clock |
| `kDatagramBytes` | `5` | One datagram, 40 bits, per chip |
| `kMinCsHighNs` | `200` | CSN high between two datagrams |
| `kCsSetupNs` | `10` | CSN low to the first SCK edge |
| `kCsHoldNs` | `100` | Last SCK edge to CSN high |
| `kUartDataBits` | `8` | 8N1 |
| `kUartGapResetBits` | `63` | A longer gap between start bits resets the receiver |
| `kMaxBaudRate` | `750000` | fCLK/16 with the internal clock |
| `kTurnaroundUs` | `12500` | Last request byte to the first reply byte, at most |

## Control pins

Each control pin has a defined idle level, and the port drives it there before it enables the pin's output and before
the driver's first transfer (handbook KB-HW-018). Configuring a pin as an output first and setting its level afterwards
gives the chip a pulse it did not ask for: DRV_ENN would enable the power stage for an instant.

| `CtrlPin` | Package pin | Polarity | Idle (`INACTIVE`) level | Before the first transfer |
| --- | --- | --- | --- | --- |
| `DrvEnn` | DRV_ENN | active low | high (driver off) | Set high, then enable the output |
| `DcEnable` | ENCB_DCEN_CFG4 | active high | low | Never driven by the driver; wire it on the board |
| `DcIn` | ENCA_DCIN_CFG5 | active high | low | Never driven by the driver; wire it on the board |

`GpioSignal::ACTIVE` always means "function asserted"; the port maps it to the electrical level. A pin tied on the board
needs no hook.

## Optional hooks

The driver finds the optional hooks at compile time, in one place (`tmc5160::detail`). A missing hook falls back in a
documented, safe way or fails to compile; it never skips a step silently.

| Hook | Used for | Without it |
| --- | --- | --- |
| `DelayUs()` | The StealthChop tuning wait, ramp-down waits, UART recovery | Compile error at the first call that waits (every port needs it in practice) |
| `GpioSet()` | DRV_ENN in `Enable()` and `Disable()` | `Enable()` writes TOFF only |
| `GpioGet()` | `Diag0Active()`, `Diag1Active()` | Those calls return `Unsupported` |
| `SckHz()` | The SPI clock check at start-up | The clock is not checked |
| `Log()` | One line per lifecycle event or failure | Nothing is logged |

A port with every hook:

```cpp
--8<-- "tests/docs/ports.cpp:full-port"
```

A `GpioSet()` that returns `void`, a `GpioGet()` with the wrong level type and an `SckHz()` that is not `const` are
compile errors: the driver must see a failed pin write. The [compile-fail tests](testing.md) check each rule.

## Reference implementations

Every port lives with its example, in `examples/<platform>/`; the driver has none. [Platforms](platforms.md) lists
them and how to add one.

- [`examples/host/host_tmc5160_port.hpp`](../examples/host/host_tmc5160_port.hpp): any PC, against a simulated chip, SPI
  and UART. Start from it for a host build or a test bench; it is also the smallest complete port.
- [`examples/esp32/main/esp32_tmc5160_port.hpp`](../examples/esp32/main/esp32_tmc5160_port.hpp): ESP-IDF, SPI and UART,
  with every hook. Start from it for any ESP-IDF target.

## Check your port

1. Build it: the ports on this page are compiled by the host CI from [`tests/docs/`](../tests/docs/my_port.hpp), with the
   driver instantiated over them.
2. Call `EnsureInitialized()`: `Ok` proves that writes and reads work and that IOIN.VERSION read `0x30` with the strap
   you expect.
3. Call `ReadStepTime()` and `ReadInterfaceCounter()` (UART): they must succeed without the motor moving.
4. Call `SetCurrent()` with a small current, `Enable()` and `MoveBy(51200)` with the motor loose on the bench.

## Threads

The driver is not thread-safe. Give each driver object one owner task, or guard every call with one mutex. When several
chips share a bus, the port must also serialise the bus. See [Contract](contract.md#threading).

## Editor setup

The clang-format and clang-tidy configuration is in `_config/`, and the repository root has no copy of it. Tools
that take a path use it directly; CI does the same:

| Tool | Setting |
| --- | --- |
| Command line | `clang-format --style=file:_config/.clang-format`, `clang-tidy --config-file=_config/.clang-tidy -p build` |
| VS Code (C/C++ extension) | `"C_Cpp.clang_format_style": "file:${workspaceFolder}/_config/.clang-format"` and `"C_Cpp.codeAnalysis.clangTidy.args": ["--config-file=${workspaceFolder}/_config/.clang-tidy"]` |
| clangd (VS Code clangd, Neovim, Emacs), CLion | These find `.clang-format` and `.clang-tidy` only by name, from the file upwards: copy both to the root once (`cp _config/.clang-format _config/.clang-tidy .`). `.gitignore` lists the copies, so they never reach a commit; copy again after `_config/` changes |

Configure with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` (clangd: `--compile-commands-dir=build`) so the editor sees the
same flags as the build.
