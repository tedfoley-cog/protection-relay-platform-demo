# Instruction manual — 87B Busbar differential protection

> Draft. Thirteen requirement rows are open with the requirement owner
> (`REQUIREMENTS.md` §2); the behaviour below is the prototype behaviour under
> the stated assumptions and may change when they are answered.

## Application
87B is a low-impedance, percentage-restrained differential for the bus
section to which bays 1–4 connect. It compares the phasor sum of the in-zone
bay currents (differential) with the sum of their magnitudes (restraint) on
each phase, and trips the bus through the 86B lockout for an internal fault
while remaining stable for load and through faults.

## Settings
| Setting | Range | Step | Default |
|---|---|---|---|
| 87B Function | Enabled / Disabled | | Enabled |
| 87B Base MVA | 1–1000 MVA | 0.1 MVA | 20 MVA |
| 87B Pickup | 0.05–2.00 pu | 0.01 pu | 0.20 pu |
| 87B Slope 1 | 10–100 % | 1 % | 25 % |
| 87B Slope 2 | 10–100 % (not below Slope 1) | 1 % | 60 % |
| 87B Breakpoint | 1.0–10.0 pu | 0.1 pu | 2.0 pu |
| 87B CT supervision alarm | 2–20 % of restraint | 1 % | 5 % |
| 87B CT supervision delay | 0.5–10.0 s | 0.1 s | 3.0 s |
| 87B Bay 1..4 in zone | Yes / No (at least two Yes) | | Yes |

Per-unit base: `Base MVA / (sqrt(3) x nominal line voltage)` — 926 A primary
with the default 20 MVA and 12.47 kV. All 87B settings and actual values use
this base.

## Characteristic
Operate when `Id > Pickup` and `Id > T(Ir)`, with `T(Ir) = S1 x Ir` up to
the breakpoint and `T(Ir) = S1 x BP + S2 x (Ir - BP)` above it. The setting
tool plots the characteristic and marks the pickup, breakpoint and the
through-fault restraint of the standard test cases.

## Operation
Pickup is indicated by `87B_PKP`; operation asserts `87B_OP`, trip output 1
and the 86B bus lockout output, with an `OPERATE 87B` event carrying the
differential current. The lockout is held until it is reset. The element is
blocked (`87B_BLK`) while any current input of an in-zone bay is flagged
invalid, while a bay in zone has its CT marked not connected, for one cycle
after power-up or reset, and while the CT-supervision alarm (`87B_CTS_ALM`)
stands. A bay set out of zone contributes to neither current and does not
block the element. Disabling the function resets the element but does not
release a standing lockout.

## CT supervision
If, under load (restraint at least 0.1 pu), a differential below the pickup
but above the alarm percentage of the restraint persists for the alarm delay,
`87B_CTS_ALM` is raised, an event is recorded and 87B is blocked until the
condition has been absent for the same delay.

## Specifications
| | |
|---|---|
| Operate time | one measurement cycle plus one protection pass (see validation report) |
| Pickup accuracy | +/- 3 % of setting |
| Timing accuracy (CT supervision) | +/- 1 protection pass (2.1 ms) |
| Drop-out ratio | 1.0 (comparator) |
| Reset | lockout: manual; element: instantaneous |
| Nominal frequency | 60 Hz |
