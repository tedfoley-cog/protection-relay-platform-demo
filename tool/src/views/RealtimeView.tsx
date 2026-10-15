import { useEffect, useMemo, useState } from "react";

interface TaskRow { task: string; rate_hz: number; budget_cycles: number; max_cycles: number; mean_cycles: number; max_us: number; headroom_pct: number; worst_scenario: string; status: string }
interface ScenarioRow { name: string; description: string; expected: string[]; observed: string[]; bus_trip: boolean; trip_ms: number | null; util_pct: number; pass: boolean }
interface ComplianceRow { id: string; test: string; clause: string; limit: string; automated: boolean; status: string; points: number; worst_tve_pct: number | null }
interface Report { generated_at: string; label: string; target: Record<string, unknown>; rates: Record<string, number>; worst_util_pct: number; tasks: TaskRow[]; scenarios: ScenarioRow[]; compliance: ComplianceRow[] }

type SortKey = keyof ScenarioRow;

export default function RealtimeView() {
  const [report, setReport] = useState<Report | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [query, setQuery] = useState("");
  const [outcome, setOutcome] = useState<"all" | "trip" | "no-trip" | "fail">("all");
  const [sort, setSort] = useState<{ key: SortKey; asc: boolean }>({ key: "name", asc: true });
  const [selected, setSelected] = useState<ScenarioRow | null>(null);

  useEffect(() => {
    fetch("/realtime.json").then((r) => (r.ok ? r.json() : Promise.reject(new Error(`HTTP ${r.status}`))))
      .then(setReport).catch((e: Error) => setError(e.message));
  }, []);

  const rows = useMemo(() => {
    if (!report) return [];
    const q = query.trim().toLowerCase();
    let out = report.scenarios.filter((s) => {
      if (q && !`${s.name} ${s.description} ${s.expected.join(" ")} ${s.observed.join(" ")}`.toLowerCase().includes(q)) return false;
      if (outcome === "trip") return s.trip_ms !== null || s.bus_trip;
      if (outcome === "no-trip") return s.trip_ms === null && !s.bus_trip;
      if (outcome === "fail") return !s.pass;
      return true;
    });
    out = [...out].sort((a, b) => {
      const av = a[sort.key], bv = b[sort.key];
      const cmp = av === null ? 1 : bv === null ? -1 : typeof av === "number" && typeof bv === "number" ? av - bv : String(av).localeCompare(String(bv));
      return sort.asc ? cmp : -cmp;
    });
    return out;
  }, [report, query, outcome, sort]);

  const th = (key: SortKey, label: string) => (
    <th className={sort.key === key ? `sorted ${sort.asc ? "asc" : ""}` : ""} onClick={() => setSort({ key, asc: sort.key === key ? !sort.asc : true })} data-testid={`sort-${key}`}>{label}</th>
  );

  if (error) return <><h2>Real-time evidence</h2><p className="lead">Could not load /realtime.json ({error}). Run <code>python3 tools/build_realtime_report.py</code>.</p></>;
  if (!report) return <><h2>Real-time evidence</h2><p className="lead">Loading...</p></>;
  return (
    <>
      <h2>Real-time evidence</h2>
      <p className="lead">Renode run of the STM32F407 target ({report.label}, {report.generated_at}): task budgets and secondary-injection scenarios. Worst CPU utilisation {report.worst_util_pct}%.</p>
      <div className="panel">
        <h3>Task budgets</h3>
        <table data-testid="task-table">
          <thead><tr><th>Task</th><th>Rate (Hz)</th><th>Budget (cycles)</th><th>Max (cycles)</th><th>Max (us)</th><th>Headroom</th><th>Worst scenario</th><th>Status</th></tr></thead>
          <tbody>{report.tasks.map((t) => (
            <tr key={t.task}><td className="mono">{t.task}</td><td className="mono">{t.rate_hz}</td><td className="mono">{t.budget_cycles}</td><td className="mono">{t.max_cycles}</td><td className="mono">{t.max_us}</td><td className="mono">{t.headroom_pct}%</td><td>{t.worst_scenario}</td><td><span className={`pill ${t.status === "within budget" ? "ok" : "err"}`}>{t.status}</span></td></tr>
          ))}</tbody>
        </table>
      </div>
      <div className="panel">
        <h3>Scenarios</h3>
        <div className="filters">
          <input placeholder="Search scenarios, elements..." value={query} onChange={(e) => setQuery(e.target.value)} data-testid="scenario-search" aria-label="Search scenarios" />
          <select value={outcome} onChange={(e) => setOutcome(e.target.value as typeof outcome)} data-testid="scenario-filter" aria-label="Filter by outcome">
            <option value="all">All outcomes</option><option value="trip">Tripped</option><option value="no-trip">No trip</option><option value="fail">Failed expectation</option>
          </select>
          <span className="mono" data-testid="scenario-count">{rows.length} of {report.scenarios.length}</span>
        </div>
        <table data-testid="scenario-table">
          <thead><tr>{th("name", "Scenario")}{th("description", "Description")}<th>Expected</th><th>Observed</th>{th("trip_ms", "Trip (ms)")}{th("util_pct", "CPU %")}{th("pass", "Result")}</tr></thead>
          <tbody>{rows.map((s) => (
            <tr key={s.name} className="clickable" data-testid={`scenario-${s.name}`} onClick={() => setSelected(s)}>
              <td className="mono">{s.name}</td><td>{s.description}</td>
              <td>{s.expected.join(", ")}</td><td>{s.observed.join(", ")}</td>
              <td className="mono">{s.trip_ms ?? "-"}</td><td className="mono">{s.util_pct}</td>
              <td><span className={`pill ${s.pass ? "ok" : "err"}`}>{s.pass ? "as expected" : "unexpected"}</span></td>
            </tr>
          ))}</tbody>
        </table>
        {selected && (
          <div className="detail" data-testid="scenario-detail">
            <strong>{selected.name}</strong> — {selected.description}
            <dl className="kv" style={{ marginTop: 6 }}>
              <dt>Expected elements</dt><dd>{selected.expected.join(", ")}</dd>
              <dt>Observed elements</dt><dd>{selected.observed.join(", ")}</dd>
              <dt>Bus trip (86B)</dt><dd>{selected.bus_trip ? "yes" : "no"}</dd>
              <dt>Trip time after inception</dt><dd>{selected.trip_ms !== null ? `${selected.trip_ms} ms` : "no trip"}</dd>
            </dl>
          </div>
        )}
      </div>
      <div className="panel">
        <h3>IEEE C37.118.1 P-class compliance</h3>
        <table data-testid="compliance-table">
          <thead><tr><th>Test</th><th>Clause</th><th>Limit</th><th>Points</th><th>Worst TVE</th><th>Status</th></tr></thead>
          <tbody>{report.compliance.map((c) => (
            <tr key={c.id}><td>{c.test}</td><td className="mono">{c.clause}</td><td>{c.limit}</td><td className="mono">{c.points}</td><td className="mono">{c.worst_tve_pct ?? "-"}</td><td><span className={`pill ${c.status === "pass" ? "ok" : c.status === "fail" ? "err" : "muted"}`}>{c.status}</span></td></tr>
          ))}</tbody>
        </table>
      </div>
    </>
  );
}
