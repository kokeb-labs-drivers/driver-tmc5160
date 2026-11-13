# Testing and CI

How the driver is checked: host tests against an independent model of the chip, a fuzzer that corrupts what the chip sends
back, compiled documentation, an install test, ESP32 and Cortex-M builds, static analysis and measured quality gates, on
every pull request. None of it has run on a chip.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Analog Devices (Trinamic) TMC5160A-DS 1.18:

    - UART CRC-8: 5.2 CRC Calculation, p. 28

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Host tests

The tests build the real driver against a model of the TMC5160 (`tests/mock_tmc5160.hpp`): the register file with its
reset values and write-only registers, the ramp generator (in physical units, with its own arithmetic), the sequencer
and microstep counter, StallGuard2 and short, overtemperature and charge-pump injection, the reference switches, the
encoder, DIAG outputs, the SPI front end (pipelined reads, status byte, daisy chain) and the UART front end (frames,
CRC, IFCNT, SENDDELAY, 12-bit recovery). The bus layer (`tests/mock_tmc5160_bus.hpp`) fails one transfer, every transfer
from some index, corrupts a reply, drops a UART reply, or holds the line at one level. They build with the firmware's
flags, `-fno-exceptions -fno-rtti`, and warnings as errors, on GCC and Clang.

```bash
cmake -S . -B build -DTMC5160_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

| File | Covers |
| --- | --- |
| [`tests/test_lifecycle.cpp`](../tests/test_lifecycle.cpp) | Construction, `EnsureInitialized()`, strap and version checks, `Enable()`, `Disable()`, `ClearFaults()` and the fault paths |
| [`tests/test_bus.cpp`](../tests/test_bus.cpp) | SPI framing and chain, UART framing, CRC, IFCNT, recovery, node programming, write verification, the datasheet vectors |
| [`tests/test_current.cpp`](../tests/test_current.cpp) | The current plan, the ceiling, the sense-resistor bound, the write order of `SetCurrent()` |
| [`tests/test_motion.cpp`](../tests/test_motion.cpp) | The ramp registers, moves, stops, switches, homing primitives, StealthChop sequencing, StallGuard2, CoolStep, DcStep, encoder, direct mode |
| [`tests/test_core.cpp`](../tests/test_core.cpp) | `Validate()` as a mutation table of invalid and valid configurations, the units, the microstep table, the registers |
| [`tests/test_stops.cpp`](../tests/test_stops.cpp) | How motion ends and when the bridge may be energised again: the stop after a failed start-up, the switch-off ramp stop, the TZEROWAIT pause, and the read order of the standstill check against a chip that runs on while each transfer is on the line (`Wire::xfer_us`) |
| [`tests/test_scenarios.cpp`](../tests/test_scenarios.cpp) | Whole sessions over both buses: start-up, motion, fault, brown-out, recovery |
| [`tests/test_knobs.cpp`](../tests/test_knobs.cpp) | One test per ambiguity knob ([Ambiguities](ambiguities.md)) |
| [`tests/test_kconfig.cpp`](../tests/test_kconfig.cpp) | `KconfigDefaults()` with and without `CONFIG_` values |
| [`tests/test_version.cpp`](../tests/test_version.cpp) | The version getters follow the generated header |
| [`tests/fuzz/`](../tests/fuzz/fuzz_tmc5160.cpp) | The fuzz target; CTest replays its corpus (`tmc5160_fuzz_tmc5160_corpus`), see below |
| [`tests/compile_fail/`](../tests/compile_fail/port_without_delay.cpp) | Code that must not compile, each for its `EXPECT:` diagnostic: a port without `DelayUs()`, a `void GpioSet()`, a wrong `GpioGet()` level type, a `DelayUs(uint16_t)`, an `SckHz()` that is not `const`, an `Exchange()` with a narrowed parameter, a port with both buses, a hook without `noexcept`, an invalid `CheckedConfig`, a class that is not a port |
| [`tests/docs/`](../tests/docs/my_port.hpp) | Every code block of this site, compiled |
| [`examples/host/`](../examples/host/README.md) | Eleven host examples, built with the test flags and run (`tmc5160_host_<name>`) |
| [`tests/check_install.cmake`](../tests/check_install.cmake) | Installs into a scratch prefix and builds [`tests/install_consumer/`](../tests/install_consumer/CMakeLists.txt) with `find_package(tmc5160)` |
| [`tests/check_doxygen.cmake`](../tests/check_doxygen.cmake) | Doxygen with `_config/Doxyfile` and warnings as errors, when `doxygen` is installed |

### The model stands alone

The model is written from the datasheet, not from the driver: it does not include `tmc5160_registers.hpp` or
`tmc5160_types.hpp`. Its register addresses (namespace `tmc5160_test::m`), field masks, reset values, ramp arithmetic and
CRC-8 are its own, so a wrong constant in the driver cannot hide behind the same wrong constant in the model
(handbook KB-TST-030). The datasheet's own vectors are test cases: the seven CRC datagrams of page 116, the SPI worked
example of page 24, the microstep table formula of page 49 and the encoder constants of page 108.

Every behaviour that was ambiguous in the datasheet is a switch of the model ([Ambiguities](ambiguities.md)), so a test
runs the driver against each reading. The model is behavioural: it does not claim a bit-exact trajectory (A22).

## Fuzz tests

Everything the driver reads comes from the chip over the bus, so the data read is what a noisy cable, a wrong part or a
broken chip controls. [`tests/fuzz/fuzz_tmc5160.cpp`](../tests/fuzz/fuzz_tmc5160.cpp) lets libFuzzer choose the bus (SPI
or UART), then up to 30 driver operations, and on every transfer what the bus does: pass it to the model, fail it, flip
bits, replace the reply or corrupt the request. Between calls it changes the chip: the version, the strap, a short, an
overtemperature, a stall, the reference switches, a brown-out. After every call it checks sixteen invariants and aborts,
naming the line, when one breaks. Among them:

- a failed call returns an `Error` the enum defines;
- before `EnsureInitialized()` no call makes a transfer, and `InvalidArgument` means nothing was sent;
- the model's coil current never exceeded the rated current (F8): on SPI, which has no integrity check, a brown-out
  followed by a corrupted read is exempt, and the limitation is documented;
- the driver is never enabled without being initialised, and a latched fault implies initialisation;
- read-only and forbidden registers are never written, and OTP_PROG and FACTORY_CONF never reach the chip;
- `UnexpectedReset` leaves the driver uninitialised;
- a refusal never leaves the bridge on above the rating (F13), and a start-up that failed after its first write leaves
  the driver off and not started, and TOFF 0 in the chip unless the error is a bus error (F14);
- `Deinitialize()` always leaves the driver uninitialised, and `ProgramNode()` on SPI is `Unsupported`;
- every log line is a non-empty plain-text message.

```bash
# libFuzzer needs Clang (on macOS, Homebrew's LLVM)
cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DTMC5160_BUILD_TESTS=ON -DTMC5160_BUILD_FUZZERS=ON
cmake --build build-fuzz --target fuzz_tmc5160
./build-fuzz/tests/fuzz/fuzz_tmc5160 -max_total_time=120 tests/fuzz/corpus/fuzz_tmc5160
```

The same file also builds without libFuzzer, and CTest replays every input in
[`tests/fuzz/corpus/fuzz_tmc5160/`](../tests/fuzz/corpus/fuzz_tmc5160) on GCC and Clang
(`tmc5160_fuzz_tmc5160_corpus`): the seeds, and every input that ever broke a promise (name it `regress-*`), so a fixed
bug stays fixed. The fuzzer found three real hazards during development (a brown-out followed by `Enable()`, lost UART
writes, and a failure between the two current writes); each is now a test.

## Mutation check

The review of this driver ran fifty hand-made faults (an operator, a shift, a deleted guard) against the tests; the
tests in `tests/test_mutants.cpp` and the strengthened older ones kill every one of them except three that cannot be
observed: `GCONF` test mode (the bit is never reachable from a `Config`), the SPI refusal of `NODECONF` (the address is
refused earlier, in the argument check) and the `MoveTo()` ramp-written check (start-up always writes the ramp). A
fourth, a wrong `RAMP_STAT` clear mask, is now a `static_assert` that ties the mask to the register table. Six more
faults found while fixing the review (the stall-clear mask, the reset gate, the counter resync, the off-verify, the
direct-mode target and the stuck-line check) are killed too. The standstill check of `Enable()` is one RAMP_STAT read
(vzero and t_zerowait_active); T257 polls it against a chip that runs on while each transfer is on the line
(`Wire::xfer_us`) and kills a missing vzero check and a missing t_zerowait_active check. The order of the motion writes
is tested the same way: T258 to T260 (`MoveTo()` after a stop or a switch-off, `Stop(HoldTarget)` and the start-up
stop, `RunVelocity()` from a move the other way) and the timed part of T256 (the switch-off stop with the mode in
doubt) kill each old write order. T261 kills the removal of any word from the stuck-line checks of `Service()`,
`ReadSnapshot()`, `ReadDriverStatus()` and `ReadStallGuard()`, and the IOIN.VERSION tie-break; T265 a failed IOIN
read. The model's position mode keeps its moving direction while it stops, so a stop overshoots the target and runs
back as the chip does (page 83 c). T262 sweeps the start-up stop with TZEROWAIT 0 and kills a standstill check without
position_reached; T263 kills one that asks for it outside position mode or with VMAX 0. T264 (`MoveTo()` from velocity
mode and the VSTART restore), T266 (`SetHomePosition()` order) and T267 (`RunVelocity()` from a running position move)
are timed too. T268 (`RunVelocity()` during the pause of a HoldTarget stop) and T277 (during the overshoot of one,
TZEROWAIT 0) kill the speed-first order from position mode and an extra VACTUAL read there; T279 compares the measured
run-back with the documented bound for five ramps (V1 0; MoveConfig(0); VSTART below V1; VSTART above V1; A1 above AMAX
below V1, so A1 and AMAX are told apart); T280 kills the mode first from hold mode at rest (a burst at the old VMAX);
T281 pins the write order for every starting mode (same velocity mode, other velocity mode, position mode, hold mode
both ways), so the latency-only orders are killed too; T271 kills an ignored read failure from hold mode, T272 a missing
read from hold mode and VMAX before VSTART on a start from rest, T278 and T282 the VSTART bounds of `SetMaxVelocity()`
(the chip's and the profile's) and a `MoveTo()` with VMAX 0; T284 a failed `SetMaxVelocity()` that would change the kept
VMAX; T285 a `MoveBy()` that reads before it refuses. T276 (a TZEROWAIT pause after a VMAX 0 stop and after an arrival
is Busy) kills a missing t_zerowait_active check. The model is checked by T269 (the run-back of a HoldTarget stop), T270
(two-phase deceleration), T274 (velocity mode passes through 0 at AMAX), T275 (every legal position move reaches its
target) and T283 (a lower VMAX is followed with AMAX; no VSTOP in velocity mode). The run is a throwaway script, not
part of CI.

## Quality gates

The `Host quality` job measures the library code, `inc/` and `src/` only (tests and examples are left out), and
fails when a value is over the limits of this repository's assurance level, `high` (set in
[`.kokeb/handbook.yml`](../.kokeb/handbook.yml), defined by the handbook's ADR-0020):

| Gate | Tool and how it is measured | Limit at `high` |
| --- | --- | --- |
| Complexity | lizard 1.24.1, modified cyclomatic complexity (`-m`: a `switch` counts once) per function | CCN at most 15 |
| Function length | lizard, lines of code per function | at most 60 |
| Parameters | lizard, per function | at most 6 |
| Line coverage | GCC 14 `--coverage -O0`, every CTest, gcovr 8.7 `--merge-lines` (a template's lines count once across its instances), `--exclude-unreachable-branches --exclude-throw-branches` | at least 85 % |
| Branch coverage | same run | at least 75 % |
| MC/DC | GCC `-fcondition-coverage`, same run | measured, not gated at `high` (100 % at `safety`) |
| Sanitizers | Clang, `-fsanitize=address,undefined -fno-sanitize-recover=all`, every CTest | no report |
| Fuzzing | libFuzzer with ASan and UBSan, 60 to 120 s per target | no broken promise |

The reports are in the run's `host-quality-reports` artifact and its step summary. To run the gates locally:

```bash
# Coverage (GCC 14 or later), then the complexity limits
cmake -S . -B build-cov -DCMAKE_CXX_COMPILER=g++-14 -DTMC5160_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="--coverage -O0 -fcondition-coverage"
cmake --build build-cov && ctest --test-dir build-cov
gcovr --root . --filter inc/ --filter src/ --merge-lines --exclude-unreachable-branches --txt build-cov
lizard -m -C 15 -L 60 -a 6 inc src

