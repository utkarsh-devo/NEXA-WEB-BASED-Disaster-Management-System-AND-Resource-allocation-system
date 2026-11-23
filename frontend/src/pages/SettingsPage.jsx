import { useEffect, useState } from 'react';
import { Database, RefreshCw, ScrollText, Settings, SlidersHorizontal } from 'lucide-react';
import { api } from '../services/api';
import { useApi, useAction } from '../hooks/useApi';
import { useToast } from '../hooks/useToast';
import { useAuth } from '../hooks/useAuth';
import PageHeader from '../components/PageHeader';
import Panel from '../components/Panel';
import { Async } from '../components/Feedback';
import { Field, NumberInput } from '../components/Form';
import { clock } from '../services/format';

const P_KEYS = [['severity', 'Severity'], ['affected', 'People affected'], ['unmet', 'Unmet units'], ['age', 'Incident age'], ['status', 'Status'], ['timeSensitivity', 'Category time-sensitivity']];
const S_KEYS = [['capability', 'Capability match'], ['proximity', 'Proximity'], ['availability', 'Availability'], ['capacity', 'Capacity fit']];

function WeightSliders({ keys, values, onChange }) {
  const total = keys.reduce((s, [k]) => s + Number(values[k] || 0), 0);
  return (
    <div className="stack">
      {keys.map(([k, label]) => (
        <div key={k}>
          <div className="row between" style={{ fontSize: 12 }}><span>{label}</span><span className="mono">{Number(values[k]).toFixed(2)} · {total ? Math.round((100 * values[k]) / total) : 0}%</span></div>
          <input type="range" className="range" min={0} max={1} step={0.01} value={values[k]} onChange={(e) => onChange({ ...values, [k]: Number(e.target.value) })} />
        </div>
      ))}
      <div className="faint" style={{ fontSize: 11 }}>Weights are normalised by their sum (Σ = {total.toFixed(2)}).</div>
    </div>
  );
}

export default function SettingsPage() {
  const toast = useToast();
  const { can } = useAuth();
  const [busy, run] = useAction();
  const w = useApi(() => api.weights(), []);
  const health = useApi(() => api.health(), []);
  const logs = useApi(() => api.logs(60), [], { enabled: can('manage_settings') });
  const [draft, setDraft] = useState(null);
  useEffect(() => { if (w.data) setDraft(JSON.parse(JSON.stringify(w.data))); }, [w.data]);

  const save = () => run(async () => {
    try { await api.saveWeights({ priority: draft.priority, suitability: draft.suitability, escalationThreshold: draft.escalationThreshold }); toast.success('Scoring weights applied', 'All open incidents were re-prioritised'); w.reload({ silent: true }); }
    catch (e) { toast.fromError(e); }
  });
  const reload = () => run(async () => { try { const r = await api.reload(); toast.success('Engine rebuilt from MySQL', `${r.incidents} incidents, ${r.resources} resources, ${r.roads} roads in ${r.milliseconds.toFixed(0)} ms`); } catch (e) { toast.fromError(e); } });

  return (
    <>
      <PageHeader kicker="Configuration" icon={Settings} title="System Settings"
        subtitle="Tune the documented scoring formulas, rebuild in-memory indexes from the database, and review the audit log.">
        {can('manage_settings') && <button className="btn" disabled={busy} onClick={reload}><RefreshCw size={15} />Rebuild indexes from DB</button>}
        {can('manage_settings') && <button className="btn primary" disabled={busy || !draft} onClick={save}>Apply weights</button>}
      </PageHeader>
      <div className="grid g-3">
        <Panel title="Priority weights" icon={SlidersHorizontal}>
          {draft && <>
            <WeightSliders keys={P_KEYS} values={draft.priority} onChange={(p) => setDraft((d) => ({ ...d, priority: p }))} />
            <div className="form-grid" style={{ marginTop: 12 }}>
              <Field label="Age saturation (h)"><NumberInput value={draft.priority.ageSaturationHours} onChange={(v) => setDraft((d) => ({ ...d, priority: { ...d.priority, ageSaturationHours: v } }))} /></Field>
              <Field label="Affected saturation"><NumberInput value={draft.priority.affectedSaturation} onChange={(v) => setDraft((d) => ({ ...d, priority: { ...d.priority, affectedSaturation: v } }))} /></Field>
            </div>
          </>}
        </Panel>
        <Panel title="Suitability weights" icon={SlidersHorizontal}>
          {draft && <>
            <WeightSliders keys={S_KEYS} values={draft.suitability} onChange={(s) => setDraft((d) => ({ ...d, suitability: s }))} />
            <div className="form-grid" style={{ marginTop: 12 }}>
              <Field label="Proximity scale (km)"><NumberInput value={draft.suitability.proximityScaleKm} onChange={(v) => setDraft((d) => ({ ...d, suitability: { ...d.suitability, proximityScaleKm: v } }))} /></Field>
              <Field label="Escalation threshold"><NumberInput value={draft.escalationThreshold} onChange={(v) => setDraft((d) => ({ ...d, escalationThreshold: v }))} /></Field>
            </div>
          </>}
        </Panel>
        <Panel title="Engine" icon={Database} tone="cyan">
          <Async state={health}>{(h) => (
            <dl className="kv"><dt>Status</dt><dd style={{ color: 'var(--green)' }}>{h.status}</dd><dt>Storage</dt><dd>{h.storage}</dd><dt>Incidents</dt><dd>{h.incidents}</dd><dt>Resources</dt><dd>{h.resources}</dd><dt>Graph vertices</dt><dd>{h.graphVertices}</dd><dt>Data</dt><dd style={{ color: 'var(--violet)', fontFamily: 'var(--font-ui)' }}>{h.dataNotice}</dd></dl>
          )}</Async>
        </Panel>
      </div>
      {can('manage_settings') && (
        <>
          <div style={{ height: 16 }} />
          <Panel title="Audit log (system_logs)" icon={ScrollText} bodyClass="flush scroll">
            <Async state={logs}>{(rows) => rows.map((l) => (
              <div key={l.id} className="row" style={{ padding: '7px 16px', borderBottom: '1px solid var(--line)', fontSize: 12, gap: 12 }}>
                <span className="mono faint" style={{ width: 120 }}>{clock(l.createdAt)}</span>
                <span className={`badge ${l.level === 'ERROR' ? 'b-red' : l.level === 'WARN' ? 'b-amber' : l.level === 'AUDIT' ? 'b-violet' : 'b-cyan'}`}>{l.level}</span>
                <span className="mono" style={{ width: 90 }}>{l.source}</span><span style={{ flex: 1 }}>{l.message}</span><span className="faint">{l.actor}</span>
              </div>
            ))}</Async>
          </Panel>
        </>
      )}
    </>
  );
}
