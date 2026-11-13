# Changelog

All notable changes to this repository are listed here. Versions follow [Semantic Versioning](https://semver.org/).
Entries under **Breaking** need source changes in code that used the affected API; any of them calls for a major
version bump (a minor bump before 1.0).

## [Unreleased]

### Added

- TMC5160 and TMC5160A driver over SPI (including daisy chains), UART (single wire, several nodes) and STEP/DIR:
  lifecycle, current limited by the declared motor and sense resistor, ramp generator, StealthChop, SpreadCycle,
  StallGuard2, CoolStep, encoder, direct mode, diagnostics, guarded dangerous writes and a write-through shadow of the
  write-only registers.
- A chip model, host tests, a fuzz target, eleven host examples and two ESP32 examples.
