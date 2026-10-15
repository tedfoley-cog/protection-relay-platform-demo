# Interface contracts

Each contract is a file-level promise other modules may rely on. Breaking a
contract requires a change to the owning design spec and CODEOWNERS review.

| Contract | Owner | Consumers | Promise |
|---|---|---|---|
| `ied_prot_ctx_t` | `ied_app.c` | elements | phasors for all 16 channels from the same pass, in primary A / V, `invalid_mask` current |
| `*_status_t` | each element | trip logic, operands | written once per pass; `blocked` set whenever a required input is invalid |
| `ied_settings_t` + `ied_settings_finalize` | `settings_file.c` | elements | all values already range-checked by shared rules; derived per-pass counts computed |
| `settings_file_parse/check/write` | `settings_file.c` | CLI, DNP3 write, tool | whole-group atomic apply; identical acceptance to `shared/relay_math/settings_rules.c` |
| `toc_curve_operate_time` | `shared/relay_math` | 51 element, tool | IEC 60255-151 / IEEE C37.112 equations; returns ≤ 0 below pickup |
| `pu_*` | `shared/relay_math` | scaling, tool | pure functions, no state |
| `testset_*` | `firmware/testset` | host tests, Renode | analytic 16-channel injection; scenarios listed in `tools/run_renode.py` |
| `soe_*` | `event_recorder.c` | all | fixed codes in `soe_code_t`; new codes appended, never renumbered |
| `tool/public/realtime.json` | `tools/build_realtime_report.py` | setting tool | schema: `tasks[]`, `scenarios[]`, `compliance[]`, `rates`, `target` |

## Adding an element

1. New `firmware/src/prot/<ansi>_<name>.{h,c}` with the init/reset/step triple.
2. Settings keys in `settings_file.c` `k_keys[]`, rule(s) in
   `shared/relay_math/settings_rules.c`, rule IDs `R-<ANSI>-nnn`.
3. Operands and SOE codes appended.
4. Scenario(s) in `firmware/testset`, expectations in `tools/run_renode.py`
   and `tools/build_realtime_report.py`.
5. Tool: fields in `tool/src/lib/settings.ts`, view under `tool/src/views`.
6. Docs: `docs/elements/<ansi>/` with the four documents (see 51).
