# Registers

The register map as the driver knows it: 63 entries, their access and the rule for each group.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Analog Devices (Trinamic) TMC5160A-DS 1.18:

    - Register map and reset values: 6 Register Mapping, p. 31
    - General configuration registers: 6.1 General Configuration Registers, p. 32
    - Motor driver registers: 6.5 Motor Driver Registers, p. 47

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

Access letters: R read, W write-only, RW both, R+WC read and clear by writing 1 (page 31). The coverage map lists every
bit field: [Coverage map](coverage.md).

| Group | Addresses | Registers | Driver |
| --- | --- | --- | --- |
| General | 0x00 to 0x0C | GCONF, GSTAT, IFCNT, NODECONF, IOIN and OUTPUT, X_COMPARE, OTP_PROG, OTP_READ, FACTORY_CONF, SHORT_CONF, DRV_CONF, GLOBALSCALER, OFFSET_READ | `SetFlags()`, `SetDiag()`, `SetBridge()`, `SetProtection()`, `SetCurrent()` |
| Velocity dependent | 0x10 to 0x15 | IHOLD_IRUN, TPOWERDOWN, TSTEP, TPWMTHRS, TCOOLTHRS, THIGH | `SetCurrent()`, `SetStandstill()`, `SetThresholds()` |
| Ramp generator | 0x20 to 0x36 | RAMPMODE, XACTUAL, VACTUAL, VSTART, A1, V1, AMAX, VMAX, DMAX, D1, VSTOP, TZEROWAIT, XTARGET, VDCMIN, SW_MODE, RAMP_STAT, XLATCH | `SetRamp()`, `MoveTo()`, `SetSwitches()` |
| Encoder | 0x38 to 0x3D | ENCMODE, X_ENC, ENC_CONST, ENC_STATUS, ENC_LATCH, ENC_DEVIATION | `SetEncoder()` |
| Microstep table | 0x60 to 0x6B | MSLUT0 to 7, MSLUTSEL, MSLUTSTART, MSCNT, MSCURACT | `SetMicrostepTable()` |
| Chopper and driver | 0x6C to 0x73 | CHOPCONF, COOLCONF, DCCTRL, DRV_STATUS, PWMCONF, PWM_SCALE, PWM_AUTO, LOST_STEPS | `SetChopper()` and the rest |

## Reset values

"All registers become reset to 0 upon power up, unless otherwise noted" (page 31). The noted ones: CHOPCONF
`0x10410150`, PWMCONF `0xC40C001E`, the default microstep table, MSLUTSEL `0xFFFF8056`, MSLUTSTART `0x00F70000`,
TPOWERDOWN 10, VSTOP 1, ENC_CONST `0x00010000`, and the OTP-defined fields (A8). `Config{}` holds them; the shadow
starts from them and is rewritten by every `EnsureInitialized()`.

## Writes and reads

- A write sends the whole 32-bit word from the shadow: there is no read-modify-write over the bus.
- Reserved bits are written as 0; read-only, forbidden and absent addresses are refused by an address table.
- A write-only register cannot be read: `Refused` (A4). The shadow answers instead.
- `ReadRegister(address)` reads any readable register; on SPI it costs two transfers.

## Never written

OTP_PROG, FACTORY_CONF, GCONF.test_mode, and OUTPUT and NODECONF over SPI.
