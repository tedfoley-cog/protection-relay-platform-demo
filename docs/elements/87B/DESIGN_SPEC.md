# Design specification: 87B busbar differential (low-impedance, percentage restrained)

| | |
|---|---|
| Requirement set | `docs/elements/87B/REQUIREMENTS.md` |
| Safety classification | protection function (see `harness.yaml`) |
| Governing standards | IEC 60255-1, IEC 60255-187-1 (differential), IEEE C37.234 (bus protection guide) |
| Status | draft — 13 requirement rows BLOCKED (see REQUIREMENTS.md §2); prototype built under assumptions A1–A13 |

## 1. Function

87B protects the single 12.47 kV bus section to which bays 1–4 connect. Each
protection pass it forms, per phase, the phasor sum of the polarity-corrected
in-zone bay currents (differential, Id) and the sum of their magnitudes
(restraint, Ir), both in per unit of one declared base. It operates when Id
exceeds the pickup and a dual-slope percentage-restraint characteristic,
drives the 86B bus lockout (TRIPB) and the bay-1 trip (TRIP1), records an
`OPERATE 87B` event, and publishes `87B` operands. A CT-circuit supervision
(CTS) timer alarms on a standing small differential under load and blocks the
element while the alarm stands. Any invalid in-zone input blocks the element;
nothing is ever substituted for a missing current.

## 2. Inputs

| Input | Source | Unit | Validity handling |
|---|---|---|---|
| Bay k phase currents, k = 1..4 | `phasor_full_cycle(hist, IED_CH_BAY_I(k, ph), tick)` in `protection_pass()`, scaled to primary A by `pu_a_per_count` x CT ratio, same `tick` for all 12 channels (REQ-87B-003) | primary A, phasor | element blocked if `invalid_mask` covers any channel of any **in-zone** bay (REQ-87B-027); out-of-zone bays are not read (REQ-87B-021) |
| Bay polarity | `bay_cfg[k].polarity` | +1 = positive toward bus | current into bus = polarity x measured phasor |
| Bay CT connected | `bay_cfg[k].ct_connected` | bool | not connected => channels flagged invalid by the acquisition layer => element blocked (REQ-87B-023) |
| Settings | `ied_settings_t.b87` (keys §3) | | validated by R-87B-001..009 before activation (REQ-87B-030) |
| VT nominal | `VT_V_LN_NOMINAL` | V | defines Ib with `87B_BASE_MVA` (A1) |
| DFT window state | `protection_pass()` arming (`hist` full) | | element not evaluated until the window is full (REQ-87B-028) |

All twelve bay currents pass through the same mimic filter (one per channel,
`mimic_bus[3][3]` added for bays 2–4, same X/R setting) before the sample
history, and every bay phasor is mimic-compensated the same way. Identical
linear filters make the differential of a through current exactly zero
through the DC-offset transient (REQ-87B-034). An earlier draft used raw DFT
for bays 2–4 and produced a false differential during the first cycle of
external faults in the sweep; that design is withdrawn. Q1 is therefore
closed for 87B: the mimic is applied to bays 2–4 in `ingest()` (protection
task, not the ISR), nine `mimic_step` per sample.

## 3. Settings

| Key | Range | Default | Rule | Requirement |
|---|---|---|---|---|
| `87B_FUNCTION` | ENABLED / DISABLED | ENABLED (A11) | — | REQ-87B-024 |
| `87B_BASE_MVA` | 1–1000 MVA | 20 (A1) | R-87B-009 | REQ-87B-007 |
| `87B_PICKUP` | 0.05–2.00 pu | 0.20 | R-87B-001 | REQ-87B-008 |
| `87B_SLOPE1` | 10–100 % | 25 (A3) | R-87B-002 | REQ-87B-009 |
| `87B_SLOPE2` | 10–100 % | 60 (A3) | R-87B-003 | REQ-87B-010 |
| `87B_BREAKPOINT` | 1.0–10.0 pu | 2.0 | R-87B-004 | REQ-87B-011 |
| (S1, S2) | S2 >= S1 | | R-87B-005 | REQ-87B-012 |
| `87B_CTS_ALARM_PCT` | 2–20 % | 5 | R-87B-006 | REQ-87B-025 |
| `87B_CTS_DELAY` | 0.5–10.0 s | 3.0 | R-87B-007 | REQ-87B-026 |
| `87B_BAY1_IN_ZONE` .. `87B_BAY4_IN_ZONE` | YES / NO | YES | R-87B-008 (at least two YES) | REQ-87B-021, -022 |

Derived at `ied_settings_finalize`: `base_a = pu_base_a_from_mva(87B_BASE_MVA,
sqrt(3) x VT_V_LN_NOMINAL / 1000)` (926.0 A with defaults) and
`cts_delay_passes = round(87B_CTS_DELAY x 480)`. The element reads only the
`bus_diff_87b_settings_t` passed to it.

## 4. Algorithm

