# Dry-run evidence (scaffold baseline)

Run inside the Devin session that built the scaffold, on `main` @ 8a63cc9. Kept on a branch so `main` stays the day-zero state.

## Commands and results

| Command | Result |
|---|---|
| `make -C shared/relay_math all` | builds; parity vectors regenerated |
| `make -C firmware test` | bus_zone 5/5, ied_protection 3/3, invalid_input 3/3, pmu 3/3, settings_rules 5/5, shared_parity 4/4 |
| `make -C firmware/target/stm32f407` | STM32F407 image builds with `-Werror` |
| `python3 tools/run_renode.py --all` / `build_realtime_report.py` | 8/8 scenarios as expected; tasks over budget: none (acq 168/2000, protection 6447/12000, pmu within 30000 after limiting the PMU DFT to its 7 reported channels, comms 15960/40000 cycles) |
| `make -C shared/relay_math wasm` | `tool/src/wasm/relay_math.js` regenerated (ENVIRONMENT=web) |
| `cd tool && npm run lint && npm run build` | clean |
| `cd tool && npm run test:e2e` | 5/5 journeys pass (navigation, invalid→valid, filter/sort/search, drawer side effects, golden path + reload) |
| `./firmware/build/sim/settings_write settings/relay.set 51P_TMS=0.01 21_Z2_REACH_PCT=70` | REJECTED: R-51-002, R-21-002, R-21-003; active group unchanged |
| `./firmware/build/sim/settings_write settings/relay.set 51P_TMS=0.8 51P_CURVE=IEC_SI` | ACCEPTED 2 overrides |
| `python3 tools/check_failure_modes.py` | 0 findings |
| `python3 tools/check_traceability.py` | 9 requirements, 0 unreferenced |
| `python3 tools/check_protected_paths.py --base <first commit>` | lists protection / measurement_chain / trip_logic hits |

## Throwaway 87B sketch (branch deleted, never pushed)

A 40-line host test computed per-phase differential / restraint from `testset_bay_current()` with the customer's settings as written (pickup 0.2 pu, slopes 60 % / 25 %, breakpoint 2 pu, bus-MVA base 926 A):

```
cc -std=c11 -Wall -Wextra -Werror -I../shared/relay_math -Iinclude -Isrc -Isrc/dsp -Isrc/prot -Isrc/pmu -Isrc/rt -Isrc/ied -Itestset -Itests --coverage -O0 -g tests/test_87b_sketch.c ../shared/relay_math/toc_curve.c ../shared/relay_math/pu_scale.c ../shared/relay_math/settings_rules.c src/dnp3_outstation.c src/dsp/phasor.c src/dsp/symcomp.c src/dsp/sample_history.c src/prot/oc_element.c src/prot/distance.c src/prot/breaker_failure.c src/prot/freq_element.c src/prot/trip_matrix.c src/pmu/timebase.c src/pmu/synchrophasor.c src/pmu/c37118.c src/rt/sample_ring.c src/rt/cycle_stats.c src/ied/event_recorder.c src/ied/ied_app.c src/ied/settings_file.c sim/host_rt.c testset/testset.c -o build/test_87b_sketch -lm
== build/test_87b_sketch
  bus_fault_internal   ph0 diff 4.072 pu rest 4.253 pu thr 1.763 -> operate
  bus_fault_internal   ph1 diff 0.000 pu rest 1.702 pu thr 1.021 -> restrain
  bus_fault_internal   ph2 diff 0.000 pu rest 1.706 pu thr 1.024 -> restrain
  bus_fault_external   ph0 diff 0.000 pu rest 1.313 pu thr 0.788 -> restrain
  bus_fault_external   ph1 diff 0.000 pu rest 7.081 pu thr 2.470 -> restrain
  bus_fault_external   ph2 diff 0.000 pu rest 6.422 pu thr 2.306 -> restrain
  steady_load          ph0 diff 0.000 pu rest 1.598 pu thr 0.959 -> restrain
  steady_load          ph1 diff 0.000 pu rest 1.598 pu thr 0.959 -> restrain
  steady_load          ph2 diff 0.000 pu rest 1.598 pu thr 0.959 -> restrain
  ct_lost_bay1_load    ph0 diff 0.000 pu rest 1.598 pu thr 0.959 -> BLOCKED
  ct_lost_bay1_load    ph1 diff 0.000 pu rest 1.598 pu thr 0.959 -> BLOCKED
  ct_lost_bay1_load    ph2 diff 0.000 pu rest 1.598 pu thr 0.959 -> BLOCKED
87b_sketch: 1 cases, 0 failed
```

Agrees with the private oracle: internal fault operates (A-phase, diff 4.07 pu vs threshold 1.76), external and load restrain (diff 0), lost bay-1 CT blocks rather than reading 0 A. Note the oracle's own cases (`ct_lost_bay1_load_if_zero_substituted`) show that treating the lost bay as 0 A would trip on healthy load — the F4 failure mode the checks exist for.

## grep for 87B on main

`grep -ri 87b firmware shared tool/src` → no matches. The only occurrences are the customer requirement, DEMO_NOTES.md, README and the flowchart.

## Lessons for the live run

* The PMU budget overran as soon as the channel count went 7 → 16; a new element that touches all bays should budget its DFT work explicitly (platform figure, not a surprise).
* `R-51-002` and `R-21-003` surface on two fields each; the tool shows both, so a live demo should click the specific form.
* Renode all-scenario run takes ~6 min on this VM; start it before narrating the design step.
