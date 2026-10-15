import { useMemo } from "react";
import type { RelayMath, Violation } from "../lib/wasm";
import type { Settings } from "../lib/settings";
import { NumField, SelectField } from "./Field";

interface Props { rm: RelayMath; settings: Settings; onChange: (s: Settings) => void; violations: Violation[] }

const W = 560, H = 360, PL = 56, PR = 16, PT = 12, PB = 40;
const MULT_MIN = 1.05, MULT_MAX = 30, T_MIN = 0.01, T_MAX = 100;
const lx = (m: number) => PL + ((Math.log10(m) - Math.log10(MULT_MIN)) / (Math.log10(MULT_MAX) - Math.log10(MULT_MIN))) * (W - PL - PR);
const ly = (t: number) => PT + (1 - (Math.log10(t) - Math.log10(T_MIN)) / (Math.log10(T_MAX) - Math.log10(T_MIN))) * (H - PT - PB);

function curvePath(rm: RelayMath, curve: number, tms: number, definiteS: number): string {
  const pts: string[] = [];
  for (let k = 0; k <= 120; k++) {
    const m = MULT_MIN * Math.pow(MULT_MAX / MULT_MIN, k / 120);
    const t = rm.operateTime(curve, tms, definiteS, m);
    if (t <= 0 || !Number.isFinite(t)) continue;
    const tc = Math.min(T_MAX, Math.max(T_MIN, t));
    pts.push(`${pts.length ? "L" : "M"}${lx(m).toFixed(1)},${ly(tc).toFixed(1)}`);
  }
  return pts.join(" ");
}

