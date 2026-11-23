import { useEffect, useMemo, useState } from 'react';
import { Link, useSearchParams } from 'react-router-dom';
import { AnimatePresence, motion } from 'framer-motion';
import { CheckCircle2, Crosshair, Grid3x3, Shuffle, Truck, Zap } from 'lucide-react';
import { api } from '../services/api';
import { useApi, useAction } from '../hooks/useApi';
import { useLiveFeed } from '../hooks/useLiveFeed';
import { useToast } from '../hooks/useToast';
import { useAuth } from '../hooks/useAuth';
import PageHeader from '../components/PageHeader';
import Panel from '../components/Panel';
import Modal from '../components/Modal';
import { Async } from '../components/Feedback';
import { Field, NumberInput, Select } from '../components/Form';
import { StatusBadge } from '../components/Badges';
import NexaMap from '../maps/NexaMap';
import { clock, km, mins } from '../services/format';

function MatrixView({ m }) {
  const incidents = [...new Set(m.cells.map((c) => c.incidentCode))];
  const resources = [...new Set(m.cells.map((c) => c.resourceCode))];
  const at = (i, r) => m.cells.find((c) => c.incidentCode === i && c.resourceCode === r);
  const max = Math.max(1, ...m.cells.map((c) => c.units));
  return (
    <>
      <div className="row wrap" style={{ gap: 14, marginBottom: 12, fontSize: 12 }}>
        <span className="chip">non-zero: {m.cells.length}</span><span className="chip">logical size: {m.rows} × {m.cols}</span>
        <span className="chip">density: {(m.density * 100).toFixed(2)}%</span><span className="chip on">sparse {m.sparseBytes} B vs dense {m.denseBytes} B</span>
      </div>
      {m.cells.length === 0 ? <div className="empty">No active allocations</div> : (
        <div style={{ overflow: 'auto' }}>
          <table className="data" style={{ fontSize: 11 }}>
            <thead><tr><th>incident \ resource</th>{resources.map((r) => <th key={r} style={{ writingMode: 'vertical-rl', transform: 'rotate(180deg)', height: 90 }}>{r}</th>)}</tr></thead>
            <tbody>{incidents.map((i) => (
              <tr key={i}><td className="mono">{i}</td>{resources.map((r) => {
                const c = at(i, r);
                return <td key={r} style={{ padding: 2 }}>{c ? <motion.div initial={{ scale: 0 }} animate={{ scale: 1 }} title={`${c.units} unit(s)`} style={{ height: 22, borderRadius: 4, display: 'grid', placeItems: 'center', fontFamily: 'var(--font-mono)', background: `rgba(255,182,39,${0.2 + 0.7 * (c.units / max)})`, color: '#140d00' }}>{c.units}</motion.div> : <div style={{ height: 22, borderRadius: 4, background: 'rgba(255,255,255,.025)' }} />}</td>;
              })}</tr>
            ))}</tbody>
          </table>
        </div>
      )}
    </>
  );
}