# Sanitizers (Clang)
cmake -S . -B build-san -DCMAKE_CXX_COMPILER=clang++ -DTMC5160_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -O1 -g"
cmake --build build-san && ctest --test-dir build-san --output-on-failure
```

The fuzz gate runs locally as shown in [Fuzz tests](#fuzz-tests).

clang-tidy 20 ([`_config/.clang-tidy`](../_config/.clang-tidy): bugprone, CERT, clang-analyzer,
cppcoreguidelines, modernize, performance, portability and readability, each disabled check with its reason) and
cppcheck (strict) run in the `C++ lint` and `Repository checks` jobs and fail on any finding.

The `MISRA` job ([`ci-misra.yml`](../.github/workflows/ci-misra.yml)) runs the shared `ru-misra` workflow of
ci-tools at level `high`: it traces a host build and the Cortex-M build (`tmc5160_optimised_build`) of every
translation unit, checks them against MISRA C++:2023 and the handbook's C++20 profile (the CodeQL coding-standards
and profile packs, plus the validated clang-tidy and cppcheck checks), and reports each finding against the
handbook's compliance matrix and deviation records. A finding is fixed in the code; a deliberate exception names
its deviation on the line before it (`// codeql::misra_deviation_next_line(MD-CPP-009)` above the `.ipp` include
of `inc/tmc5160.hpp`). Until the repository is public the job runs with `open_source_prerelease: true`, the
declaration that the code is released under the licence in `LICENSE` and is to be published.

