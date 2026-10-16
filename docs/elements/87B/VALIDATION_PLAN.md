# Validation plan: 87B busbar differential

## 1. Scope and pass criteria

Element 87B (`firmware/src/prot/bus_diff_87b.c`), firmware at the head of
branch `devin/1792157441-87b-bus-differential`, default settings group
(`settings/relay.set` plus the 87B defaults of DESIGN_SPEC §3, prototype
assumptions A1–A13). The plan passes when every row in §3 passes on host
**and** on the Renode STM32F407 target, the real-time report shows the
protection task within `IED_BUDGET_PROT_CYCLES` on all scenarios, and no
pre-existing scenario changes its expected elements or trip time by more than
±1 protection pass (2.1 ms). Rows whose requirement is BLOCKED are executed
against the stated prototype assumption and reported as "pass (assumption
An)"; they do not close the requirement.

## 2. Test levels

| Level | Tool | What it proves |
|---|---|---|
| Unit | `firmware/tests/test_bus_diff_87b.c` | equations, characteristic, state machine, degraded modes |
| Rules | `firmware/tests/test_settings_rules.c` | R-87B-001..009 reject/accept, atomic write |
| Invariant | seeded sweep in `test_bus_diff_87b.c` (>= 500 cases, seed printed) | any in-zone bay invalid on healthy load => no trip; external fault (any feeder, any type, any location, Rf, point on wave) => no trip; internal fault at >= 2 x PU => operate |
| Scenario (host) | `firmware/tests/ied_loop.h` closed loop + `firmware/testset` | end-to-end with SOE: `bus_fault_internal`, `bus_fault_external`, `ct_lost_bay1_load`, `steady_load` |
| Scenario (target) | `tools/run_renode.py --all`, `tools/build_realtime_report.py` | same on STM32F407, cycle counts before/after |
| Tool | Playwright `tool/tests/setting-tool.spec.ts` journey 6 | 87B page, slope-order rule blocks the write, corrected write accepted |
| Parity | `test_shared_parity` | firmware and WASM rule engine agree on 87B vectors |
| Characterization | `firmware/tests/test_characterization.c` | all eight scenarios' operated elements and trip passes pinned before the change |

## 3. Test matrix

| ID | Requirement | Condition | Expected | Level | Result |
|---|---|---|---|---|---|
| T-87B-01 | REQ-87B-001 | `steady_load` and `bus_fault_external` at 0.35 s | Id < 0.05 pu on every phase | scenario (host) | |
| T-87B-02 | REQ-87B-002 | `bus_fault_external` at 0.35 s | Ir(B) > 5 pu; `steady_load` Ir(A) = 2 x 740 A / Ib +/- 2 % | scenario (host) | |
| T-87B-03 | REQ-87B-001, REQ-87B-006 | `bus_fault_internal` (A-G) | Id(A) > 2 pu; phase A operates; B, C do not | scenario (host) | |
| T-87B-04 | REQ-87B-004 | Id = 0.15 pu, Ir = 0.2 pu (below PU) and Id = 0.5 pu, Ir = 4 pu (above PU, below T) | no operate | unit | |
| T-87B-05 | REQ-87B-005 | `bus_diff_87b_threshold_pu` with S1 = 25 %, S2 = 60 %, BP = 2 | T(1) = 0.25, T(2) = 0.50, T(4) = 1.70 pu | unit | (A2) |
| T-87B-06 | REQ-87B-007 | defaults | `base_a` = 926.0 +/- 0.5 A; Id actual value of `bus_fault_internal` = Id_A / 926 A | unit | (A1) |
| T-87B-07 | REQ-87B-013 | `bus_fault_internal`, inception 0.30 s | `OPERATE 87B` SOE and TRIP1 within 25 ms of inception; measured value reported in §5 | scenario (host + target) | (A4) |
| T-87B-08 | REQ-87B-014, REQ-87B-034 | `bus_fault_external` (B-C bay 3, DC offset) | no 87B pickup-to-operate, no trip for the whole run | scenario (host + target) | (A5) |
| T-87B-SW | REQ-87B-014, REQ-87B-027 | seeded sweep >= 500 cases: (a) healthy load, random bay masks invalid; (b) external fault on bays 1, 3, 4, random type/location/Rf/point-on-wave; (c) internal fault random type/Rf with Id >= 2 x PU | (a) no trip; (b) no trip; (c) operate | invariant | |
| T-87B-09 | REQ-87B-015, REQ-87B-016, REQ-87B-017, REQ-87B-033 | `bus_fault_internal` | `trip1` and `tripb` true, `OPERATE 87B` SOE with value > 2 pu, `ied_status()->b87.operate`; `tripb` held after the fault clears until `ied_reset_lockout()` | scenario (host) | (A6) |
| T-87B-10 | REQ-87B-018, REQ-87B-019 | healthy load, bay-3 current removed from the sums with channels valid (host hook) | `cts_alarm` after 3.0 s +/- 1 pass; alarm SOE; subsequent internal fault does not operate | unit | (A7) |
| T-87B-11 | REQ-87B-021 | bay 3 out of zone, `bus_fault_external` (bay-3 fault) | Id equals the bay-3 current; bay 3 invalid does not block; R-87B-008 rejects three bays out | unit | (A9) |
| T-87B-12 | REQ-87B-023, REQ-87B-027 | `BAY4_CT_CONNECTED = NO` with bay 4 in zone, `bus_fault_internal`; `ct_lost_bay1_load` | no 87B operate, `blocked` true, `blocked_passes` increments; `ct_lost_bay1_load` no trip | scenario (host) | (A10) |
| T-87B-13 | REQ-87B-024 | `87B_FUNCTION = DISABLED`, `bus_fault_internal` | no 87B operate, no `tripb`, `enabled` false | scenario (host) | (A11) |
| T-87B-14 | REQ-87B-028 | standing internal fault from t = 0 | no operate in the first 32 samples after `ied_init` | unit | (A12) |
| T-87B-15 | REQ-87B-029 | settings write with `87B_PICKUP = 5.0` during pickup; valid write during CTS count | rejected write leaves `b87` unchanged; accepted write zeroes the CTS counter | rules | (A13) |
| T-87B-R01..R09 | REQ-87B-008, REQ-87B-009, REQ-87B-010, REQ-87B-011, REQ-87B-012, REQ-87B-022, REQ-87B-025, REQ-87B-026, REQ-87B-030 | each 87B key one step outside its range; S2 < S1; three bays out of zone | write rejected with the matching R-87B code; the same vector valid is accepted | rules + parity + Playwright | |
| T-87B-RT | REQ-87B-031, REQ-87B-032, REQ-87B-036 | `tools/build_realtime_report.py` after the change | protection max cycles <= 12 000 on all scenarios; seven pre-existing scenarios unchanged within 1 pass; `bus_fault_internal` -> `87B`, bus trip | scenario (target) | |
| T-87B-UI | REQ-87B-012, REQ-87B-030 | setting tool, 87B page | S1 = 60, S2 = 25 shows R-87B-005 on both fields and the write is blocked; S2 = 60 clears it and the write is accepted | tool | |
| T-87B-DOC | REQ-87B-037, REQ-87B-035, REQ-87B-003 | review (REQ-87B-003: one `tick` for all bay phasors in `protection_pass()`) | DESIGN_SPEC, VALIDATION_PLAN, MANUAL_CHAPTER present; 60 Hz build flag unchanged | inspection | |

