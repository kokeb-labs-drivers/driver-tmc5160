# Configuration

Every field of `tmc5160::Config`, how a configuration is built and checked, and the optional Kconfig layer.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Analog Devices (Trinamic) TMC5160A-DS 1.18:

    - Register map and reset values: 6 Register Mapping, p. 31
    - General configuration registers: 6.1 General Configuration Registers, p. 32
    - Quick configuration guide: 22 Quick Configuration Guide, p. 111
    - Clock oscillator and input: 26 Clock Oscillator and Input, p. 119

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Start from the motor

`Config{}` holds the chip's reset values, but it is **not a usable configuration**: it declares no motor and no sense
resistor, and the driver derives every current register from those two. `Validate(Config{})` returns `InvalidArgument`.
Build a configuration with `MakeConfig(motor, board)`, which sets the run current to 85 percent of the motor's ceiling
and the hold current to 30 percent of that, then change fields:

```cpp
--8<-- "tests/docs/configuration.cpp:custom"
```

## Fields

| Field | Default | Meaning | Registers |
| --- | --- | --- | --- |
| `mode` | `MotionController` | `MotionController` or `StepDir`; must match the SD_MODE strap | GCONF, IOIN |
| `clock` | internal 12 MHz | `external` and `hz` (4 to 18 MHz) | all time and velocity values |
| `motor` | none | `rated_rms_ma`, `max_rms_ma`, `coil_resistance_mohm`, `fullsteps_per_rev` | the current ceiling |
| `board` | none | `rsense_mohm`, `supply_mv` (10 000 to 55 000), `supply_max_mv` | GLOBALSCALER, guards |
| `max_velocity_reg` | 0 | Optional ceiling for VMAX | VMAX |
| `include_tolerance` | `true` | Reduce the ceiling by the 5 percent full-scale tolerance | current plan |
| `current` | from `MakeConfig()` | `run_ma`, `hold_ma` | GLOBALSCALER, IHOLD_IRUN |
| `standstill` | datasheet | `hold_delay`, `power_down_delay`, `fast_standstill` | IHOLD_IRUN, TPOWERDOWN, GCONF |
| `chopper` | SpreadCycle, TOFF 3 | mode, TOFF, TBL, hysteresis, `mres`, high-velocity options | CHOPCONF |
| `stealth` | off | enable, autoscale, autograd, frequency, regulation, limits, offsets, freewheel | PWMCONF, GCONF |
| `thresholds` | 0 | `pwm`, `cool`, `high`, `small_hysteresis` | TPWMTHRS, TCOOLTHRS, THIGH |
| `cool`, `stall`, `dcstep` | off | CoolStep, StallGuard2, DcStep | COOLCONF, DCCTRL, VDCMIN |
| `bridge`, `protection` | not declared | Gate-driver and short-protection values, written only when `declared` | DRV_CONF, SHORT_CONF |
| `flags`, `diag` | off | General flags; DIAG outputs | GCONF |
| `ramp` | datasheet quick start | The six-point ramp | VSTART to TZEROWAIT |
| `switches` | off | Reference switches and stop behaviour | SW_MODE |
| `encoder` | not declared | ENCMODE, ENC_CONST, deviation | ENCMODE and following |
| `custom_table`, `table` | default table | A custom microstep table | MSLUT, MSLUTSEL, MSLUTSTART |
| `uart` | node 0, 115200 baud | node, `send_delay`, `multi_node`, `baud` | NODECONF |
| `spi` | chain of 1 | `chain_length`, `chain_index` | transfer length |
| `verify` | `None` | The check `EnsureInitialized()` applies to its writes | |
| `ack` | none | Acknowledgements that lift guards ([Guards](guards.md)) | |
| `begin_enabled` | `false` | Leave the driver enabled after `EnsureInitialized()`; a motor that is still turning (or in its TZEROWAIT pause) makes the start-up fail with `Busy`, bridge off | CHOPCONF.TOFF |

TOFF defaults to 3 where the chip's reset value is 0 (off): the chip's `CHOPCONF` reset word is `0x10410150`, but the
driver writes TOFF only in `Enable()`, so a configured chopper never runs before you ask for it.

## Validation

`Validate(config)` checks a configuration without touching the chip, and `EnsureInitialized()` calls it before any I/O.
It returns `InvalidArgument` when a field is out of range, the motor or the sense resistor is missing, a current is
above the motor's ceiling, or a combination breaks a guard. `CheckedConfig` runs it at build time:
`constexpr CheckedConfig kBoard{MakeConfig(...)}` does not compile when the configuration is invalid.

## Kconfig

Under ESP-IDF, `idf.py menuconfig` shows **TMC5160 stepper driver** when the driver is a component of the project
([Installation](installation.md)); the menu is [`_config/Kconfig`](../_config/Kconfig). The options reach the code only
through [`inc/tmc5160_kconfig.hpp`](../inc/tmc5160_kconfig.hpp), never through a driver signature:

```cpp
--8<-- "tests/docs/configuration.cpp:kconfig"
```

| Option | Default | `Config` field |
| --- | --- | --- |
| `CONFIG_TMC5160_MOTOR_RATED_MA` | 1000 | `motor.rated_rms_ma` |
| `CONFIG_TMC5160_MOTOR_MAX_MA` | 0 | `motor.max_rms_ma` |
| `CONFIG_TMC5160_RSENSE_MOHM` | 100 | `board.rsense_mohm` |
| `CONFIG_TMC5160_SUPPLY_MV` | 24000 | `board.supply_mv` |
| `CONFIG_TMC5160_RUN_MA`, `CONFIG_TMC5160_HOLD_MA` | 0 (from `MakeConfig()`) | `current` |
| `CONFIG_TMC5160_MRES` | 0 (256 microsteps) | `chopper.mres` |
| `CONFIG_TMC5160_STEALTHCHOP` | n | `stealth.enable` |
| `CONFIG_TMC5160_UART_BAUD`, `CONFIG_TMC5160_UART_NODE` | 115200, 0 | `uart` |
| `CONFIG_TMC5160_SPI_CHAIN_LENGTH`, `CONFIG_TMC5160_SPI_CHAIN_INDEX` | 1, 0 | `spi` |
| `CONFIG_TMC5160_VERIFY_STARTUP` | n | `verify` |

Without Kconfig (plain CMake, other platforms) `KconfigDefaults()` returns `Config{}`, which `Validate()` rejects: declare
the motor yourself. Both cases are host tests.

Next: [Porting](porting.md).
