import { useState } from "react";
import type { Violation } from "../lib/wasm";
import { toSettingsFile, type Settings } from "../lib/settings";

interface Props { settings: Settings; violations: Violation[]; onClose: () => void; onWritten: () => void }

export default function WriteDrawer({ settings, violations, onClose, onWritten }: Props) {
  const text = toSettingsFile(settings);
  const [confirmed, setConfirmed] = useState(false);
  const blocked = violations.length > 0;
  const download = () => {
    const blob = new Blob([text], { type: "text/plain" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = "relay.set";
    a.click();
    URL.revokeObjectURL(a.href);
  };
  return (
    <div className="drawer-backdrop" onClick={onClose} data-testid="write-drawer">
      <aside className="drawer" role="dialog" aria-modal="true" aria-labelledby="write-title" onClick={(e) => e.stopPropagation()}>
        <h2 id="write-title">Write settings group to relay</h2>
        <p className="lead">The relay re-runs the same rules on receipt (firmware/src/ied/settings_file.c) and rejects the whole group on any violation; nothing is applied partially.</p>
        {blocked ? (
          <div className="detail" style={{ borderColor: "var(--err)" }} data-testid="write-blocked">
            <strong>Write blocked: {violations.length} rule violation{violations.length > 1 ? "s" : ""}</strong>
            <ul className="violations">
              {violations.map((v, k) => <li key={k}><code>{v.id}</code> {v.message}{v.detail ? ` (bay ${v.detail + 1})` : ""} — value {Number.isFinite(v.value) ? v.value : "?"}</li>)}
            </ul>
          </div>
        ) : (
          <div className="detail" style={{ borderColor: "var(--ok)" }} data-testid="write-ready">
            <strong>All rules satisfied.</strong> {text.split("\n").length - 2} settings will be written.
          </div>
        )}
        <h3 style={{ marginBottom: 6 }}>relay.set</h3>
        <pre data-testid="settings-file">{text}</pre>
        <label style={{ display: "flex", gap: 8, alignItems: "center" }}>
          <input type="checkbox" data-testid="write-confirm" checked={confirmed} onChange={(e) => setConfirmed(e.target.checked)} disabled={blocked} />
          I have checked the group against the approved settings sheet
        </label>
        <div className="actions">
          <button className="primary" data-testid="write-apply" disabled={blocked || !confirmed} onClick={onWritten}>Write group 1</button>
          <button onClick={download} data-testid="write-download">Download relay.set</button>
          <button onClick={onClose} data-testid="write-cancel">Cancel</button>
        </div>
      </aside>
    </div>
  );
}