export default function OvercurrentView({ rm, settings: s, onChange, violations }: Props) {
  const set = (patch: Partial<Settings>) => onChange({ ...s, ...patch });
  const pCurve = rm.curveFromName(s.t51pCurve);
  const gCurve = rm.curveFromName(s.t51gCurve);
  const pathP = useMemo(() => curvePath(rm, pCurve, s.t51pTms, s.t51pDefiniteS), [rm, pCurve, s.t51pTms, s.t51pDefiniteS]);
  const pathG = useMemo(() => curvePath(rm, gCurve, s.t51gTms, s.t51gDefiniteS), [rm, gCurve, s.t51gTms, s.t51gDefiniteS]);
  const probe = [2, 3, 5, 10].map((m) => ({ m, p: rm.operateTime(pCurve, s.t51pTms, s.t51pDefiniteS, m), g: rm.operateTime(gCurve, s.t51gTms, s.t51gDefiniteS, m) }));
  const xticks = [1.05, 1.5, 2, 3, 5, 10, 20, 30];
  const yticks = [0.01, 0.1, 1, 10, 100];
  return (
    <>
      <h2>50/51 Overcurrent</h2>
      <p className="lead">Inverse-time curves per IEC 60255-151 and IEEE C37.112 computed by the shared relay math (the same C the firmware runs). Pickup in primary amperes of bay 1.</p>
      <div className="grid2">
        <div>
          <div className="panel">
            <h3>51P phase time-overcurrent</h3>
            <div className="form" data-testid="form-51p">
              <NumField id="t51pPickupA" label="Pickup" unit="A" value={s.t51pPickupA} violations={violations} onChange={(v) => set({ t51pPickupA: v })} />
              <SelectField id="t51pCurve" label="Curve" value={s.t51pCurve} options={rm.curveNames} violations={violations} onChange={(v) => set({ t51pCurve: v })} />
              <NumField id="t51pTms" label="Time multiplier" unit="TMS" step={0.01} value={s.t51pTms} violations={violations} onChange={(v) => set({ t51pTms: v })} />
              <NumField id="t51pDefiniteS" label="Definite time" unit="s" step={0.01} value={s.t51pDefiniteS} violations={violations} onChange={(v) => set({ t51pDefiniteS: v })} />
            </div>
          </div>
          <div className="panel">
            <h3>51G ground time-overcurrent</h3>
            <div className="form" data-testid="form-51g">
              <NumField id="t51gPickupA" label="Pickup" unit="A" value={s.t51gPickupA} violations={violations} onChange={(v) => set({ t51gPickupA: v })} />
              <SelectField id="t51gCurve" label="Curve" value={s.t51gCurve} options={rm.curveNames} violations={violations} onChange={(v) => set({ t51gCurve: v })} />
              <NumField id="t51gTms" label="Time multiplier" unit="TMS" step={0.01} value={s.t51gTms} violations={violations} onChange={(v) => set({ t51gTms: v })} />
              <NumField id="t51gDefiniteS" label="Definite time" unit="s" step={0.01} value={s.t51gDefiniteS} violations={violations} onChange={(v) => set({ t51gDefiniteS: v })} />
            </div>
          </div>
          <div className="panel">
            <h3>50P / 50G instantaneous</h3>
            <div className="form" data-testid="form-50">
              <NumField id="i50pPickupA" label="50P pickup" unit="A" value={s.i50pPickupA} violations={violations} onChange={(v) => set({ i50pPickupA: v })} />
              <NumField id="i50pSecurityPasses" label="50P security count" unit="passes" step={1} value={s.i50pSecurityPasses} violations={violations} onChange={(v) => set({ i50pSecurityPasses: v })} />
              <NumField id="i50gPickupA" label="50G pickup" unit="A" value={s.i50gPickupA} violations={violations} onChange={(v) => set({ i50gPickupA: v })} />
              <NumField id="i50gSecurityPasses" label="50G security count" unit="passes" step={1} value={s.i50gSecurityPasses} violations={violations} onChange={(v) => set({ i50gSecurityPasses: v })} />
            </div>
          </div>
        </div>
        <div>
          <div className="panel">
            <h3>Operate-time characteristic</h3>
            <div className="curve-wrap">
              <svg viewBox={`0 0 ${W} ${H}`} role="img" aria-label="51 operate time versus multiple of pickup" data-testid="curve-plot">
                {xticks.map((m) => <g key={m}><line x1={lx(m)} x2={lx(m)} y1={PT} y2={H - PB} stroke="#e3e6ea" /><text x={lx(m)} y={H - PB + 14} fontSize="10" textAnchor="middle" fill="#5d6673">{m}</text></g>)}
                {yticks.map((t) => <g key={t}><line x1={PL} x2={W - PR} y1={ly(t)} y2={ly(t)} stroke="#e3e6ea" /><text x={PL - 6} y={ly(t) + 3} fontSize="10" textAnchor="end" fill="#5d6673">{t}</text></g>)}
                <text x={(W + PL) / 2} y={H - 6} fontSize="11" textAnchor="middle" fill="#5d6673">Multiple of pickup (I / Ipickup)</text>
                <text x={12} y={(H - PB) / 2} fontSize="11" textAnchor="middle" fill="#5d6673" transform={`rotate(-90 12 ${(H - PB) / 2})`}>Operate time (s)</text>
                <path d={pathP} fill="none" stroke="#0b5cad" strokeWidth="2" data-testid="curve-51p" />
                <path d={pathG} fill="none" stroke="#b4530a" strokeWidth="2" strokeDasharray="6 3" data-testid="curve-51g" />
              </svg>
              <div className="legend"><span style={{ color: "#0b5cad" }}>51P {s.t51pCurve} TMS {s.t51pTms}</span><span style={{ color: "#b4530a" }}>51G {s.t51gCurve} TMS {s.t51gTms}</span></div>
            </div>
            <table style={{ marginTop: 10 }} data-testid="curve-probe">
              <thead><tr><th>Multiple</th><th>51P operate (s)</th><th>51G operate (s)</th></tr></thead>
              <tbody>{probe.map((r) => <tr key={r.m}><td className="mono">{r.m}x</td><td className="mono" data-testid={`t51p-at-${r.m}`}>{r.p > 0 ? r.p.toFixed(3) : "-"}</td><td className="mono">{r.g > 0 ? r.g.toFixed(3) : "-"}</td></tr>)}</tbody>
            </table>
          </div>
        </div>
      </div>
    </>
  );
}
