import type { Violation } from "../lib/wasm";
import type { Settings } from "../lib/settings";
import { NumField } from "./Field";

interface Props { section: "distance" | "breaker-failure" | "frequency"; settings: Settings; onChange: (s: Settings) => void; violations: Violation[] }

export default function ElementsView({ section, settings: s, onChange, violations }: Props) {
  const set = (patch: Partial<Settings>) => onChange({ ...s, ...patch });
  if (section === "distance") {
    const zmag = Math.hypot(s.z1LineROhm, s.z1LineXOhm);
    return (
      <>
        <h2>21 Distance</h2>
        <p className="lead">Mho phase and ground loops on bay 1. Zone 1 is instantaneous and under-reaching; zone 2 over-reaches with a coordination delay. Zone 1 reach must stay below zone 2 reach (R-21-003).</p>
        <div className="grid2">
          <div className="panel">
            <h3>Line and zones</h3>
            <div className="form" data-testid="form-21">
              <NumField id="z1LineROhm" label="Line R (positive seq.)" unit="ohm" step={0.01} value={s.z1LineROhm} violations={violations} onChange={(v) => set({ z1LineROhm: v })} />
              <NumField id="z1LineXOhm" label="Line X (positive seq.)" unit="ohm" step={0.01} value={s.z1LineXOhm} violations={violations} onChange={(v) => set({ z1LineXOhm: v })} />
              <NumField id="z1ReachPct" label="Zone 1 reach" unit="% line" step={1} value={s.z1ReachPct} violations={violations} onChange={(v) => set({ z1ReachPct: v })} />
              <NumField id="z2ReachPct" label="Zone 2 reach" unit="% line" step={1} value={s.z2ReachPct} violations={violations} onChange={(v) => set({ z2ReachPct: v })} />
              <NumField id="z2DelayS" label="Zone 2 delay" unit="s" step={0.01} value={s.z2DelayS} violations={violations} onChange={(v) => set({ z2DelayS: v })} />
              <NumField id="minLoopA" label="Minimum loop current" unit="A" value={s.minLoopA} violations={violations} onChange={(v) => set({ minLoopA: v })} />
              <NumField id="minResidualA" label="Minimum residual (ground loops)" unit="A" value={s.minResidualA} violations={violations} onChange={(v) => set({ minResidualA: v })} />
            </div>
          </div>
          <div className="panel">
            <h3>Derived reach</h3>
            <dl className="kv">
              <dt>Line impedance</dt><dd>{zmag.toFixed(2)} ohm at {(Math.atan2(s.z1LineXOhm, s.z1LineROhm) * 180 / Math.PI).toFixed(1)} deg</dd>
              <dt>Zone 1 reach</dt><dd data-testid="z1-ohm">{(zmag * s.z1ReachPct / 100).toFixed(2)} ohm</dd>
              <dt>Zone 2 reach</dt><dd data-testid="z2-ohm">{(zmag * s.z2ReachPct / 100).toFixed(2)} ohm</dd>
              <dt>Zone 2 delay</dt><dd>{Math.round(s.z2DelayS * 1920)} samples ({s.z2DelayS} s)</dd>
            </dl>
          </div>
        </div>
      </>
    );
  }
  if (section === "breaker-failure") {
    return (
      <>
        <h2>50BF Breaker failure</h2>
        <p className="lead">Current-supervised breaker-failure scheme on bay 1: re-trip, then bus trip (86B) if current persists past the failure delay. Re-trip must come before the bus trip (R-BF-002).</p>
        <div className="panel">
          <h3>Timers</h3>
          <div className="form" data-testid="form-50bf">
            <NumField id="bfPickupA" label="Current detector pickup" unit="A" value={s.bfPickupA} violations={violations} onChange={(v) => set({ bfPickupA: v })} />
            <NumField id="bfRetripS" label="Re-trip delay" unit="s" step={0.005} value={s.bfRetripS} violations={violations} onChange={(v) => set({ bfRetripS: v })} />
            <NumField id="bfDelayS" label="Breaker-failure delay (86B)" unit="s" step={0.005} value={s.bfDelayS} violations={violations} onChange={(v) => set({ bfDelayS: v })} />
          </div>
        </div>
      </>
    );
  }
  return (
    <>
      <h2>81 Frequency</h2>
      <p className="lead">Under/over-frequency and rate-of-change elements on the bus VTs, with an undervoltage inhibit. 81U must be below nominal and 81O above it (R-81-001).</p>
      <div className="panel">
        <h3>Thresholds</h3>
        <div className="form" data-testid="form-81">
          <NumField id="f81uPickupHz" label="81U under-frequency pickup" unit="Hz" step={0.01} value={s.f81uPickupHz} violations={violations} onChange={(v) => set({ f81uPickupHz: v })} />
          <NumField id="f81oPickupHz" label="81O over-frequency pickup" unit="Hz" step={0.01} value={s.f81oPickupHz} violations={violations} onChange={(v) => set({ f81oPickupHz: v })} />
          <NumField id="f81rPickupHzS" label="81R rate-of-change pickup" unit="Hz/s" step={0.1} value={s.f81rPickupHzS} violations={violations} onChange={(v) => set({ f81rPickupHzS: v })} />
          <NumField id="f81UvInhibitPu" label="Undervoltage inhibit" unit="pu" step={0.05} value={s.f81UvInhibitPu} violations={violations} onChange={(v) => set({ f81UvInhibitPu: v })} />
        </div>
      </div>
    </>
  );
}
