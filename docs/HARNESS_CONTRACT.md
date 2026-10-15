# Harness contract

`harness.yaml` is the machine-readable version of this page. It tells an
external Agentic SDLC mechanism how to build, test and gate changes in this
repository without reading the code first.

## What the orchestrator gets

| Need | Where |
|---|---|
| Build / test / lint commands | `harness.yaml: commands` (all run from repo root) |
| Requirement ID grammar | `requirements.id_pattern`; convention in `docs/standards/REQUIREMENTS_CONVENTION.md` |
| Where customer asks arrive | `requirements/customer/` |
| Paths that need two human reviewers | `protected_paths` + `.github/CODEOWNERS` + `tools/check_protected_paths.py` |
| Safety classification and standards | `safety` block |
| Coding standard, interface contracts, templates | `safety.*` paths |
| Deterministic failure-mode checks | `tools/check_failure_modes.py` (F1-F4) |
| Evidence a PR must carry | `evidence_required_per_change` |
| Platform limits a new element inherits | `docs/platform/PLATFORM_FIGURES.md`, `OPEN_QUESTIONS.md` |

## Flow the contract expects

1. Intake: a file lands in `requirements/customer/`. The agent decomposes it
   per the convention into `docs/elements/<ansi>/REQUIREMENTS.md`, marking
   every gap `BLOCKED` with a written question.
2. Design: fill `DESIGN_SPEC_TEMPLATE.md`; cite platform figures and open
   questions; state the per-unit base and degraded-mode behaviour.
3. Implement under `.agents/skills/safety-critical-change`: pin, fail, fix,
   sweep, report.
4. Validate: fill `VALIDATION_PLAN_TEMPLATE.md`; run `run_target` and
   `realtime_report`; add scenarios to the test set rather than mocking.
5. Deliver: draft PR with blast-radius statement, before/after report, and
   the manual chapter. CI runs every command in `harness.yaml`; CODEOWNERS
   blocks merge without two human approvals on protected paths.

## Assumptions

Listed under `assumptions` in the YAML. The main one: **reference answers
and oracles live outside this repository**, so the agent cannot shortcut the
decomposition by reading the answer.