REQ-87B-020 (bay 5) has no executable row: the platform has four bays.

## 4. Performance targets

| Target | Definition | Standard reference | Value | Verified by |
|---|---|---|---|---|
| Operate time at 2x pickup | Inception to trip-output assertion, 60 Hz | IEC 60255-1 §6.8; IEC 60255-187-1 §6.3 | BLOCKED (REQ-87B-013); measured value in §5 | T-87B-07 |
| Operate time at 5x / 10x pickup | as above | | same mechanism (no intentional delay); measured in sweep | T-87B-SW |
| Operate-time variation with DC offset | X/R worst-case point on wave | IEC 60255-187-1 §6.3.4 | <= 1 pass variation, sweep | T-87B-SW |
| Drop-out time | Fault cleared to `87B_OP` reset | IEC 60255-1 §6.9 | <= 1 cycle + 1 pass | T-87B-09 |
| Drop-out ratio | comparator, no hysteresis | IEC 60255-1 §6.9 | 1.0 | T-87B-04 |
| Level accuracy | pickup vs setting | IEC 60255-1 §6.5 | +/- 3 % of setting (DFT gain) | T-87B-SW |
| Transient overreach | DC offset vs steady state | IEC 60255-151 Annex | bounded by T-87B-08 / sweep | T-87B-08 |
| Seal-in | TRIPB held after operand drops | project rule | until lockout reset (A6) | T-87B-09 |
| Security on external fault | no operation, ideal CTs | IEC 60255-187-1 §6.4 | no trip | T-87B-08, T-87B-SW |
| Behaviour on invalid input | any in-zone channel invalid | platform rule | block | T-87B-12, T-87B-SW |
| Behaviour on settings change | new group applied | project rule | reset (A13) | T-87B-15 |
| Behaviour at power-up | until first full window | project rule | blocked one cycle (A12) | T-87B-14 |
| CPU budget | added protection-pass cycles | PLATFORM_FIGURES.md | <= 12 000 total; delta reported | T-87B-RT |

## 5. Evidence

Filled in by the pull request: `realtime.json` before (`/tmp/realtime-before.json`,
label `baseline`) and after (label `87b`), `make -C firmware test` log with the
sweep seed and case count, failing-test output before the implementation,
Playwright report and testing-agent recording, blast-radius statement
(DESIGN_SPEC §9), `check_failure_modes` / `check_protected_paths` /
`check_traceability` output, reviewer sign-off (two human CODEOWNERS for
protection paths).

Measured results (to be copied from the PR):

| Measurement | Host | Target |
|---|---|---|
| `bus_fault_internal` operate time (inception to TRIP1) | | |
| Protection task max cycles before / after | — | |
| Sweep seed / cases / failures | | — |