export default function AllocationPage() {
  const toast = useToast();
  const { can } = useAuth();
  const [params, setParams] = useSearchParams();
  const [incidentId, setIncidentId] = useState(params.get('incident') || '');
  const [picked, setPicked] = useState({});
  const [reassign, setReassign] = useState(null);
  const [busy, run] = useAction();
  const open = useApi(() => api.incidents({ limit: 200 }), []);
  useEffect(() => { if (!incidentId && open.data?.items?.length) setIncidentId(String(open.data.items[0].id)); }, [open.data, incidentId]);
  const rank = useApi(() => api.ranking(incidentId, { limit: 12 }), [incidentId], { enabled: Boolean(incidentId) });
  const deps = useApi(() => api.deployments('ACTIVE'), []);
  const matrix = useApi(() => api.matrix(), []);
  const refresh = () => { rank.reload({ silent: true }); deps.reload({ silent: true }); matrix.reload({ silent: true }); open.reload({ silent: true }); };
  useLiveFeed(refresh, ['deployment.', 'resource.'], 600);
  useEffect(() => setPicked({}), [incidentId]);

  const incident = open.data?.items?.find((i) => String(i.id) === String(incidentId));
  const act = (fn, ok) => run(async () => { try { const r = await fn(); toast.success(ok(r)); refresh(); setPicked({}); return r; } catch (e) { toast.fromError(e, 'Allocation rejected'); return null; } });
  const allocateAuto = () => act(() => api.allocate({ incidentId: Number(incidentId) }), (d) => `Deployment #${d.id}: ${d.resources.map((r) => r.resourceCode).join(', ')}`);
  const allocateManual = () => act(() => api.allocate({ incidentId: Number(incidentId), items: Object.entries(picked).map(([resourceId, units]) => ({ resourceId: Number(resourceId), units })) }), (d) => `Deployment #${d.id} created`);

  const incidents = useMemo(() => (incident ? [incident] : []), [incident]);
  const cands = useMemo(() => (rank.data?.candidates || []).map((c) => ({ id: c.resourceId, code: c.code, name: c.name, lat: c.lat, lon: c.lon, status: 'AVAILABLE', category: c.category, available: c.available, capacity: c.capacity })), [rank.data]);
  const highlight = useMemo(() => ({ resources: new Set(Object.keys(picked).map(Number)) }), [picked]);
  const fitTo = useMemo(() => (incident ? [incident, ...cands.slice(0, 4)] : null), [incident, cands]);

  return (
    <>
      <PageHeader kicker="Module 9" icon={Crosshair} title="Resource Allocation & Deployment"
        subtitle="Allocation validates availability, units, capability and simultaneous-deployment conflicts (per-resource interval tree), then commits deployment + resource + incident changes in ONE MySQL transaction. Every assignment records why it was chosen.">
        <Field label="Incident"><Select value={incidentId} onChange={(v) => { setIncidentId(v); setParams({ incident: v }); }} options={(open.data?.items || []).map((i) => ({ value: String(i.id), label: `${i.code} · ${i.title} (${i.assignedUnits}/${i.requiredUnits})` }))} /></Field>
      </PageHeader>

      <div className="grid g-main">
        <Panel title="Candidates" icon={Zap} hint={incident ? `${incident.assignedUnits}/${incident.requiredUnits} units assigned` : ''} bodyClass="flush"
          actions={can('allocate') && (
            <>
              <button className="btn sm" disabled={busy || !Object.keys(picked).length} onClick={allocateManual}>Deploy selected ({Object.keys(picked).length})</button>
              <button className="btn sm primary" disabled={busy || !incidentId} onClick={allocateAuto}><Zap size={13} />Auto-allocate</button>
            </>
          )}>
          <Async state={rank} label="Ranking">
            {(r) => r.candidates.map((c, i) => {
              const sel = picked[c.resourceId] !== undefined;
              return (
                <motion.div key={c.resourceId} layout className="row between" style={{ padding: '11px 16px', borderBottom: '1px solid var(--line)', background: sel ? 'var(--amber-soft)' : undefined }}>
                  <label className="checkbox" style={{ gap: 12 }}>
                    <input type="checkbox" checked={sel} onChange={(e) => setPicked((p) => { const n = { ...p }; if (e.target.checked) n[c.resourceId] = 1; else delete n[c.resourceId]; return n; })} />
                    <span><span className="mono" style={{ color: 'var(--text)', fontWeight: 700 }}>{i + 1}. {c.code}</span><span className="cell-sub" style={{ display: 'block' }}>{c.reason}</span></span>
                  </label>
                  <div className="row" style={{ gap: 10 }}>
                    {sel && c.capacity > 1 && <div style={{ width: 80 }}><NumberInput value={picked[c.resourceId]} min={1} max={c.available} onChange={(v) => setPicked((p) => ({ ...p, [c.resourceId]: Math.max(1, Math.min(c.available, Number(v) || 1)) }))} /></div>}
                    <span className="mono" style={{ color: 'var(--amber)', fontWeight: 700, width: 50, textAlign: 'right' }}>{c.score.toFixed(1)}</span>
                  </div>
                </motion.div>
              );
            })}
          </Async>
        </Panel>
        <Panel title="Dispatch view" bodyClass="flush"><NexaMap className="short" incidents={incidents} resources={cands} highlight={highlight} fitTo={fitTo} /></Panel>
      </div>

      <div style={{ height: 16 }} />
      <div className="grid g-2">
        <Panel title="Active deployments" icon={Truck} bodyClass="flush scroll">
          <Async state={deps}>
            {(list) => (list.length === 0 ? <div className="empty">No active deployments</div> : (
              <AnimatePresence>
                {list.map((d) => (
                  <motion.div key={d.id} layout initial={{ opacity: 0 }} animate={{ opacity: 1 }} exit={{ opacity: 0, x: 30 }} style={{ padding: '12px 16px', borderBottom: '1px solid var(--line)' }}>
                    <div className="row between">
                      <span><span className="mono" style={{ color: 'var(--amber)' }}>#{d.id}</span> → <Link to={`/incidents/${d.incidentId}`}>incident #{d.incidentId}</Link> <span className="faint">· {clock(d.createdAt)} · {d.createdBy}</span></span>
                      <StatusBadge status={d.status} />
                    </div>
                    {d.resources.map((r) => (
                      <div key={r.resourceId} className="row between" style={{ marginTop: 6, opacity: r.released ? 0.45 : 1 }}>
                        <span className="mono" style={{ fontSize: 12 }}>{r.resourceCode} × {r.units} · {km(r.routeKm)} · ETA {mins(r.etaMinutes)}{r.released ? ' · released' : ''}</span>
                        {!r.released && can('allocate') && <button className="btn sm ghost" onClick={() => setReassign({ resourceId: r.resourceId, code: r.resourceCode, fromDeploymentId: d.id, toIncidentId: '' })}><Shuffle size={12} />Reassign</button>}
                      </div>
                    ))}
                    {can('allocate') && (
                      <div className="row" style={{ marginTop: 8 }}>
                        <button className="btn sm green" disabled={busy} onClick={() => act(() => api.completeDeployment(d.id), () => `Deployment #${d.id} completed — units restored`)}><CheckCircle2 size={13} />Complete</button>
                        <button className="btn sm danger" disabled={busy} onClick={() => act(() => api.cancelDeployment(d.id, 'cancelled from allocation console'), () => `Deployment #${d.id} cancelled`)}>Cancel</button>
                      </div>
                    )}
                  </motion.div>
                ))}
              </AnimatePresence>
            ))}
          </Async>
        </Panel>
        <Panel title="Allocation matrix (sparse)" icon={Grid3x3} tone="cyan"><Async state={matrix}>{(m) => <MatrixView m={m} />}</Async></Panel>
      </div>

      <Modal open={Boolean(reassign)} onClose={() => setReassign(null)} title={`Reassign ${reassign?.code || ''}`} icon={Shuffle}
        footer={<button className="btn primary" disabled={!reassign?.toIncidentId || busy} onClick={() => act(() => api.reassign({ ...reassign, toIncidentId: Number(reassign.toIncidentId) }), (d) => `Moved to deployment #${d.id}`).then(() => setReassign(null))}>Reassign</button>}>
        <Field label="Target incident"><Select value={reassign?.toIncidentId || ''} onChange={(v) => setReassign((r) => ({ ...r, toIncidentId: v }))} placeholder="Select…"
          options={(open.data?.items || []).map((i) => ({ value: String(i.id), label: `${i.code} · ${i.title}` }))} /></Field>
        <div className="note" style={{ marginTop: 12 }}>The unit is released from deployment #{reassign?.fromDeploymentId} (availability restored, interval removed) and then allocated to the new incident, each in its own transaction.</div>
      </Modal>
    </>
  );
}
