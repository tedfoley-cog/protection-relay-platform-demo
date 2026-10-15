# Design specification: 51 time-overcurrent (51P / 51G)

| | |
|---|---|
| Requirement set | `REQUIREMENTS.md` |
| Safety classification | protection function |
| Governing standards | IEC 60255-1, IEC 60255-151, IEEE C37.112 |
| Status | implemented |

## 1. Function
Inverse-time overcurrent on bay-1 phase currents (51P) and residual current
(51G). Outputs `51P_PKP`, `51P_OP`, `51G_PKP`, `51G_OP`; trips breaker 1.

## 2. Inputs
Phasors `IED_CH_IA..IC` (51P) and `IED_CH_IN` (51G) from `ied_prot_ctx_t`,
mimic-compensated, primary A. Blocked when `invalid_mask & IED_BAY_MASK(0)`.

## 3. Settings
| Key | Range | Default | Rule |
|---|---|---|---|
| `51P_PICKUP_A` / `51G_PICKUP_A` | 10-20000 | 600 / 120 | R-51-001 |
| `51P_TMS` / `51G_TMS` | 0.05-15.0 | 0.5 | R-51-002 |
| `51P_CURVE` / `51G_CURVE` | curve name | IEEE_VI | R-51-003 |
| `51P_DEFINITE_S` / `51G_DEFINITE_S` | 0-100 s | 0 | R-51-004 |

## 4. Algorithm
Multiple `M = I / I_pickup`. Operate time `t(M)` from
`toc_curve_operate_time` (shared). Integration: per pass the accumulator adds
`IED_PROT_PERIOD_S / t(M)`; operate when ≥ 1.0. Reset: IEEE curves integrate
`-IED_PROT_PERIOD_S / t_reset(M)`; IEC curves and DEFINITE clear the
accumulator. Pickup uses the largest phase.

## 5. Operands
`51P_PKP` while M > 1; `51P_OP` when accumulator ≥ 1, held until reset.

## 6. Degraded conditions
Invalid input: blocked, accumulator held (REQ-51-007). Settings write:
`oc_reset` (REQ-51-008). Power-up: blocked one cycle (platform).

## 7. Budget
≈ 900 cycles per pass measured (`realtime.json`, protection task).

## 8. Traceability
See `REQUIREMENTS.md` acceptance column and `VALIDATION_PLAN.md`.
