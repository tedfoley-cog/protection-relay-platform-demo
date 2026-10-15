// Bridge to shared/relay_math compiled by Emscripten (make -C shared/relay_math wasm).
// The same C that the firmware links decides curve times and settings rules here.
// @ts-ignore generated single-file ES module
import createModule from "../wasm/relay_math.js";

export interface Violation {
  code: number;
  id: string;
  message: string;
  detail: number;
  value: number;
}

export interface RelayMath {
  curveNames: string[];
  curveFromName(name: string): number;
  operateTime(curve: number, tms: number, definiteS: number, multiple: number): number;
  resetTime(curve: number, tms: number, multiple: number): number;
  aPerCount(ctPrimaryA: number, countsPerRated: number): number;
  ctRatio(p: number, s: number): number;
  secondaryA(a: number, p: number, s: number): number;
  baseAFromMva(mva: number, kvLl: number): number;
  rulesInputNames: string[];
  rulesCheck(input: Float32Array): Violation[];
}

let instance: Promise<RelayMath> | null = null;

export function loadRelayMath(): Promise<RelayMath> {
  if (instance) return instance;
  instance = createModule().then((m: any) => {
    const str = (p: number) => m.UTF8ToString(p);
    const curveCount: number = m._rm_curve_count();
    const curveNames: string[] = [];
    for (let k = 0; k < curveCount; k++) curveNames.push(str(m._rm_curve_name(k)));
    const inCount: number = m._rm_rules_input_count();
    const rulesInputNames: string[] = [];
    for (let k = 0; k < inCount; k++) rulesInputNames.push(str(m._rm_rules_input_name(k)));
    const curveFromName = m.cwrap("rm_curve_from_name", "number", ["string"]) as (s: string) => number;
    const api: RelayMath = {
      curveNames,
      curveFromName,
      operateTime: (c, tms, d, mult) => m._rm_toc_operate_time(c, tms, d, mult),
      resetTime: (c, tms, mult) => m._rm_toc_reset_time(c, tms, mult),
      aPerCount: (p, n) => m._rm_a_per_count(p, n),
      ctRatio: (p, s) => m._rm_ct_ratio(p, s),
      secondaryA: (a, p, s) => m._rm_secondary_a(a, p, s),
      baseAFromMva: (mva, kv) => m._rm_base_a_from_mva(mva, kv),
      rulesInputNames,
      rulesCheck: (input) => {
        if (input.length !== inCount) throw new Error(`rules input must have ${inCount} entries`);
        const ptr = m._malloc(inCount * 4);
        m.HEAPF32.set(input, ptr >> 2);
        const n: number = m._rm_rules_check(ptr);
        m._free(ptr);
        const out: Violation[] = [];
        for (let k = 0; k < n; k++) {
          const code = m._rm_violation_code(k);
          out.push({
            code,
            id: str(m._rm_rule_id(code)),
            message: str(m._rm_rule_message(code)),
            detail: m._rm_violation_detail(k),
            value: m._rm_violation_value(k),
          });
        }
        return out;
      },
    };
    return api;
  });
  return instance;
}
