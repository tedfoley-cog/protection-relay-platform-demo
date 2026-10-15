# Customer requirement: Busbar differential protection (ANSI 87B)

| | |
|---|---|
| From | Protection & Control Engineering, Distribution Substations |
| To | Relay platform firmware team |
| Reference | PCE-REQ-2026-087, revision B |
| Status | issued for implementation |

## 1. Background

Our 12.47 kV single-bus substations use the four-bay relay platform (one
transformer infeed, three feeders). Bus faults are presently cleared by the
transformer backup overcurrent after 0.8 s. We require a low-impedance
percentage-restrained busbar differential element (87B) in the existing relay
so that bus faults are cleared instantaneously and feeder faults are not.

## 2. Functional requirements

2.1 The relay shall compute a per-phase differential current as the phasor
sum of the bay currents, and a per-phase restraint current as the sum of the
magnitudes of the bay currents, using the currents of bays 1 to 4.

2.2 The element shall operate when the differential current exceeds the
minimum pickup setting `87B_PICKUP` and exceeds the percentage restraint
characteristic.

2.3 The restraint characteristic shall be dual-slope. Slope 1 shall apply
below the breakpoint `87B_BREAKPOINT`; slope 2 shall apply above it. Default
settings: slope 1 = 60 %, slope 2 = 25 %, breakpoint = 2.0 pu. The
minimum pickup default shall be 0.2 pu.

2.4 All current quantities shall be expressed in per unit of the bus
rating, 20 MVA at 12.47 kV. The pickup setting range shall be 0.05 to
2.0 pu of the largest bay CT primary rating.

2.5 The element shall operate within one cycle for an internal fault with a
differential current of at least 2 × pickup, and shall not operate for an
external (through) fault on any feeder up to 20 times the feeder CT rating.

2.6 Operation shall trip all four bay breakers through the 86B lockout and
record an `87B` event. The trip output shall be sealed in.

2.7 A CT-circuit supervision function shall alarm when a standing
differential current of more than 5 % of the restraint current persists
for 3 s under load, and shall block the 87B element.

2.8 The element shall be available for the spare bay position (bay 5) when
that position is equipped in future.

2.9 The element shall be settable per bay as "in zone" or "out of zone" so
that a bay can be transferred to the adjacent bus section.

## 3. Settings

| Setting | Range | Default |
|---|---|---|
| `87B_FUNCTION` | Enabled / Disabled | Enabled |
| `87B_PICKUP` | 0.05 - 2.0 pu | 0.2 pu |
| `87B_SLOPE1` | 10 - 100 % | 60 % |
| `87B_SLOPE2` | 10 - 100 % | 25 % |
| `87B_BREAKPOINT` | 1.0 - 10.0 pu | 2.0 pu |
| `87B_CTS_ALARM_PCT` | 2 - 20 % | 5 % |
| `87B_CTS_DELAY` | 0.5 - 10 s | 3 s |

## 4. Acceptance

Acceptance testing will be performed by secondary injection on the delivered
firmware using our standard bus-fault and through-fault cases. The supplier
shall provide the design specification, the validation report and the
instruction-manual chapter in the platform's standard format.

## 5. Notes

Bay 4 at the pilot substation is a capacitor bank position whose CTs are not
yet wired to the relay; the element shall nevertheless be commissioned at
that site.
