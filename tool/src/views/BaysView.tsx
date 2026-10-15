import { useState } from "react";
import type { RelayMath, Violation } from "../lib/wasm";
import type { Bay, Settings } from "../lib/settings";

interface Props { rm: RelayMath; settings: Settings; onChange: (s: Settings) => void; violations: Violation[] }

export default function BaysView({ rm, settings, onChange, violations }: Props) {
  const [selected, setSelected] = useState<number | null>(null);
  const update = (k: number, patch: Partial<Bay>) => {
    const bays = settings.bays.map((b, i) => (i === k ? { ...b, ...patch } : b));
    onChange({ ...settings, bays });
  };
  const bayErrors = (k: number) => violations.filter((v) => (v.id === "R-CT-001" || v.id === "R-CT-002") && v.detail === k);
  const vtErrors = violations.filter((v) => v.id === "R-VT-001");
  const countsPerRated = 1200;
  return (
    <>
      <h2>Bays and CT inputs</h2>
      <p className="lead">Four-bay bus section. Each bay has its own CT ratio and polarity; all 16 channels are sampled on one 1920 Hz clock (full-cycle DFT, 480 Hz protection pass).</p>
      <div className="panel">
        <h3>Measurement chain</h3>
        <table data-testid="bay-table">
          <thead><tr><th>Bay</th><th>Name</th><th>Role</th><th>CT primary (A)</th><th>CT secondary (A)</th><th>Ratio</th><th>Polarity</th><th>CT connected</th><th>A per ADC count</th><th>Status</th></tr></thead>
          <tbody>
            {settings.bays.map((b, k) => {
              const errs = bayErrors(k);
              return (
                <tr key={k} className="clickable" data-testid={`bay-row-${k + 1}`} onClick={() => setSelected(k)}>
                  <td className="mono">{k + 1}</td>
                  <td>{b.name}</td>
                  <td>{b.role}</td>
                  <td className="mono">{b.ctPrimaryA}</td>
                  <td className="mono">{b.ctSecondaryA}</td>
                  <td className="mono">{b.ctSecondaryA > 0 ? rm.ctRatio(b.ctPrimaryA, b.ctSecondaryA).toFixed(0) : "-"}:1</td>
                  <td>{b.polarity === "BUS" ? "toward bus" : "toward line"}</td>
                  <td>{b.ctConnected ? "yes" : <span className="pill warn">no source</span>}</td>
                  <td className="mono">{rm.aPerCount(b.ctPrimaryA, countsPerRated).toFixed(4)}</td>
                  <td>{errs.length ? <span className="pill err">{errs.map((e) => e.id).join(", ")}</span> : <span className="pill ok">valid</span>}</td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
      <div className="grid2">
        <div className="panel">
          <h3>Bay detail {selected !== null ? `- bay ${selected + 1}` : ""}</h3>
          {selected === null ? <p className="lead">Select a bay row to edit its CT inputs.</p> : (
            <div className="form" data-testid="bay-form">
              <label htmlFor="bay-name">Name</label>
              <input id="bay-name" value={settings.bays[selected].name} onChange={(e) => update(selected, { name: e.target.value })} /><span />
              <label htmlFor="bay-role">Role</label>
              <select id="bay-role" value={settings.bays[selected].role} onChange={(e) => update(selected, { role: e.target.value as Bay["role"] })}>
                {["FEEDER", "INFEED", "TIE", "SPARE"].map((r) => <option key={r}>{r}</option>)}
              </select><span />
              <label htmlFor="bay-ct-primary">CT primary</label>
              <input id="bay-ct-primary" type="number" className={bayErrors(selected).some((e) => e.id === "R-CT-001") ? "invalid" : ""} value={settings.bays[selected].ctPrimaryA} onChange={(e) => update(selected, { ctPrimaryA: Number(e.target.value) })} /><span className="unit">A</span>
              <label htmlFor="bay-ct-secondary">CT secondary</label>
              <input id="bay-ct-secondary" type="number" className={bayErrors(selected).some((e) => e.id === "R-CT-002") ? "invalid" : ""} value={settings.bays[selected].ctSecondaryA} onChange={(e) => update(selected, { ctSecondaryA: Number(e.target.value) })} /><span className="unit">A</span>
              <label htmlFor="bay-polarity">Polarity</label>
              <select id="bay-polarity" value={settings.bays[selected].polarity} onChange={(e) => update(selected, { polarity: e.target.value as Bay["polarity"] })}>
                <option value="BUS">BUS (current into bus positive)</option>
                <option value="LINE">LINE (current out of bus positive)</option>
              </select><span />
              <label htmlFor="bay-connected">CT connected</label>
              <select id="bay-connected" value={settings.bays[selected].ctConnected ? "YES" : "NO"} onChange={(e) => update(selected, { ctConnected: e.target.value === "YES" })}>
                <option>YES</option><option>NO</option>
              </select><span />
              {bayErrors(selected).map((e) => <div key={e.id} className="field-error" data-testid={`error-${e.id}`}><code>{e.id}</code> {e.message}</div>)}
            </div>
          )}
        </div>
        <div className="panel">
          <h3>Voltage inputs and source</h3>
          <div className="form">
            <label htmlFor="vtVLnNominal">VT nominal (L-N)</label>
            <input id="vtVLnNominal" type="number" className={vtErrors.length ? "invalid" : ""} value={settings.vtVLnNominal} onChange={(e) => onChange({ ...settings, vtVLnNominal: Number(e.target.value) })} /><span className="unit">V</span>
            {vtErrors.map((e) => <div key={e.id} className="field-error" data-testid={`error-${e.id}`}><code>{e.id}</code> {e.message}</div>)}
            <label htmlFor="sourceXOverR">Source X/R</label>
            <input id="sourceXOverR" type="number" value={settings.sourceXOverR} onChange={(e) => onChange({ ...settings, sourceXOverR: Number(e.target.value) })} /><span />
          </div>
          <dl className="kv" style={{ marginTop: 12 }}>
            <dt>Base current (20 MVA at 12.47 kV)</dt><dd>{rm.baseAFromMva(20, 12.47).toFixed(1)} A</dd>
            <dt>Sample rate</dt><dd>1920 Hz (32 samples/cycle at 60 Hz)</dd>
            <dt>Protection pass</dt><dd>480 Hz (every 4th sample)</dd>
            <dt>Phasor estimator</dt><dd>Full-cycle DFT, mimic-compensated currents</dd>
          </dl>
        </div>
      </div>
    </>
  );
}
