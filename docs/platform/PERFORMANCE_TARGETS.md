# Performance targets (customer-fillable template)

Copy this table into the element's design spec and fill every cell. "TBD" is a
`BLOCKED` question for the requirement owner, not a default. Values are
verified on the Renode target by the element's validation plan.

| Target | Definition | Standard reference | Value | Verified by |
|---|---|---|---|---|
| Operate time at 2× pickup | Inception to trip-output assertion, 60 Hz, no DC offset | IEC 60255-1 §6.8; IEC 60255-187-1 §6.3 | TBD ms | scenario, Renode SOE |
| Operate time at 5× pickup | as above | | TBD ms | |
| Operate time at 10× pickup | as above | | TBD ms | |
| Operate-time variation with DC offset | X/R = 4 worst-case point on wave | IEC 60255-187-1 §6.3.4 | TBD ms max | |
| Drop-out (reset) time | Current removed to operand reset | IEC 60255-1 §6.9 | TBD ms | |
| Drop-out ratio | Reset level / pickup level | IEC 60255-1 §6.9 | TBD (e.g. 0.95) | |
| Level accuracy | Measured pickup vs setting | IEC 60255-1 §6.5 | ±TBD % of setting or ±TBD A | |
| Transient overreach | Pickup with full DC offset vs steady-state pickup | IEC 60255-151 Annex | ≤ TBD % | |
| Seal-in | Trip output held after operand drops | project rule | TBD ms minimum | |
| Security on external fault | No operation for the external bus-fault scenario, including CT saturation if modelled | IEC 60255-187-1 §6.4 | no trip | `bus_fault_external` |
| Behaviour on invalid input | No operation while any required channel is invalid | platform rule | hold / block | `ct_lost_bay1_load` pattern |
| Behaviour on settings change | Element state after a new group is applied | project rule | reset / hold | |
| Behaviour at power-up | Element state until the first full window | project rule | blocked for one cycle | |
| CPU budget | Added protection-pass cycles | `PLATFORM_FIGURES.md` | ≤ TBD cycles | real-time report |

## How the measurements are taken

* Times are read from the SOE log (`OPERATE`, `TRIP1`, `TRIP1_OFF`) relative to
  `fault_start_s` of the scenario, as `trip_ms` in `realtime.json`.
* Level accuracy sweeps are host-side (`firmware/sim/ied_sim`) with the
  secondary-injection test set at steps of 1 % around the setting.
* Seal-in and drop-out use the `external_clear_s` mechanism of the test set to
  remove the fault without opening the relay's own breaker.
