# Coding standard (firmware)

Scope: `firmware/src`, `firmware/include`, `shared/relay_math`. Target: C11,
`-Wall -Wextra -Werror`, no dynamic allocation after init, no recursion, no
floating-point in ISRs except the acquisition scaling already present.

## Rules

1. **One element, one module.** `firmware/src/prot/<element>.{h,c}` with
   `<element>_init`, `<element>_reset`, `<element>_step(ctx, settings, status)`.
   No element reads another element's internals; cross-element logic lives in
   trip logic through operands.
2. **Settings are inputs.** Elements take a `const` settings struct and never
   read the global group. Validation happens in `settings_file_check` and the
   shared rules, not in the element.
3. **Invalid is not zero.** A missing or invalid channel blocks the element
   (`status->blocked = true`) and is logged once; it is never substituted by
   0 A, the previous value, or a default. (`check_failure_modes.py` F3)
4. **Error paths stay.** A refactor may not remove a `return false`, a range
   check or a `blocked` assignment without a design-spec change. (F2)
5. **Deterministic time.** Timers count protection passes (`uint32_t`),
   converted from seconds once at settings-finalize. No wall-clock reads in
   elements.
6. **Shared math is shared.** Curve equations, per-unit conversion and rule
   checks are implemented once in `shared/relay_math` and linked by firmware
   and compiled to WASM for the tool. No copies in the tool.
7. **Operands are the interface.** New quantities are published as operands
   in `ied_app.c`; the tool, PMU and comms read operands, not element structs.
8. **Fixed sizes.** `IED_NUM_BAYS`, `IED_NUM_CHANNELS` arrays; no VLAs.
9. **Naming.** `snake_case` C, `UPPER_SNAKE` macros, ANSI prefix for element
   files and keys (`oc_element.c` is the historical exception; new elements
   use the ANSI number: `bus_diff_87b.c`).
10. **Tests before code.** A characterization test pins current behaviour,
    a failing test states the new behaviour, then the change
    (`.agents/skills/safety-critical-change`).

## Forbidden

`malloc` after init, `goto` except single cleanup label, implicit float→int
narrowing, `#pragma GCC diagnostic ignored`, magic numbers for platform
figures (use `ied_config.h`), `printf` in `firmware/src` (sim only).
