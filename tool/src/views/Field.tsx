import type { Violation } from "../lib/wasm";
import { RULE_FIELDS } from "../lib/settings";

export function fieldErrors(field: string, violations: Violation[]): Violation[] {
  return violations.filter((v) => (v.fields ?? RULE_FIELDS[v.id] ?? []).includes(field));
}

interface NumFieldProps {
  id: string;
  label: string;
  unit: string;
  value: number;
  step?: number;
  violations: Violation[];
  onChange: (v: number) => void;
}

export function NumField({ id, label, unit, value, step, violations, onChange }: NumFieldProps) {
  const errs = fieldErrors(id, violations);
  return (
    <>
      <label htmlFor={id}>{label}</label>
      <input id={id} name={id} type="number" step={step ?? "any"} value={value} className={errs.length ? "invalid" : ""}
        aria-invalid={errs.length > 0} onChange={(e) => onChange(Number(e.target.value))} />
      <span className="unit">{unit}</span>
      {errs.map((e) => <div key={e.id} className="field-error" data-testid={`error-${e.id}`}><code>{e.id}</code> {e.message}</div>)}
    </>
  );
}

interface SelectFieldProps {
  id: string;
  label: string;
  value: string;
  options: string[];
  violations: Violation[];
  onChange: (v: string) => void;
}

export function SelectField({ id, label, value, options, violations, onChange }: SelectFieldProps) {
  const errs = fieldErrors(id, violations);
  return (
    <>
      <label htmlFor={id}>{label}</label>
      <select id={id} name={id} value={value} className={errs.length ? "invalid" : ""} onChange={(e) => onChange(e.target.value)}>
        {options.map((o) => <option key={o} value={o}>{o}</option>)}
      </select>
      <span className="unit" />
      {errs.map((e) => <div key={e.id} className="field-error" data-testid={`error-${e.id}`}><code>{e.id}</code> {e.message}</div>)}
    </>
  );
}