Per-unit base (A1): Ib = `87B_BASE_MVA` / (sqrt(3) x VLL), VLL = sqrt(3) x
`VT_V_LN_NOMINAL`; defaults give Ib = 926.0 A primary. Every pu quantity in
this element is on Ib.

Per phase p, in-zone bay set Z (REQ-87B-001, -002):

    I_k,p   = polarity_k x I_meas,k,p                 (primary A, into bus positive)
    Id_p    = | sum_{k in Z} I_k,p | / Ib
    Ir_p    = sum_{k in Z} | I_k,p | / Ib

Characteristic (A2, REQ-87B-005):

    T(Ir) = S1 x Ir                        for Ir <= BP
    T(Ir) = S1 x BP + S2 x (Ir - BP)       for Ir >  BP

Operate per phase (REQ-87B-004, -006):

    pickup_p  = Id_p > PU
    operate_p = pickup_p AND Id_p > T(Ir_p)
    operate   = OR_p operate_p

CT supervision (A7, REQ-87B-018, -019): condition `cts_cond_p = Ir_p >= 0.1 pu
AND Id_p < PU AND Id_p > (CTS_PCT/100) x Ir_p`. A pass counter advances while
any phase satisfies `cts_cond`, resets to zero otherwise, and `cts_alarm` is
set when the counter reaches `cts_delay_passes`. The alarm drops out when the
condition has been absent for the same number of passes (symmetric
timer). While `cts_alarm` is set, `blocked` is true and `operate` is forced
false.

State machine, evaluated once per protection pass:

| State | Entry | Exit |
|---|---|---|
| DISABLED | `87B_FUNCTION = DISABLED`; all flags false, counters zero | setting changes to ENABLED (via finalize, which resets state) |
| BLOCKED | any in-zone bay channel invalid, or DFT window not full, or `cts_alarm` | condition clears; counters are held (not advanced) while BLOCKED |
| IDLE | enabled, not blocked, no phase above pickup | pickup on any phase |
| PICKUP | Id_p > PU on some phase but below T | Id above T => OPERATE; Id below PU => IDLE |
| OPERATE | `operate` true; trip matrix asserts TRIP1 and TRIPB (86B latched); SOE `OPERATE 87B` on the rising edge | `operate` falls when Id drops below T; TRIPB stays latched until `ied_reset_lockout()` (A6, REQ-87B-017) |

There is no intentional time delay between pickup and operate; the operate
time is the measurement-chain delay (one DFT cycle + one pass).

## 5. Operands and outputs

| Operand | Meaning | Set when | Reset when |
|---|---|---|---|
| `87B_PKP` | any phase Id > PU | any `pickup_p` | all phases below PU |
| `87B_OP` | element operated | any `operate_p` and not blocked | `operate` false (TRIPB remains latched) |
| `87B_BLK` | element blocked | invalid in-zone input, window not full, or CTS alarm | none of those |
| `87B_CTS_ALM` | CT supervision alarm | CTS counter reaches delay | condition absent for the delay |
| `87B_ENA` | function enabled | `87B_FUNCTION = ENABLED` | DISABLED |
| actual values | `Id_A/B/C`, `Ir_A/B/C` in pu of Ib | every evaluated pass | zeroed when disabled or blocked |

Outputs: trip matrix `ELEM_87B` routed to `trip1_mask` and `tripb_mask`
(REQ-87B-015); SOE `SOE_ELEMENT_OPERATE` detail `ELEM_87B`, value = Id of the
first operating phase in pu (REQ-87B-016); SOE `SOE_INPUT_INVALID`-style
blocked transition logged once; CTS alarm logged as an SOE event with detail
`ELEM_87B`; DNP3 binary and PMU digital word carry `87B_OP` (REQ-87B-033).

## 6. Behaviour in degraded conditions

| Condition | Behaviour | Requirement |
|---|---|---|
| Any channel of an in-zone bay invalid (lost CT, saturated ADC, frame gap) | BLOCKED: no operate, CTS counter held, `87B_BLK` true, logged once | REQ-87B-027 |
| Bay CT not connected (`BAYn_CT_CONNECTED = NO`) while bay in zone | channels invalid => BLOCKED permanently; nothing substituted | REQ-87B-023 (A10) |
| Bay out of zone | excluded from Id and Ir; its validity ignored | REQ-87B-021 (A9) |
| Fewer than two bays in zone | settings write rejected (R-87B-008) | REQ-87B-022 |
| Settings change accepted mid-pickup | finalize resets element state (counters, flags); TRIPB latch untouched | REQ-87B-029 (A13) |
| Settings write rejected | active group unchanged, element unaffected | REQ-87B-029 |
| Power-up / reset | not evaluated until the DFT window is full (one cycle) | REQ-87B-028 (A12) |
| `87B_FUNCTION = DISABLED` | DISABLED state; no operate, no CTS; standing 86B lockout not cleared | REQ-87B-024 (A11) |
| CTS alarm standing | BLOCKED | REQ-87B-019 |
| Bay-1 bus blocking (`invalid_mask & BAY1_INPUT_MASK`) early return | 87B evaluated before that return only when bay 1 is out of zone; otherwise bay 1 invalid blocks 87B as any in-zone bay | REQ-87B-027 |

