# Implementation plan — protection-relay platform + setting tool (Pilot 1 substrate)

Written before the rest of the scaffold. This is the design the repository is built to; the
repo tree in §4 is the contract, and the dry run in §6 is what "done" means.

## 1. What the demo proves

An external Agentic SDLC mechanism (orchestrator → analyst → architect → developer → QA →
reviewer, with human approval gates, `BLOCKED` questions and an audit trail) can take a
**flawed, incomplete product-owner requirement for a new protection element** — ANSI 87B,
low-impedance busbar differential — and turn it into a reviewed, requirement-traced,
merge-ready **draft** PR on a realistic relay-firmware platform, without any agent merging,
approving or advancing a ticket past "ready for review".

The repo therefore ships the **platform, conventions and evidence rails**, not the element:

- a protection relay (IED) firmware with existing 50/51/21/50BF/81 elements, full-cycle DFT
  phasors, a 1920 Hz sampling / 480 Hz protection execution model, PMU, DNP3 and SOE;
- a measurement chain with **four bays** (own CT ratio each, three-phase currents, one
  common time reference, per-unit helpers) and a secondary-injection test set that already
  produces an internal and an external bus fault — with **no 87B logic anywhere on `main`**;
- a desktop-style **setting tool** (Vite + React + TypeScript) that edits the relay settings
  file, validates element rules, and plots the 51 inverse-time characteristic from the
  **same C math the firmware runs** (compiled to WASM);
- element conventions (design-spec template, manual chapter, validation plan, `REQ-<ELEMENT>-nnn`
  IDs, operand/data-model layout with IEC 61850 mapping) worked out on the existing 51 element
  so agents have one complete example to imitate;
- a safety-critical-change discipline (skill, CODEOWNERS two-reviewer rule, deterministic
  failure-mode checks) that the mechanism must satisfy;
- a harness contract (`harness.yaml`, `docs/HARNESS_CONTRACT.md`) telling the mechanism how to
  build, test, lint, trace and where the protected paths are;
- the flawed customer requirement `requirements/customer/87B-customer-requirement.md`.

A private companion repo (`…-reference-answer`) holds the reference specification, the
planted-problem list and an independent closed-form oracle so a live session cannot read them.

## 2. What Devin does live

Every step runs inside a Devin session; the presenter never types in a terminal.

1. **Harness read-in** — Devin opens `harness.yaml`, the 51 element's spec/manual/validation
   plan and the four-bay measurement-chain doc, and states the conventions it will follow.
2. **Requirement analysis** — Devin reads the customer 87B requirement, produces an atomic
   `REQ-87B-nnn` list, and raises `BLOCKED` questions for the genuine ambiguities (slopes,
   seal-in drop-out, per-unit base, bay 4 CT source, block / disable / power-up /
   setting-change behaviour). The presenter answers as the product owner.
3. **Design** — Devin fills `docs/templates/DESIGN_SPEC_TEMPLATE.md` for 87B: operands,
   settings table, characteristic, data model placement, IEC 61850 `PDIF` mapping.
4. **Implementation** — on a branch, Devin follows `.agents/skills/safety-critical-change`:
   pins behaviour, writes the failing test (internal bus fault not cleared), adds the shared
   `shared/relay_math/diff_char.c` characteristic, the firmware element, settings-file keys,
   settings rules, and the tool's 87B view, then the ≥500-case seeded invariant ("no operate on
   healthy load with any single CT invalid").
5. **Evidence** — Devin runs `make -C firmware test`, the host simulator, the Renode real-time
   report (budgets held on the emulated STM32F407), the shared-math parity vectors, and the
   Playwright gauntlet; it opens a **draft PR** whose body carries Defect / Root cause / Fix /
   Evidence / Blast radius and the requirement-ID trace, and stops at "ready for review".
6. **UI finale** — Devin starts the setting tool in its own browser, clicks through the new
   87B settings, the slope characteristic and the write-to-relay drawer, and reports verdicts.

Fallback if the external mechanism is unavailable: the single-session prompt in
`DEMO_NOTES.md` runs steps 1–6 in one Devin session.

## 3. Stack and rationale

