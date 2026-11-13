# Current and limits

How the motor current is derived from what you declare, and the limits the driver enforces on it.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Analog Devices (Trinamic) TMC5160A-DS 1.18:

    - Standstill current and power-down: 6.1 General Configuration Registers, p. 36
    - Selecting sense resistors and the current scale: 9 Selecting Sense Resistors, p. 74
    - DC and timing characteristics: 28.2 DC and Timing Characteristics, p. 121

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## The formula

The chip's coil RMS current is (datasheet section 9, page 74):

```text
I_rms = (GLOBALSCALER / 256) * ((CS + 1) / 32) * (VFS / Rsense) / sqrt(2)
```

`VFS` is 325 mV typical with a tolerance of 5 percent (section 28.2, page 121); GLOBALSCALER 0 means 256, and values
1 to 31 are not allowed. `CS` is IRUN while moving and IHOLD at standstill; CoolStep lowers it between the two.

## What you declare

| You declare | Field | Why |
| --- | --- | --- |
| The motor's rated RMS current | `Motor::rated_rms_ma` | The ceiling for run and hold current |
| A hard ceiling, if lower than rated | `Motor::max_rms_ma` | 0 means the rated current |
| The coil resistance | `Motor::coil_resistance_mohm` | The StealthChop lower limit; 0 means unknown |
| The sense resistor per coil | `Board::rsense_mohm` | The divisor in the formula |
| The motor supply | `Board::supply_mv`, `supply_max_mv` | Checked against 10 V to 55 V; the short-protection level depends on it |

## The ceiling

`units::PlanCurrent()` turns a request in mA into registers. The ceiling is the smaller of `max_rms_ma` and
`rated_rms_ma`, reduced by 5 percent (`Config::include_tolerance`, default on) so that a chip whose full-scale voltage is
5 percent high still stays below the rated current. A request above the ceiling returns `LimitExceeded` and sends
nothing. A request below one CS step at the smallest allowed GLOBALSCALER (129) also returns `LimitExceeded`: it cannot
be realised; use a larger sense resistor.

GLOBALSCALER is chosen in the range 129 to 256 so that the CS field has the most resolution. IHOLD above IRUN is refused
unless `Ack::HoldAboveRun` is in `Config::ack` ([Guards](guards.md)).

## The order of writes

`SetCurrent()` changes GLOBALSCALER and IHOLD_IRUN with two writes. The driver picks the order by the intermediate
current: when the new scaler is not larger than the old one (by the product with the CS value) it writes the scaler
first, otherwise IHOLD_IRUN first, so a bus failure between the two writes never leaves a current above the ceiling.

## What the plan returns

`CurrentPlan` holds `global_scaler`, `irun`, `ihold`, and the realised `run_ma` and `hold_ma`. The realised values are
at or below the request, never above it. `GetCurrentPlan()` returns the plan in force.

## After a reset

A chip reset sets every register to its reset value, including GLOBALSCALER 0 (256) and IRUN 0. The driver's shadow
keeps the values it wrote; `Enable()` refuses after a reset flag so the reset values are never driven into a motor.
`EnsureInitialized()` again rewrites everything from the configuration.

## The sense resistor table

The datasheet's table on page 74 rounds the full-scale current up by 2 to 5 percent against the formula
([Ambiguities](ambiguities.md), A20); the driver uses the formula. Any sense resistor is accepted if the request fits:
the advice outside the table's range (0.05 to 0.22 ohm) is the application's (A47).
