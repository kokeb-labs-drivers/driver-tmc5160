# Datasheet map

The driver follows the TMC5160 datasheet. This page says which revision, where to get it, and where each part of
the driver is described in it, so you can check any behaviour against the source. Section names and page numbers
are the datasheet's own (the printed page numbers).


<!-- generated: datasheet-map -->
- **Datasheet: TMC5160 / TMC5160A Datasheet**, Analog Devices (Trinamic) TMC5160A-DS, revision 1.18 (2023-03-01). Get it
  from the [vendor
  page](https://www.analog.com/media/en/technical-documentation/data-sheets/TMC5160A_datasheet_rev1.18.pdf).
  SHA-256 of the file used: `3c3edc4fbd694f3afd95ff8f5922a777fdc56d91660c3c0fbdccc016f1bd0be1`.

| Topic | Datasheet section | Page | In the driver | Explained on |
| --- | --- | --- | --- | --- |
| Features, control interfaces and key concepts | 1 Principles of Operation | p. 5 | `TMC5160`, `Mode` | [TMC5160 Driver](index.md), [Architecture](architecture.md) |
| Package, pins and signal descriptions | 2 Pin Assignments | p. 11 | `CtrlPin`, `InputPin`, `Config::mode` | [Hardware](hardware.md), [Quickstart](quickstart.md) |
| Sample circuits and gate drive | 3 Sample Circuits | p. 15 | `Bridge`, `SetBridge()` | [Hardware](hardware.md), [Guards and acknowledgements](guards.md) |
| SPI datagram, status byte and timing | 4 SPI Interface | p. 23 | `SpiInterface`, `Transfer()`, `Verify::Echo`, `kSpiMode` | [SPI protocol](spi-protocol.md), [Porting](porting.md) |
| SPI daisy chain | 4.1 SPI Datagram Structure | p. 23 | `SpiOptions::chain_length`, `chain_index` | [SPI protocol](spi-protocol.md) |
| UART datagram, node address and SENDDELAY | 5.1 Datagram Structure | p. 26 | `UartInterface`, `Exchange()`, `UartOptions` | [UART protocol](uart-protocol.md), [Porting](porting.md) |
| UART CRC-8 | 5.2 CRC Calculation | p. 28 | `detail::Crc8()`, `Error::Crc` | [UART protocol](uart-protocol.md), [Testing and CI](testing.md) |
| Addressing multiple UART nodes | 5.4 Addressing Multiple Nodes | p. 29 | `ProgramNode()` | [UART protocol](uart-protocol.md), [Examples](examples.md) |
| Register map and reset values | 6 Register Mapping | p. 31 | `inc/tmc5160_registers.hpp`, `ReadRegister()` | [Registers](registers.md), [Configuration](configuration.md) |
| General configuration registers | 6.1 General Configuration Registers | p. 32 | `SetFlags()`, `SetDiag()`, `Config::flags` | [Registers](registers.md), [Configuration](configuration.md) |
| Global status and interface counter | 6.1 General Configuration Registers | p. 33 | `ReadGlobalStatus()`, `ClearGlobalStatus()`, `Error::UnexpectedReset` | [DIAG and status](diag-and-status.md), [Troubleshooting](troubleshooting.md) |
| Standstill current and power-down | 6.1 General Configuration Registers | p. 36 | `SetStandstill()`, `Standstill` | [Current and limits](current-and-limits.md) |
| Velocity dependent control | 6.2 Velocity Dependent Driver Feature Control Register Set | p. 38 | `SetThresholds()`, `Thresholds` | [Motion](motion.md), [StealthChop and SpreadCycle](chopper.md) |
| Ramp generator registers | 6.3 Ramp Generator Registers | p. 40 | `SetRamp()`, `RampProfile`, `MoveTo()`, `RunVelocity()` | [Motion](motion.md) |
| Reference switches and SW_MODE | 6.3 Ramp Generator Registers | p. 43 | `SetSwitches()`, `Switches`, `SetStopOnStall()` | [Motion](motion.md), [StallGuard2 and CoolStep](stallguard-and-coolstep.md) |
| Ramp status and events | 6.3 Ramp Generator Registers | p. 44 | `ReadRampStatus()`, `ClearRampEvents()`, `ReleaseStall()` | [DIAG and status](diag-and-status.md), [Motion](motion.md) |
| Encoder registers | 6.4 Encoder Registers | p. 45 | `SetEncoder()`, `Encoder`, `ReadEncoderPosition()` | [DcStep, encoder and direct mode](special-modes.md) |
| Motor driver registers | 6.5 Motor Driver Registers | p. 47 | `SetChopper()`, `ReadMicrostepCounter()`, `ReadDriverStatus()` | [Registers](registers.md), [StealthChop and SpreadCycle](chopper.md) |
| Chopper configuration | 6.5 Motor Driver Registers | p. 51 | `SetChopper()`, `Chopper` | [StealthChop and SpreadCycle](chopper.md) |
| CoolStep and StallGuard2 registers | 6.5 Motor Driver Registers | p. 53 | `SetCoolStep()`, `SetStallGuard()` | [StallGuard2 and CoolStep](stallguard-and-coolstep.md) |
| Driver status | 6.5 Motor Driver Registers | p. 55 | `ReadDriverStatus()`, `DriverStatus` | [DIAG and status](diag-and-status.md) |
| StealthChop2 and automatic tuning | 7 StealthChop | p. 57 | `SetStealthChop()`, `StealthChop`, `ReadStealthChopState()` | [StealthChop and SpreadCycle](chopper.md) |
| StealthChop and SpreadCycle combined | 7.5 Combine StealthChop and SpreadCycle | p. 64 | `SetThresholds()`, `Thresholds::pwm` | [StealthChop and SpreadCycle](chopper.md) |
| Freewheeling and passive braking | 7.7 Freewheeling and Passive Braking | p. 66 | `Freewheel`, `StealthChop::freewheel` | [StealthChop and SpreadCycle](chopper.md) |
| SpreadCycle and classic chopper | 8 SpreadCycle and Classic Chopper | p. 68 | `Chopper`, `ChopperMode` | [StealthChop and SpreadCycle](chopper.md) |
| Selecting sense resistors and the current scale | 9 Selecting Sense Resistors | p. 74 | `MakeConfig()`, `units::PlanCurrent()`, `SetCurrent()` | [Current and limits](current-and-limits.md), [Guards and acknowledgements](guards.md) |
| Velocity based mode control | 10 Velocity Based Mode Control | p. 76 | `Thresholds` | [StealthChop and SpreadCycle](chopper.md) |
| Temperature sensors and short protection | 11 Diagnostics and Protection | p. 78 | `SetProtection()`, `Protection`, `Ack::ShortProtectionOff`, `Ack::LowOtLevel` | [Guards and acknowledgements](guards.md), [DIAG and status](diag-and-status.md) |
| Open load diagnostics | 11.3 Open Load Diagnostics | p. 80 | `DriverStatus::open_load_a` | [DIAG and status](diag-and-status.md) |
| Real world unit conversion | 12.1 Real World Unit Conversion | p. 81 | `units::VelocityRegFromHz()`, `units::AccelRegFromHzPerS()` | [Motion](motion.md) |
| Motion profiles and early ramp termination | 12.2 Motion Profiles | p. 82 | `MoveTo()`, `Stop()`, `Hold()`, `StopKind` | [Motion](motion.md) |
| Velocity thresholds | 12.3 Velocity Thresholds | p. 84 | `Thresholds` | [Motion](motion.md) |
| Reference switches | 12.4 Reference Switches | p. 85 | `Switches`, `ReadInputs()` | [Motion](motion.md) |
| StallGuard2 | 13 StallGuard2 Load Measurement | p. 87 | `SetStallGuard()`, `ReadStallGuard()` | [StallGuard2 and CoolStep](stallguard-and-coolstep.md) |
| Homing with StallGuard | 13.4 Homing with StallGuard | p. 90 | `SetStopOnStall()`, `ReleaseStall()`, examples/host/homing_stall.cpp | [StallGuard2 and CoolStep](stallguard-and-coolstep.md), [Examples](examples.md) |
| CoolStep | 14 CoolStep Operation | p. 91 | `SetCoolStep()`, `CoolStep` | [StallGuard2 and CoolStep](stallguard-and-coolstep.md) |
| STEP/DIR interface and MicroPlyer | 15 STEP/DIR Interface | p. 94 | `Mode::StepDir`, `Config::mode` | [Motion](motion.md), [DcStep, encoder and direct mode](special-modes.md) |
| DIAG outputs | 16 DIAG Outputs | p. 97 | `SetDiag()`, `Diag`, `Diag0Active()`, `Diag1Active()` | [DIAG and status](diag-and-status.md) |
| DcStep | 17 DcStep | p. 99 | `SetDcStep()`, `DcStep`, `ReadLostSteps()` | [DcStep, encoder and direct mode](special-modes.md) |
| Sine-wave look-up table | 18 Sine-Wave Look-up Table | p. 105 | `SetMicrostepTable()`, `MicrostepTable` | [Microstep table](microstep-table.md) |
| Emergency stop | 19 Emergency Stop | p. 106 | `Stop()`, `StopKind::Emergency` | [Motion](motion.md) |
| ABN incremental encoder interface | 20 ABN Incremental Encoder Interface | p. 107 | `SetEncoder()`, `units::EncoderScaleFor()` | [DcStep, encoder and direct mode](special-modes.md) |
| DC motor and direct mode | 21 DC Motor or Solenoid | p. 110 | `EnterDirectMode()`, `SetDirectCurrents()`, `Ack::DirectMode` | [DcStep, encoder and direct mode](special-modes.md), [Guards and acknowledgements](guards.md) |
| Quick configuration guide | 22 Quick Configuration Guide | p. 111 | `MakeConfig()`, `Config` | [Configuration](configuration.md), [Quickstart](quickstart.md) |
| Initialization examples | 23.1 Initialization Examples | p. 116 | `EnsureInitialized()` | [Quickstart](quickstart.md), [Architecture](architecture.md) |
| Power-up reset | 25 Power-Up Reset | p. 119 | `EnsureInitialized()`, `Error::UnexpectedReset` | [Contract](contract.md), [Troubleshooting](troubleshooting.md) |
| Clock oscillator and input | 26 Clock Oscillator and Input | p. 119 | `Clock`, `kInternalClockHz` | [Configuration](configuration.md), [Motion](motion.md) |
| Absolute maximum ratings and operational range | 28.1 Operational Range | p. 120 | `Board::supply_mv`, `kVsMinMv`, `kVsMaxMv` | [Hardware](hardware.md), [Guards and acknowledgements](guards.md) |
| DC and timing characteristics | 28.2 DC and Timing Characteristics | p. 121 | `kVfsMv`, `kCurrentTolerancePercent` | [Current and limits](current-and-limits.md), [Hardware](hardware.md) |
| Thermal characteristics | 28.3 Thermal Characteristics | p. 124 | `Ack::LowOtLevel` | [Hardware](hardware.md) |
| Package mechanical data and codes | 30 Package Mechanical Data | p. 128 | - | [Hardware](hardware.md) |
<!-- /generated -->

## When the datasheet and the driver differ

The driver is written against the revision above. If you have a newer revision, compare its revision history with
the topics above; a change to a pin, a register, a timing or a value the driver uses is a bug report for this
repository. Where the driver deliberately goes beyond the datasheet, the [Contract](contract.md) says so.

The repository never contains the PDF itself: vendors own their documents. Download it from the vendor, and compare
its SHA-256 with the one above to confirm you have the same file.
