/**
 * @file api_tasks.cpp
 * @brief The API guide's task examples, compiled by the host build (never run).
 */
#include "my_port.hpp"
#include "tmc5160.hpp"

using Motor = tmc5160::TMC5160<MyPort>;

// --8<-- [start:lifecycle]
// Start, enable, stop. Every call returns a Result: check it.
tmc5160::Error RunAndStop(Motor& motor) {
  if (auto r = motor.EnsureInitialized(); !r) { // writes every register; the driver stays off
    return r.error;
  }
  if (auto r = motor.Enable(); !r) { // refuses after a chip reset or a driver fault
    return r.error;
  }
  if (auto r = motor.RunVelocity(50'000); !r) { // VMAX in the chip's own units, signed
    return r.error;
  }
  if (auto r = motor.Stop(tmc5160::StopKind::Ramp); !r) { // decelerate with the programmed ramp
    return r.error;
  }
  return motor.Disable().error; // DRV_ENN inactive; the motor is not held
}
// --8<-- [end:lifecycle]

// --8<-- [start:current]
// The current is always checked against the declared motor and the sense resistor.
tmc5160::Error SetCurrents(Motor& motor) {
  const auto plan = motor.SetCurrent(800, 240); // run and hold, mA rms
  if (!plan) {
    return plan.error; // LimitExceeded: above the motor or the 5 % ceiling, or below one CS step
  }
  // `plan.value` holds GLOBALSCALER, IRUN, IHOLD and the realised currents.
  return tmc5160::Error::Ok;
}
// --8<-- [end:current]

// --8<-- [start:motion]
// Position mode: the ramp generator moves to XTARGET. Poll PositionReached().
tmc5160::Error GoTo(Motor& motor, int32_t target) {
  if (auto r = motor.MoveTo(target); !r) {
    return r.error;
  }
  for (int i = 0; i < 1000; ++i) { // bounded: the caller decides how long a move may take
    const auto done = motor.PositionReached();
    if (!done) {
      return done.error;
    }
    if (done.value) {
      return tmc5160::Error::Ok;
    }
  }
  return tmc5160::Error::Timeout;
}
// --8<-- [end:motion]

// --8<-- [start:stealth]
// StealthChop below a velocity, SpreadCycle above it.
tmc5160::Error QuietMotor(Motor& motor) {
  tmc5160::StealthChop stealth;
  stealth.enable = true;
  if (auto r = motor.SetStealthChop(stealth); !r) { // waits the automatic tuning time at standstill
    return r.error;
  }
  return motor.SetThresholds(tmc5160::Thresholds{.pwm = 500})
      .error; // TSTEP above 500 is StealthChop
}
// --8<-- [end:stealth]

// --8<-- [start:stall]
// StallGuard2 with a stop on stall. The motor stays stopped until ReleaseStall().
tmc5160::Error SensorlessStop(Motor& motor) {
  if (auto r = motor.SetThresholds(tmc5160::Thresholds{.cool = 200'000}); !r) {
    return r.error;
  }
  if (auto r = motor.SetStallGuard(tmc5160::StallGuard{.threshold = 4, .filter = true}); !r) {
    return r.error;
  }
  if (auto r = motor.SetStopOnStall(true); !r) {
    return r.error;
  }
  const auto ramp = motor.ReadRampStatus();
  if (ramp && ramp.value.event_stop_stall) {
    return motor.ReleaseStall().error; // clear the event: the motor may move again
  }
  return ramp.error;
}
// --8<-- [end:stall]

// --8<-- [start:diag]
// A periodic service call: one batch read, the events decoded and the latched ones cleared.
tmc5160::Error ServiceLoop(Motor& motor) {
  const auto events = motor.Service();
  if (!events) {
    return events.error; // UnexpectedReset: write-only registers are lost, Enable() refuses
  }
  if (events.value.driver.over_temp || events.value.driver.s2g_a || events.value.driver.s2g_b) {
    return tmc5160::Error::DeviceFault;
  }
  return tmc5160::Error::Ok;
}
// --8<-- [end:diag]

// --8<-- [start:guard]
// A guarded write: IHOLD above IRUN is refused unless the call acknowledges it.
tmc5160::Error HoldAboveRun(Motor& motor) {
  tmc5160::Config c = motor.GetConfig();
  c.current.hold_ma = c.current.run_ma + 100;
  const auto refused = motor.SetCurrent(c.current.run_ma, c.current.hold_ma);
  return refused.error; // Refused: no tmc5160::Ack::HoldAboveRun in the configuration
}
// --8<-- [end:guard]

// --8<-- [start:errors]
// What each error asks of the application.
const char* Advice(tmc5160::Error e) {
  switch (e) {
  case tmc5160::Error::Bus:
  case tmc5160::Error::Timeout:
  case tmc5160::Error::Crc:
    return "retry the read; a write is not retried: look at the wiring"; // reads are retried twice
  case tmc5160::Error::UnexpectedReset:
    return "the chip lost its registers: EnsureInitialized() again";
  case tmc5160::Error::DeviceFault:
    return "short or overtemperature: remove the cause, then ClearFaults()";
  case tmc5160::Error::Refused:
  case tmc5160::Error::LimitExceeded:
    return "the guard stopped an unsafe request: change the request";
  default:
    return "see the error table";
  }
}
// --8<-- [end:errors]

// --8<-- [start:deinit]
// Release the driver: the chip is quiesced and every later call returns NotInitialized.
tmc5160::Error Shutdown(Motor& motor) {
  return motor.Deinitialize().error;
}
// --8<-- [end:deinit]
