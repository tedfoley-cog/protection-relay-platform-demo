# 51 time-overcurrent: requirements

Worked example of the convention. Source: platform baseline (no customer
document); standards IEC 60255-151:2009, IEEE C37.112-2018.

| ID | Statement | Acceptance | Verification | Status |
|---|---|---|---|---|
| REQ-51-001 | The 51P element shall pick up when any bay-1 phase current exceeds `51P_PICKUP_A` (primary A). | pickup at 1.00-1.05 × setting in a 1 % sweep | test | verified |
| REQ-51-002 | Operate time shall follow the selected curve from `TOC_IEC_SI, VI, EI, LTI, IEEE_MI, VI, EI, DEFINITE` with the equations of IEC 60255-151 Table 1 and IEEE C37.112 Table 1. | `test_shared_parity`, `test_settings_rules` | test | verified |
| REQ-51-003 | Operate time at 3 × pickup, IEEE VI, TMS 0.5 shall be 1.47 s ± 1 pass. | `t51p-at-3` in the tool and `fault_ag_high_resistance` for 51G | test | verified |
| REQ-51-004 | Below pickup the element shall reset following the curve's reset equation (IEEE) or instantaneously (IEC). | unit test | test | verified |
| REQ-51-005 | Time multiplier range 0.05-15.0; pickup 10-20 000 A; definite time 0-100 s. | rules R-51-001, R-51-002, R-51-004 | test | verified |
| REQ-51-006 | 51P pickup shall be below 50P pickup. | rule R-51-005 | test | verified |
| REQ-51-007 | While any bay-1 current channel is invalid the element shall not pick up and shall not advance its timer. | `test_invalid_input` | test | verified |
| REQ-51-008 | A settings-group write shall reset the element timer. | `test_settings_rules` (apply case) | test | verified |
| REQ-51-009 | The element shall add no more than 1 500 cycles to the protection pass. | real-time report | test | verified |