## 7. Timing and budget

| Quantity | Expected | Basis |
|---|---|---|
| Operate time, internal fault Id >= 2 x PU | ~1 cycle + 1 pass from inception (18.8 ms typical); customer target one cycle is BLOCKED (REQ-87B-013) | full-cycle DFT group delay, PLATFORM_FIGURES.md |
| Operate time at 5 x, 10 x PU | same; the DFT window fills before the magnitude is final, no time delay in the element | — |
| Drop-out | Id below T; ratio 1.0 on the comparator, no intentional hysteresis | §4 |
| Seal-in | TRIPB latched by the existing 86B path | REQ-87B-017 |
| CTS alarm delay | `87B_CTS_DELAY` +/- 1 pass | §4 |
| Cycles per pass | 9 additional `phasor_full_cycle` (bays 2–4, 32-tap complex MAC each), 9 `mimic_compensate`, 9 x 4 `mimic_step` per pass in `ingest()`, plus ~200 cycles of element arithmetic; estimate +2 500 to +4 000 cycles on 6 447 before (budget 12 000) | measured in `realtime.json` after (REQ-87B-031) |

## 8. Traceability

| Requirement | Section | Test |
|---|---|---|
| REQ-87B-001, -002, -006 | §4 | `test_bus_diff_87b.c` T-87B-01..03 |
| REQ-87B-003 | §2 | inspection of `protection_pass()` |
| REQ-87B-004, -005 | §4 | T-87B-04, T-87B-05 |
| REQ-87B-007 | §3, §4 | T-87B-06 |
| REQ-87B-008..012, -022, -025, -026, -030 | §3 | `test_settings_rules.c` T-87B-R01..R09, Playwright journey 6 |
| REQ-87B-013 | §7 | T-87B-07 (host), realtime.json `bus_fault_internal.trip_ms` |
| REQ-87B-014, -034 | §4 | T-87B-08, sweep T-87B-SW |
| REQ-87B-015, -016, -017 | §5 | T-87B-09 |
| REQ-87B-018, -019 | §4 | T-87B-10 |
| REQ-87B-020 | §1 | analysis only |
| REQ-87B-021 | §6 | T-87B-11 |
| REQ-87B-023, -027 | §6 | T-87B-12, `ct_lost_bay1_load`, sweep T-87B-SW |
| REQ-87B-024 | §6 | T-87B-13 |
| REQ-87B-028, -029 | §6 | T-87B-14, T-87B-15 |
| REQ-87B-031, -032, -036 | §7 | realtime.json before/after |
| REQ-87B-033 | §5 | T-87B-09 |
| REQ-87B-035, -037 | — | inspection |

## 9. Blast radius

Files touched: `firmware/src/prot/bus_diff_87b.{h,c}` (new),
`firmware/src/ied/event_recorder.{h,c}` (four appended SOE codes),
`firmware/testset/testset.c` (Kirchhoff after the bay-1 breaker opens: the
infeed no longer supplies bay 1 — before this fix the test set left a standing
0.4 pu differential after every bay-1 trip, invisible to feeder elements but a
false internal fault to 87B),
`firmware/src/prot/trip_matrix.{h,c}` (append `ELEM_87B`, route to TRIP1 and
TRIPB), `firmware/src/ied/ied_app.{h,c}` (settings struct, finalize, bay 2–4
phasors, element call, SOE, operands), `firmware/src/ied/settings_file.c`
(keys), `shared/relay_math/settings_rules.{h,c}` (inputs and R-87B rules),
`shared/relay_math/wasm_exports.c` only if the ABI needs it, `firmware/testset`
(expected elements for `bus_fault_internal`), `tools/build_realtime_report.py`
and `tools/run_renode.py` (element name table, expectation), `tool/` (87B page,
settings model), tests and docs.

Measurement chain: bays 2–4 samples are now mimic-filtered in `ingest()`;
nothing else reads bays 2–4 from the history (PMU and all feeder elements use
bay 1 and the VTs), and bay-1 / VT processing is unchanged.

Elements affected: none of 50/51/21/81/50BF read anything 87B writes. The only
behavioural change to an existing scenario is `bus_fault_internal`, which goes
from "no trip (87B absent)" to `87B` with bus trip; every other scenario keeps
its expected elements and trip time within one protection pass (REQ-87B-032).
Operands added: `87B_PKP/OP/BLK/CTS_ALM/ENA` and six actual values. The trip
matrix gains one row; existing rows are unchanged.

What cannot change: expected elements and trip times of `steady_load`,
`fault_ag_zone1`, `fault_bc_zone2`, `fault_ag_high_resistance`,
`breaker_failure`, `bus_fault_external`, `ct_lost_bay1_load`; the acquisition
ISR; the shared rule codes R-51/50/21/BF/81/VT/CT (new codes are appended).
