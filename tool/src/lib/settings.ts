// Relay settings group: the same KEY = VALUE file the firmware parses
// (firmware/src/ied/settings_file.c). Key names, units and the rules-input
// layout mirror the firmware; the shared C decides validity.
import type { RelayMath, Violation } from "./wasm";

export type Polarity = "BUS" | "LINE";
export type BayRole = "FEEDER" | "INFEED" | "TIE" | "SPARE";

export interface Bay {
  name: string;
  role: BayRole;
  ctPrimaryA: number;
  ctSecondaryA: number;
  polarity: Polarity;
  ctConnected: boolean;
}

export interface Settings {
  vtVLnNominal: number;
  sourceXOverR: number;
  i50pPickupA: number;
  i50pSecurityPasses: number;
  i50gPickupA: number;
  i50gSecurityPasses: number;
  t51pPickupA: number;
  t51pTms: number;
  t51pCurve: string;
  t51pDefiniteS: number;
  t51gPickupA: number;
  t51gTms: number;
  t51gCurve: string;
  t51gDefiniteS: number;
  z1LineROhm: number;
  z1LineXOhm: number;
  z1ReachPct: number;
  z2ReachPct: number;
  z2DelayS: number;
  minLoopA: number;
  minResidualA: number;
  bfPickupA: number;
  bfRetripS: number;
  bfDelayS: number;
  f81uPickupHz: number;
  f81oPickupHz: number;
  f81rPickupHzS: number;
  f81UvInhibitPu: number;
  bays: Bay[];
}

/* Relay defaults (firmware/src/ied/ied_app.c k_default_settings). */
export const DEFAULT_SETTINGS: Settings = {
  vtVLnNominal: 7200,
  sourceXOverR: 4,
  i50pPickupA: 6000,
  i50pSecurityPasses: 2,
  i50gPickupA: 6000,
  i50gSecurityPasses: 2,
  t51pPickupA: 600,
  t51pTms: 0.5,
  t51pCurve: "IEEE_VI",
  t51pDefiniteS: 0,
  t51gPickupA: 120,
  t51gTms: 0.5,
  t51gCurve: "IEEE_VI",
  t51gDefiniteS: 0,
  z1LineROhm: 0.95,
  z1LineXOhm: 2.45,
  z1ReachPct: 80,
  z2ReachPct: 120,
  z2DelayS: 0.3,
  minLoopA: 100,
  minResidualA: 50,
  bfPickupA: 150,
  bfRetripS: 0.02,
  bfDelayS: 0.15,
  f81uPickupHz: 59.3,
  f81oPickupHz: 60.5,
  f81rPickupHzS: 1.0,
  f81UvInhibitPu: 0.6,
  bays: [
    { name: "Feeder 1", role: "FEEDER", ctPrimaryA: 1200, ctSecondaryA: 5, polarity: "LINE", ctConnected: true },
    { name: "Transformer T1", role: "INFEED", ctPrimaryA: 2000, ctSecondaryA: 5, polarity: "BUS", ctConnected: true },
    { name: "Feeder 3", role: "FEEDER", ctPrimaryA: 600, ctSecondaryA: 5, polarity: "BUS", ctConnected: true },
    { name: "Feeder 4", role: "FEEDER", ctPrimaryA: 800, ctSecondaryA: 5, polarity: "BUS", ctConnected: true },
  ],
};

type NumKey = Exclude<keyof Settings, "bays" | "t51pCurve" | "t51gCurve">;

