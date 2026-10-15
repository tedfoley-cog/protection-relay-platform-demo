import { useEffect, useMemo, useState } from "react";
import { loadRelayMath, type RelayMath, type Violation } from "./lib/wasm";
import { DEFAULT_SETTINGS, SECTION_OF_RULE, checkSettings, type Settings } from "./lib/settings";
import BaysView from "./views/BaysView";
import OvercurrentView from "./views/OvercurrentView";
import ElementsView from "./views/ElementsView";
import RealtimeView from "./views/RealtimeView";
import WriteDrawer from "./views/WriteDrawer";

export type Route = "bays" | "overcurrent" | "distance" | "breaker-failure" | "frequency" | "realtime";
const ROUTES: Array<{ id: Route; label: string; group: string }> = [
  { id: "bays", label: "Bays and CT inputs", group: "Device setup" },
  { id: "overcurrent", label: "50/51 Overcurrent", group: "Protection elements" },
  { id: "distance", label: "21 Distance", group: "Protection elements" },
  { id: "breaker-failure", label: "50BF Breaker failure", group: "Protection elements" },
  { id: "frequency", label: "81 Frequency", group: "Protection elements" },
  { id: "realtime", label: "Real-time evidence", group: "Records" },
];

const STORAGE_KEY = "relay-setting-tool.group";
const WRITTEN_KEY = `${STORAGE_KEY}.written`;

function readRoute(): Route {
  const h = window.location.hash.replace(/^#\/?/, "") as Route;
  return ROUTES.some((r) => r.id === h) ? h : "bays";
}

export default function App() {
  const [rm, setRm] = useState<RelayMath | null>(null);
  const [route, setRoute] = useState<Route>(readRoute);
  const [settings, setSettings] = useState<Settings>(() => {
    const saved = window.localStorage.getItem(STORAGE_KEY);
    return saved ? (JSON.parse(saved) as Settings) : DEFAULT_SETTINGS;
  });
  const [written, setWrittenState] = useState<Settings>(() => {
    const saved = window.localStorage.getItem(WRITTEN_KEY);
    return saved ? (JSON.parse(saved) as Settings) : DEFAULT_SETTINGS;
  });
  const setWritten = (s: Settings) => {
    window.localStorage.setItem(WRITTEN_KEY, JSON.stringify(s));
    setWrittenState(s);
  };
  const [drawer, setDrawer] = useState(false);
  const [lastWrite, setLastWrite] = useState<string | null>(null);

  useEffect(() => {
    loadRelayMath().then(setRm);
    const onHash = () => setRoute(readRoute());
    window.addEventListener("hashchange", onHash);
    return () => window.removeEventListener("hashchange", onHash);
  }, []);
  useEffect(() => {
    window.localStorage.setItem(STORAGE_KEY, JSON.stringify(settings));
  }, [settings]);

  const violations: Violation[] = useMemo(() => (rm ? checkSettings(settings, rm) : []), [rm, settings]);
  const bySection = useMemo(() => {
    const m: Record<string, Violation[]> = {};
    for (const v of violations) (m[SECTION_OF_RULE(v.id)] ||= []).push(v);
    return m;
  }, [violations]);
  const modified = JSON.stringify(settings) !== JSON.stringify(written);

  const nav = (r: Route) => {
    window.location.hash = `/${r}`;
    setRoute(r);
  };

  if (!rm) return <div className="app"><div className="toolbar"><span className="title">Relay Setting Tool</span><span>Loading shared relay math (WASM)...</span></div></div>;

  const groups = Array.from(new Set(ROUTES.map((r) => r.group)));
  return (
    <div className="app">
      <div className="toolbar">
        <span className="title">Relay Setting Tool</span>
        <span className="device" data-testid="device-id">BUS1 FDR1 RELAY · firmware 1.4 · group 1</span>
        <button onClick={() => setSettings(DEFAULT_SETTINGS)}>Load defaults</button>
        <button onClick={() => setSettings(written)} disabled={!modified}>Discard changes</button>
        <button className="primary" data-testid="write-button" onClick={() => setDrawer(true)}>Write to relay...</button>
      </div>
      <div className="body">
        <nav className="tree" aria-label="Device tree">
          {groups.map((g) => (
            <div key={g}>
              <h4>{g}</h4>
              {ROUTES.filter((r) => r.group === g).map((r) => (
                <a key={r.id} href={`#/${r.id}`} className={r.id === route ? "active" : ""} onClick={(e) => { e.preventDefault(); nav(r.id); }} data-testid={`nav-${r.id}`}>
                  <span>{r.label}</span>
                  {bySection[r.id]?.length ? <span className="badge" title="rule violations">{bySection[r.id].length}</span> : null}
                </a>
              ))}
            </div>
          ))}
        </nav>
        <main className="main">
          {route === "bays" && <BaysView rm={rm} settings={settings} onChange={setSettings} violations={bySection.bays ?? []} />}
          {route === "overcurrent" && <OvercurrentView rm={rm} settings={settings} onChange={setSettings} violations={bySection.overcurrent ?? []} />}
          {(route === "distance" || route === "breaker-failure" || route === "frequency") && (
            <ElementsView section={route} settings={settings} onChange={setSettings} violations={bySection[route] ?? []} />
          )}
          {route === "realtime" && <RealtimeView />}
        </main>
      </div>
      <div className="statusbar">
        <span data-testid="status-rules">
          {violations.length === 0 ? <span className="ok">Rules: all {rm.rulesInputNames.length} inputs valid</span> : <span className="err">Rules: {violations.length} violation{violations.length > 1 ? "s" : ""}</span>}
        </span>
        <span data-testid="status-modified">{modified ? "Modified - not written" : "In sync with relay"}</span>
        {lastWrite && <span data-testid="status-last-write">Last write: {lastWrite}</span>}
        <span style={{ marginLeft: "auto" }}>Shared math: relay_math.wasm · {rm.curveNames.length} curves</span>
      </div>
      {drawer && (
        <WriteDrawer
          settings={settings}
          violations={violations}
          onClose={() => setDrawer(false)}
          onWritten={() => {
            setWritten(settings);
            setLastWrite(new Date().toLocaleTimeString());
            setDrawer(false);
          }}
        />
      )}
    </div>
  );
}