## Documentation snippets

Every C++ block on this site is included from [`tests/docs/`](../tests/docs/my_port.hpp), which the host build compiles
(target `tmc5160_doc_snippets`) with the driver instantiated over the documented port. A change to the API that breaks a
page breaks the build first. The README's example is a generated copy of the same file; `kokeb-docs check` fails when it
drifts.

## CI workflows

Everything runs from [`ci.yml`](../.github/workflows/ci.yml), which calls one workflow file per check and ends in
the `CI gate` job, the single required check (`CI gate / gate`).

| Job | What it checks |
| --- | --- |
| Host tests | GCC, then Clang (one job), build and run every test above, then build and run `examples/host` on its own |
| Host quality | The gates above: complexity, coverage, sanitizers, fuzzing |
| Cross-compile | `arm-none-eabi-g++` for Cortex-M0+, M4F and M7F with the strict flags, warnings as errors (nothing linked) |
| C++ lint | clang-format and clang-tidy over the sources |
| MISRA | MISRA C++:2023 and the C++20 profile on traced host and Cortex-M builds, at level `high` (see [Quality gates](#quality-gates)) |
| Docs | Strict site build (`_config/mkdocs.yml`), figure and generated-region drift, Doxygen (`_config/Doxyfile`), offline link check, Lighthouse |
| Repository checks | One job: DCO on pull requests, the org standard, Markdown and YAML lint, the [Kokeb Labs handbook](https://github.com/kokeb-labs/handbook) rules for `.kokeb/handbook.yml` (by rule ID), template drift (warns only) and cppcheck over the driver sources; a second job runs the handbook's source checks (include guards, bus requirements, raw access) |
| CI gate | Passes only if every job above passed: the one required check |

The **ESP32 examples** job builds every app in `examples/esp32/app_config.yml`, for each listed ESP-IDF version and
build type, with the repository root as the driver's component, and lints the example sources. The gate needs it
too.

The workflows call the shared, versioned workflows of
[kokeb-labs/ci-tools](https://github.com/kokeb-labs/ci-tools) at `@v1`.

## On hardware

Nothing in this repository has run on a chip. An MCU example ([Examples](examples.md)) is the bench test: it moves the
motor one revolution each way and logs the position. The bench steps that confirm each decision of
[Ambiguities](ambiguities.md) are open work. Record the board and the result in the pull request's bench-status section.