| Layer | Choice | Why | Reference |
|---|---|---|---|
| Firmware | C11, `gcc` host build, `arm-none-eabi-gcc` Cortex-M4F image | Existing platform; relays ship bare-metal C | — |
| Emulation | Renode 1.15.3 **portable tarball** (`renode-1.15.3.linux-portable-dotnet.tar.gz`) | The `.deb` depends on Mono packages absent from Ubuntu 24.04 runners; the portable build is self-contained | https://github.com/renode/renode/releases/tag/v1.15.3 |
| Curves | IEC 60255-151 (k, α) and IEEE C37.112 (A, B, p, tr) constants | Both standards' published constants; already in the firmware | IEC 60255-151:2009 Table 1; IEEE Std C37.112-2018 Table 1 |
| Shared math | one C source under `shared/relay_math/`, compiled into firmware **and** to WASM with Emscripten (`emcc -O2 -sEXPORTED_FUNCTIONS=…`), generated JS + `.wasm` checked in; CI rebuilds and diffs | One implementation of curve/threshold math, zero-copy into the tool; Emscripten is the mature C→browser path | https://emscripten.org/docs/tools_reference/emcc.html |
| Setting tool | Vite 5 + React 18 + TypeScript, port 3000, settings file served via a Vite dev/preview middleware | Desktop-style tooling the customer's web developers recognise; file access stays local | https://vitejs.dev/guide/api-plugin.html#configureserver |
| Settings file | flat `KEY = VALUE` ASCII (`settings/relay.set`) parsed by firmware (`src/ied/settings_file.c`) and by the tool | Mirrors vendor ASCII settings exports; trivially diffable in PRs | — |
| UI tests | Playwright (Chromium), `tests/ui/`, mandatory on every push | Playbook requirement; drives the real tool against a temp copy of the settings file | https://playwright.dev/docs/test-webserver |
| Data model | operands in `firmware/src/ied/operands.h` (types/enums only); IEC 61850-7-4 LNs `PTOC/PIOC/PDIS/RBRF/PTOF/PTUF/PFRC/PTRC/MMXU/RDRE`; 87B maps to `PDIF` | Keeps the data model free of settings/scaling/logic (checked deterministically) | IEC 61850-7-4:2010 |
| Safety | IEC 60255-1 (general), IEC 60255-151 (overcurrent), IEC 60255-187-1 (differential functional requirements, transformer/generator/motor — busbar read-across is an open question), IEC 61508 (process vocabulary only) | Governing standards named in the harness contract; **no certification credit claimed** | — |

## 4. Planned repo tree

```
protection-relay-platform-demo/
├── README.md                          flowchart, what the demo shows / Devin does, run, layout, concepts
├── DEMO_NOTES.md                      5–8 bullets; last bullet = UI testing finale; single-session prompt
├── harness.yaml                       machine-readable harness contract
├── playwright.config.ts               tests/ui, port 3000, temp settings file
├── .github/CODEOWNERS                 two human reviewers on protection / measurement / trip paths
├── .github/workflows/ci.yml           firmware-host, firmware-target (Renode), shared-parity, tool+UI, checks
├── .agents/skills/safety-critical-change/SKILL.md
├── docs/
│   ├── IMPLEMENTATION_PLAN.md  HARNESS_CONTRACT.md  flowchart.html  flowchart.png
│   ├── platform/PLATFORM_FIGURES.md         1920/480 Hz, DFT, filter response, rates — single source
│   ├── platform/PERFORMANCE_TARGETS.md      customer-fillable template (operate, drop-out, accuracy, seal-in)
│   ├── platform/MEASUREMENT_CHAIN.md        four bays, CT ratios, polarity, time reference, pu helpers
│   ├── platform/DATA_MODEL.md               operands, actual values, events, IEC 61850 mapping, open items
│   ├── standards/CODING_STANDARD.md  INTERFACE_CONTRACTS.md  REQUIREMENTS_CONVENTION.md
│   ├── templates/DESIGN_SPEC_TEMPLATE.md  VALIDATION_PLAN_TEMPLATE.md
│   └── elements/51/REQUIREMENTS.md  DESIGN_SPEC.md  MANUAL.md  VALIDATION_PLAN.md  characteristic.svg
├── requirements/customer/87B-customer-requirement.md
├── settings/relay.set                 the relay settings file the tool edits
├── shared/relay_math/                 toc_curve.[ch]  pu_scale.[ch]  settings_rules.[ch]  wasm_exports.c
│   ├── Makefile                       host lib, vector generator, `make wasm`
│   └── vectors/parity_vectors.json    generated by the C build, replayed by the tool's tests
├── firmware/
│   ├── include/ied_config.h           platform figures + four-bay channel map
│   ├── include/bay_config.h           bay table: name, CT ratio, polarity, channel base
│   ├── src/{dsp,prot,pmu,rt,ied}/     (ied/operands.h, ied/settings_file.c added)
│   ├── sim/ied_sim.c  pmu_compliance.c  settings_write.c   ← protocol-write bypass CLI
│   ├── testset/                       four-bay scenarios incl. bus_fault_internal / bus_fault_external
│   ├── tests/test_*.c                 + test_bus_zone.c  test_settings_rules.c  test_invalid_input.c
│   ├── renode/  target/stm32f407/
├── tool/                              Vite + React + TS setting tool (port 3000)
│   └── src/{views,lib,wasm}/
├── tests/ui/*.spec.ts  UI_TEST_PLAN.md
└── tools/run_renode.py  build_realtime_report.py  check_failure_modes.py  check_protected_paths.py
```

