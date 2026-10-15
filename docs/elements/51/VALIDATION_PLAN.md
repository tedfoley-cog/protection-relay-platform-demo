# Validation plan: 51 time-overcurrent

## 1. Scope
51P/51G, firmware `main`, settings `settings/relay.set`.

## 2. Matrix
| ID | Requirement | Condition | Expected | Level | Result |
|---|---|---|---|---|---|
| V-51-01 | REQ-51-002 | parity vectors, all 8 curves, M = 1.5-20 | firmware = shared = tool within 1e-5 | parity | pass |
| V-51-02 | REQ-51-003 | IEEE VI, TMS 0.5, 3× | 1.471 s | unit / tool | pass |
| V-51-03 | REQ-51-001 | 51G, `fault_ag_high_resistance` | `51G` operates, no 21 | scenario host + Renode | pass |
| V-51-04 | REQ-51-005/006 | out-of-range TMS, pickup above 50P | group rejected, active group unchanged | `settings_write` CLI | pass |
| V-51-05 | REQ-51-007 | `ct_lost_bay1_load` | no trip, `blocked_passes > 0` | scenario | pass |
| V-51-06 | REQ-51-009 | all scenarios | protection task within 12 000 cycles | real-time report | pass |

## 3. Evidence
`make -C firmware test`, `python3 tools/build_realtime_report.py`,
`tool/playwright-report`.