const FILE_KEYS: Array<[string, keyof Settings]> = [
  ["VT_V_LN_NOMINAL", "vtVLnNominal"],
  ["SOURCE_X_OVER_R", "sourceXOverR"],
  ["50P_PICKUP_A", "i50pPickupA"],
  ["50P_SECURITY_PASSES", "i50pSecurityPasses"],
  ["50G_PICKUP_A", "i50gPickupA"],
  ["50G_SECURITY_PASSES", "i50gSecurityPasses"],
  ["51P_PICKUP_A", "t51pPickupA"],
  ["51P_TMS", "t51pTms"],
  ["51P_CURVE", "t51pCurve"],
  ["51P_DEFINITE_S", "t51pDefiniteS"],
  ["51G_PICKUP_A", "t51gPickupA"],
  ["51G_TMS", "t51gTms"],
  ["51G_CURVE", "t51gCurve"],
  ["51G_DEFINITE_S", "t51gDefiniteS"],
  ["21_Z1_LINE_R_OHM", "z1LineROhm"],
  ["21_Z1_LINE_X_OHM", "z1LineXOhm"],
  ["21_Z1_REACH_PCT", "z1ReachPct"],
  ["21_Z2_REACH_PCT", "z2ReachPct"],
  ["21_Z2_DELAY_S", "z2DelayS"],
  ["21_MIN_LOOP_A", "minLoopA"],
  ["21_MIN_RESIDUAL_A", "minResidualA"],
  ["50BF_PICKUP_A", "bfPickupA"],
  ["50BF_RETRIP_S", "bfRetripS"],
  ["50BF_DELAY_S", "bfDelayS"],
  ["81U_PICKUP_HZ", "f81uPickupHz"],
  ["81O_PICKUP_HZ", "f81oPickupHz"],
  ["81R_PICKUP_HZ_S", "f81rPickupHzS"],
  ["81_UV_INHIBIT_PU", "f81UvInhibitPu"],
];

export function toSettingsFile(s: Settings): string {
  const lines = ["# Relay settings group (KEY = VALUE)"];
  for (const [key, field] of FILE_KEYS) lines.push(`${key} = ${String(s[field])}`);
  s.bays.forEach((b, k) => {
    const n = k + 1;
    lines.push(`BAY${n}_CT_PRIMARY_A = ${b.ctPrimaryA}`);
    lines.push(`BAY${n}_CT_SECONDARY_A = ${b.ctSecondaryA}`);
    lines.push(`BAY${n}_POLARITY = ${b.polarity}`);
    lines.push(`BAY${n}_CT_CONNECTED = ${b.ctConnected ? "YES" : "NO"}`);
  });
  return lines.join("\n") + "\n";
}