## 5. Flowchart

Nodes: `Customer requirement` → `Harness contract` → `Analyst: REQ-87B-nnn + BLOCKED` →
`Product owner answers` → `Architect: design spec` → `Developer: shared math + firmware + tool`
→ `QA: tests, Renode report, parity, gauntlet` → `Reviewer: traceability + safety skill` →
`Draft PR (ready for review)` → `Humans: 2 reviewers`. Side nodes: `Private oracle` scores the
PR (dashed edge from Draft PR), `Platform: 4-bay test set, 51 example, data model` feeds
Developer and QA. Mermaid settings: `htmlLabels:false`, no fontFamily, short unquoted labels,
`nodeSpacing 40 / rankSpacing 50`.

## 6. Runtime commands and outputs

| Command | Output |
|---|---|
| `make -C firmware test` | host unit tests incl. bus-zone KCL, settings rules, invalid-input |
| `make -C firmware sim && firmware/build/ied_sim --scenario bus_fault_internal` | SOE log; today: no element operates (87B absent) |
| `python tools/build_realtime_report.py` | builds ELF, boots Renode per scenario, writes `tool/public/realtime.json`, fails on budget or wrong element |
| `make -C shared/relay_math vectors wasm` | `vectors/parity_vectors.json`, `tool/src/wasm/relay_math.{js,wasm}` |
| `firmware/build/settings_write settings/relay.set 51P_TMS=0.05` | protocol-write path: firmware-side rule rejects out-of-range value |
| `npm --prefix tool run dev` | setting tool on :3000 |
| `npx playwright test` | gauntlet (≥5 journeys) |
| `python tools/check_failure_modes.py` | deterministic data-model / guard-count / invalid-input checks |

## 7. Primary visual artifact

The **setting tool** (Settings → Characteristic → Operands → Events → Write drawer). The
real-time report is secondary and is displayed inside the tool's Events view.

## 8. UI test gauntlet

1. Multi-view navigation: Settings → Characteristic → Operands → Events; assert domain facts
   (51P pickup 600 A, IEEE VI curve, `PTOC1` mapping, scenario names).
2. Invalid-then-valid form: TMS 0.01 rejected by rule `R-51-002` with firmware-identical
   message; 0.5 accepted; dirty indicator and curve re-plot.
3. Filter/sort/search on operands: filter `Element=21`, sort by LN, search `Z2` → exact rows.
4. Write drawer: open, see diff, confirm → settings file rewritten, banner, drawer closed,
   reload shows value persisted.
5. Golden path: edit 21 Z2 reach below Z1 (rule `R-21-001`), fix, open characteristic, verify
   curve point at 5× pickup equals WASM/C value, write, navigate away and back — state preserved.
Plus: reject a bay CT ratio of 0 in the measurement-chain form; events search by element.

## 9. CI plan

Jobs on every push: `firmware-host` (make test, sim smoke, settings_write bypass test),
`firmware-target` (arm-gcc + Renode portable, real-time report, upload JSON),
`shared-parity` (emsdk, rebuild wasm, diff against committed, run vectors in Node),
`tool-ui` (npm ci, tsc, eslint, build, Playwright), `checks` (failure-mode script, protected
paths need two human approvals — enforced by CODEOWNERS + branch protection; script verifies
the CODEOWNERS covers every protected path listed in `harness.yaml`).

## 10. Risks and unknowns

- Protection-task budget with 19 channels: the 87B pass will add 12 full-cycle DFTs at 480 Hz;
  the report will show headroom — it is a real platform figure the customer must confirm.
- Emscripten in CI adds ~2 min; mitigated by caching emsdk.
- Renode is instruction-accurate, not cycle-accurate; budgets are relative evidence.
- IEC 60255-187-1 covers transformer/generator/motor differential; whether the customer wants
  its vocabulary for busbar is an open question, as is the per-unit base (bus MVA vs largest CT).
- Settings file write from the browser needs the Vite middleware; a packaged desktop shell
  (Tauri/Electron) is out of scope.
