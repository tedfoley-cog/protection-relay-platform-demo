# Instruction manual — 51 Time-overcurrent protection

## Application
51P provides phase overcurrent protection for the bay-1 feeder with
inverse-time coordination to downstream devices; 51G covers ground faults
using the residual CT.

## Settings
| Setting | Range | Step | Default |
|---|---|---|---|
| 51P Pickup | 10-20 000 A primary | 1 A | 600 A |
| 51P Curve | IEC SI / VI / EI / LTI, IEEE MI / VI / EI, Definite | | IEEE VI |
| 51P Time multiplier | 0.05-15.00 | 0.01 | 0.50 |
| 51P Definite time | 0.00-100.00 s | 0.01 s | 0.00 s |
(51G identical, default pickup 120 A.)

## Characteristic
Operate time `t = TMS × (A / (M^p − 1) + B)` with the constants of IEC
60255-151 and IEEE C37.112. The setting tool plots the selected curve and
tabulates operate times at 2×, 3×, 5× and 10× pickup.

## Operation
Pickup is indicated by `51P_PKP`; operation asserts `51P_OP` and trip output
1, with an `OPERATE 51P` event. The element is blocked while any bay-1
current input is flagged invalid (`INPUT_INVALID` event).

## Specifications
| | |
|---|---|
| Pickup accuracy | ±3 % of setting |
| Timing accuracy | ±3 % or ±1 protection pass (2.1 ms), whichever greater |
| Drop-out ratio | 0.97 |
| Reset | curve reset (IEEE) / instantaneous (IEC) |