export function parseSettingsFile(text: string, base: Settings): { settings: Settings; errors: string[] } {
  const s: Settings = { ...base, bays: base.bays.map((b) => ({ ...b })) };
  const errors: string[] = [];
  const byKey = new Map(FILE_KEYS);
  text.split(/\r?\n/).forEach((raw, idx) => {
    const line = raw.replace(/#.*$/, "").trim();
    if (!line) return;
    const eq = line.indexOf("=");
    if (eq < 0) {
      errors.push(`line ${idx + 1}: expected KEY = VALUE`);
      return;
    }
    const key = line.slice(0, eq).trim();
    const value = line.slice(eq + 1).trim();
    const bay = /^BAY([1-4])_(CT_PRIMARY_A|CT_SECONDARY_A|POLARITY|CT_CONNECTED)$/.exec(key);
    if (bay) {
      const b = s.bays[Number(bay[1]) - 1];
      if (bay[2] === "POLARITY") {
        if (value !== "BUS" && value !== "LINE") errors.push(`line ${idx + 1}: ${key} must be BUS or LINE`);
        else b.polarity = value;
      } else if (bay[2] === "CT_CONNECTED") {
        if (value !== "YES" && value !== "NO") errors.push(`line ${idx + 1}: ${key} must be YES or NO`);
        else b.ctConnected = value === "YES";
      } else {
        const f = Number(value);
        if (!Number.isFinite(f)) errors.push(`line ${idx + 1}: ${key} is not a number`);
        else if (bay[2] === "CT_PRIMARY_A") b.ctPrimaryA = f;
        else b.ctSecondaryA = f;
      }
      return;
    }
    const field = byKey.get(key);
    if (!field) {
      errors.push(`line ${idx + 1}: unknown setting ${key}`);
      return;
    }
    if (field === "t51pCurve" || field === "t51gCurve") {
      s[field] = value;
    } else {
      const f = Number(value);
      if (!Number.isFinite(f)) errors.push(`line ${idx + 1}: ${key} is not a number`);
      else (s as any)[field as NumKey] = f;
    }
  });
  return { settings: s, errors };
}

/* Rules input vector in RULES_IN_* order (shared/relay_math/settings_rules.h). */
export function rulesInput(s: Settings, rm: RelayMath): Float32Array {
  const v = new Float32Array(rm.rulesInputNames.length);
  const vals = [
    s.t51pPickupA, s.t51pTms, rm.curveFromName(s.t51pCurve), s.t51pDefiniteS,
    s.t51gPickupA, s.t51gTms, rm.curveFromName(s.t51gCurve), s.t51gDefiniteS,
    s.i50pPickupA, s.i50gPickupA,
    s.z1ReachPct, s.z2ReachPct, s.z2DelayS, s.minLoopA,
    s.bfPickupA, s.bfRetripS, s.bfDelayS,
    s.f81uPickupHz, s.f81oPickupHz, s.f81rPickupHzS,
    s.vtVLnNominal,
    ...s.bays.flatMap((b) => [b.ctPrimaryA, b.ctSecondaryA]),
  ];
  vals.forEach((x, k) => (v[k] = x));
  return v;
}

/* Attribute each violation to the field(s) that cause it, using the shared
 * math only: a candidate field is a culprit when resetting it to its default
 * makes the violation disappear. Ordering rules (R-21-003) keep both fields. */
const RELATIONAL_RULES = new Set(["R-21-003", "R-BF-002", "R-81-001"]);

export function checkSettings(s: Settings, rm: RelayMath): Violation[] {
  const found = rm.rulesCheck(rulesInput(s, rm));
  return found.map((v) => {
    const candidates = RULE_FIELDS[v.id] ?? [];
    if (candidates.length < 2 || RELATIONAL_RULES.has(v.id)) return { ...v, fields: candidates };
    const culprits = candidates.filter((f) => {
      const probe = { ...s, [f]: DEFAULT_SETTINGS[f as keyof Settings] } as Settings;
      return !rm.rulesCheck(rulesInput(probe, rm)).some((w) => w.id === v.id && w.detail === v.detail && w.value === v.value);
    });
    return { ...v, fields: culprits.length ? culprits : candidates };
  });
}

/* Which form fields a rule points at, for inline highlighting. */
export const RULE_FIELDS: Record<string, string[]> = {
  "R-51-001": ["t51pPickupA", "t51gPickupA"],
  "R-51-002": ["t51pTms", "t51gTms"],
  "R-51-003": ["t51pCurve", "t51gCurve"],
  "R-51-004": ["t51pDefiniteS", "t51gDefiniteS"],
  "R-51-005": ["t51pPickupA"],
  "R-50-001": ["i50pPickupA", "i50gPickupA"],
  "R-21-001": ["z1ReachPct"],
  "R-21-002": ["z2ReachPct"],
  "R-21-003": ["z1ReachPct", "z2ReachPct"],
  "R-21-004": ["z2DelayS"],
  "R-21-005": ["minLoopA"],
  "R-BF-001": ["bfPickupA"],
  "R-BF-002": ["bfRetripS", "bfDelayS"],
  "R-81-001": ["f81uPickupHz", "f81oPickupHz"],
  "R-81-002": ["f81rPickupHzS"],
  "R-VT-001": ["vtVLnNominal"],
  "R-CT-001": ["ctPrimaryA"],
  "R-CT-002": ["ctSecondaryA"],
};

export const SECTION_OF_RULE = (id: string): string => {
  if (id.startsWith("R-51") || id.startsWith("R-50-")) return "overcurrent";
  if (id.startsWith("R-21")) return "distance";
  if (id.startsWith("R-BF")) return "breaker-failure";
  if (id.startsWith("R-81")) return "frequency";
  return "bays";
};
