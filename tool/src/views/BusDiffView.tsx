import type { RelayMath, Violation } from "../lib/wasm";
import type { Settings } from "../lib/settings";
import { NumField, SelectField, fieldErrors } from "./Field";

interface Props { rm: RelayMath; settings: Settings; onChange: (s: Settings) => void; violations: Violation[] }

/* Dual-slope operate threshold, continuous at the breakpoint (DESIGN_SPEC §4, assumption A2). */
function threshold(s: Settings, ir: number): number {
  const s1 = s.b87Slope1Pct / 100;
  const s2 = s.b87Slope2Pct / 100;
  const bp = s.b87BreakpointPu;
  return ir <= bp ? s1 * ir : s1 * bp + s2 * (ir - bp);
}

export default function BusDiffView({ rm, settings: s, onChange, violations }: Props) {
  const set = (patch: Partial<Settings>) => onChange({ ...s, ...patch });
  const kvLl = (s.vtVLnNominal * Math.sqrt(3)) / 1000;
  const baseA = rm.baseAFromMva(s.b87BaseMva, kvLl);
  const inZone = s.b87BayInZone.filter(Boolean).length;
  const zoneErrs = fieldErrors("b87BayInZone", violations);

  const irMax = 10;
  const w = 420, h = 260, pad = 36;
  const x = (ir: number) => pad + (ir / irMax) * (w - 2 * pad);
  const y = (id: number) => h - pad - (id / 6) * (h - 2 * pad);
  const pts: string[] = [];
  for (let ir = 0; ir <= irMax; ir += 0.1) pts.push(`${x(ir).toFixed(1)},${y(Math.max(threshold(s, ir), s.b87PickupPu)).toFixed(1)}`);
  const path = `M${pts.join(" L")}`;
  const bp = s.b87BreakpointPu;

  return (
    <>
      <h2>87B Bus differential</h2>
      <p className="lead">
        Low-impedance percentage-restrained differential over the in-zone bays. Operates when the differential current exceeds the pickup
        and the dual-slope characteristic; slope 2 must not be below slope 1 (R-87B-005). All per-unit values use the base below.
        Thirteen requirement rows are open with the requirement owner (docs/elements/87B/REQUIREMENTS.md §2).
      </p>
      <div className="grid2">
        <div className="panel">
          <h3>Element</h3>
          <div className="form" data-testid="form-87b">
            <SelectField id="b87Function" label="Function" value={s.b87Function} options={["ENABLED", "DISABLED"]} violations={violations} onChange={(v) => set({ b87Function: v as Settings["b87Function"] })} />
            <NumField id="b87BaseMva" label="Base" unit="MVA" step={0.1} value={s.b87BaseMva} violations={violations} onChange={(v) => set({ b87BaseMva: v })} />
            <NumField id="b87PickupPu" label="Pickup" unit="pu" step={0.01} value={s.b87PickupPu} violations={violations} onChange={(v) => set({ b87PickupPu: v })} />
            <NumField id="b87Slope1Pct" label="Slope 1" unit="%" step={1} value={s.b87Slope1Pct} violations={violations} onChange={(v) => set({ b87Slope1Pct: v })} />
            <NumField id="b87Slope2Pct" label="Slope 2" unit="%" step={1} value={s.b87Slope2Pct} violations={violations} onChange={(v) => set({ b87Slope2Pct: v })} />
            <NumField id="b87BreakpointPu" label="Breakpoint" unit="pu" step={0.1} value={s.b87BreakpointPu} violations={violations} onChange={(v) => set({ b87BreakpointPu: v })} />
          </div>
          <h3>CT supervision</h3>
          <div className="form" data-testid="form-87b-cts">
            <NumField id="b87CtsAlarmPct" label="Alarm level" unit="% of restraint" step={1} value={s.b87CtsAlarmPct} violations={violations} onChange={(v) => set({ b87CtsAlarmPct: v })} />
            <NumField id="b87CtsDelayS" label="Alarm delay" unit="s" step={0.1} value={s.b87CtsDelayS} violations={violations} onChange={(v) => set({ b87CtsDelayS: v })} />
          </div>
          <h3>Zone</h3>
          <div className="form" data-testid="form-87b-zone">
            {s.bays.map((b, k) => (
              <div key={k} className="check-row" style={{ gridColumn: "1 / -1" }}>
                <label htmlFor={`b87BayInZone${k + 1}`}>
                  <input
                    id={`b87BayInZone${k + 1}`}
                    type="checkbox"
                    checked={s.b87BayInZone[k]}
                    onChange={(e) => {
                      const z = [...s.b87BayInZone];
                      z[k] = e.target.checked;
                      set({ b87BayInZone: z });
                    }}
                  />{" "}
                  Bay {k + 1} {b.name} in zone
                  {!b.ctConnected && <span className="muted"> (CT not connected: element blocked while this bay is in zone)</span>}
                </label>
              </div>
            ))}
            {zoneErrs.map((e) => <div key={e.id} className="field-error" data-testid={`error-${e.id}`} style={{ gridColumn: "1 / -1" }}><code>{e.id}</code> {e.message}</div>)}
          </div>
        </div>
        <div className="panel">
          <h3>Characteristic</h3>
          <svg width={w} height={h} role="img" aria-label="87B operate characteristic" data-testid="char-87b">
            <rect x={pad} y={pad} width={w - 2 * pad} height={h - 2 * pad} fill="none" stroke="#999" />
            {[0, 2, 4, 6, 8, 10].map((v) => <text key={v} x={x(v)} y={h - pad + 14} fontSize="10" textAnchor="middle">{v}</text>)}
            {[0, 2, 4, 6].map((v) => <text key={v} x={pad - 6} y={y(v) + 3} fontSize="10" textAnchor="end">{v}</text>)}
            <text x={w / 2} y={h - 6} fontSize="10" textAnchor="middle">Restraint Ir (pu)</text>
            <text x={10} y={h / 2} fontSize="10" textAnchor="middle" transform={`rotate(-90 10 ${h / 2})`}>Differential Id (pu)</text>
            <line x1={x(bp)} y1={pad} x2={x(bp)} y2={h - pad} stroke="#bbb" strokeDasharray="3 3" />
            <path d={path} fill="none" stroke="#c0392b" strokeWidth={2} data-testid="char-87b-path" />
            <text x={x(bp) + 3} y={pad + 10} fontSize="10">breakpoint</text>
          </svg>
          <dl className="kv">
            <dt>Base current</dt><dd data-testid="b87-base-a">{baseA.toFixed(0)} A primary ({s.b87BaseMva} MVA at {kvLl.toFixed(2)} kV)</dd>
            <dt>Pickup</dt><dd data-testid="b87-pickup-a">{(s.b87PickupPu * baseA).toFixed(0)} A primary</dd>
            <dt>Threshold at 1 / 2 / 4 pu restraint</dt>
            <dd data-testid="b87-thresholds">{threshold(s, 1).toFixed(2)} / {threshold(s, 2).toFixed(2)} / {threshold(s, 4).toFixed(2)} pu</dd>
            <dt>Bays in zone</dt><dd data-testid="b87-in-zone">{inZone} of {s.bays.length}</dd>
            <dt>Outputs</dt><dd>TRIP1 and 86B bus lockout; SOE OPERATE 87B; CT supervision alarm blocks the element</dd>
          </dl>
        </div>
      </div>
    </>
  );
}
