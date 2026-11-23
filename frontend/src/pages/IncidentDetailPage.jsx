import { useMemo, useState } from 'react';
import { Link, useNavigate, useParams } from 'react-router-dom';
import { motion } from 'framer-motion';
import { Activity, ArrowLeft, BedDouble, ClipboardList, Crosshair, Edit3, Flame, MessageSquarePlus, Trash2, Truck } from 'lucide-react';
import { api } from '../services/api';
import { useApi, useAction } from '../hooks/useApi';
import { useLiveFeed } from '../hooks/useLiveFeed';
import { useToast } from '../hooks/useToast';
import { useAuth } from '../hooks/useAuth';
import PageHeader from '../components/PageHeader';
import Panel from '../components/Panel';
import Modal from '../components/Modal';
import IncidentForm from '../components/IncidentForm';
import { Async } from '../components/Feedback';
import { Chips, ScoreBar, SeverityBadge, SimBadge, StatusBadge } from '../components/Badges';
import { TextArea } from '../components/Form';
import NexaMap from '../maps/NexaMap';
import { clock, km, mins, num, timeAgo } from '../services/format';

const NEXT = { PENDING: ['ACTIVE', 'CONTAINED'], ACTIVE: ['CONTAINED', 'RESOLVED'], CONTAINED: ['ACTIVE', 'RESOLVED'], RESOLVED: ['ACTIVE', 'ARCHIVED'], ARCHIVED: [] };

function PriorityBreakdown({ p }) {
  const parts = [['Severity', p.severity, 'var(--red)'], ['People affected', p.affected, 'var(--orange)'], ['Unmet units', p.unmet, 'var(--amber)'], ['Age', p.age, 'var(--violet)'], ['Status', p.status, 'var(--cyan)'], ['Time sensitivity', p.timeSensitivity, 'var(--green)']];
  return (
    <div className="stack" style={{ gap: 9 }}>
      <div className="row between"><span className="faint" style={{ fontSize: 11, letterSpacing: '.14em' }}>PRIORITY SCORE</span><span className="mono" style={{ fontSize: 28, color: 'var(--amber)', fontWeight: 700 }}>{p.total.toFixed(1)}</span></div>
      <div className="row" style={{ height: 10, borderRadius: 6, overflow: 'hidden', background: 'rgba(255,255,255,.05)' }}>
        {parts.map(([n, v, c]) => <motion.div key={n} title={`${n}: ${v.toFixed(1)}`} initial={{ width: 0 }} animate={{ width: `${v}%` }} transition={{ duration: 0.9 }} style={{ height: '100%', background: c }} />)}
      </div>
      {parts.map(([n, v, c]) => <div key={n} className="row between" style={{ fontSize: 12 }}><span className="row" style={{ gap: 6 }}><span className="legend-dot" style={{ background: c }} />{n}</span><span className="mono">{v.toFixed(2)}</span></div>)}
    </div>
  );
}

export default function IncidentDetailPage() {
  const { id } = useParams();
  const nav = useNavigate();
  const toast = useToast();
  const { can } = useAuth();
  const [editing, setEditing] = useState(false);
  const [note, setNote] = useState('');
  const [busy, run] = useAction();
  const inc = useApi(() => api.incident(id), [id]);
  const hosp = useApi(() => api.suitableHospitals({ incidentId: id, limit: 4 }), [id]);
  useLiveFeed(() => inc.reload({ silent: true }), ['incident.', 'deployment.', 'request.', 'hospital.'], 600);

  const act = (fn, ok) => run(async () => { try { await fn(); toast.success(ok); inc.reload({ silent: true }); hosp.reload({ silent: true }); } catch (e) { toast.fromError(e); } });
  const fitTo = useMemo(() => (inc.data ? [inc.data, ...(hosp.data || []).slice(0, 2).map((h) => h.hospital)] : null), [inc.data, hosp.data]);
  const incidentsLayer = useMemo(() => (inc.data ? [inc.data] : []), [inc.data]);
  const hospitalLayer = useMemo(() => (hosp.data || []).map((h) => h.hospital), [hosp.data]);

  return (
    <Async state={inc} label="Loading incident file">
      {(d) => (
        <>
          <PageHeader kicker={<><Link to="/incidents" className="row" style={{ gap: 4 }}><ArrowLeft size={12} />Incidents</Link></>} title={d.title}
            subtitle={<span className="row wrap" style={{ gap: 8 }}><span className="mono" style={{ color: 'var(--amber)' }}>{d.code}</span><SeverityBadge severity={d.severity} /><StatusBadge status={d.status} /><SimBadge simulated={d.simulated} /><span className="faint">{d.categoryPath} · {timeAgo(d.createdAt)}</span></span>}>
            {can('manage_incidents') && NEXT[d.status].map((s) => <button key={s} className="btn" disabled={busy} onClick={() => act(() => api.setIncidentStatus(d.id, s), `Status → ${s}`)}>→ {s}</button>)}
            {can('manage_incidents') && <button className="btn" onClick={() => setEditing(true)}><Edit3 size={15} />Edit</button>}
            {can('allocate') && <button className="btn primary" onClick={() => nav(`/allocation?incident=${d.id}`)}><Crosshair size={15} />Allocate resources</button>}
          </PageHeader>

          <div className="grid g-main">
            <div className="stack" style={{ gap: 16 }}>
              <Panel title="Situation" icon={Flame}>
                <p style={{ marginTop: 0, lineHeight: 1.6 }}>{d.description || <span className="faint">No description</span>}</p>
                <div className="grid g-4" style={{ marginTop: 14 }}>
                  <div><div className="faint" style={{ fontSize: 11 }}>AFFECTED</div><div className="mono" style={{ fontSize: 22 }}>{num(d.affectedPeople)}</div></div>
                  <div><div className="faint" style={{ fontSize: 11 }}>UNITS ASSIGNED</div><div className="mono" style={{ fontSize: 22, color: d.assignedUnits >= d.requiredUnits ? 'var(--green)' : 'var(--amber)' }}>{d.assignedUnits}/{d.requiredUnits}</div></div>
                  <div><div className="faint" style={{ fontSize: 11 }}>LOCATION</div><div style={{ fontSize: 14 }}>{d.locationName || '—'}</div><div className="mono faint" style={{ fontSize: 11 }}>{d.lat.toFixed(4)}, {d.lon.toFixed(4)}</div></div>
                  <div><div className="faint" style={{ fontSize: 11 }}>REPORTED</div><div style={{ fontSize: 14 }}>{d.reportedBy}</div><div className="faint" style={{ fontSize: 11 }}>{d.reporterContact}</div></div>
                </div>
                <div className="divider" />
                <div className="faint" style={{ fontSize: 11, letterSpacing: '.14em', marginBottom: 6 }}>REQUIRED CAPABILITIES</div>
                <Chips items={d.requiredCapabilities} />
              </Panel>

              <Panel title="Assigned resources (multi-linked list)" icon={Truck} bodyClass="flush">
                {d.assigned.length === 0 ? <div className="empty">No active deployments{can('allocate') && <> — <Link to={`/allocation?incident=${d.id}`}>allocate now</Link></>}</div> : d.assigned.map((a) => (
                  <div key={a.resourceId} style={{ padding: '12px 16px', borderBottom: '1px solid var(--line)' }}>
                    <div className="row between"><Link to={`/resources/${a.resourceId}`} className="mono" style={{ fontWeight: 700 }}>{a.resourceCode} × {a.units}</Link>
                      <span className="mono faint">{km(a.routeKm)} · ETA {mins(a.etaMinutes)} · suitability {a.suitability.toFixed(1)}</span></div>
                    <div className="cell-sub" style={{ marginTop: 4 }}>{a.reason}</div>
                  </div>
                ))}
                {d.deploymentIds.length > 0 && can('allocate') && (
                  <div className="row" style={{ padding: 12, gap: 8 }}>
                    {d.deploymentIds.map((dep) => (
                      <span key={dep} className="row" style={{ gap: 6 }}>
                        <button className="btn sm green" onClick={() => act(() => api.completeDeployment(dep), `Deployment #${dep} completed`)}>Complete #{dep}</button>
                        <button className="btn sm danger" onClick={() => act(() => api.cancelDeployment(dep, 'cancelled by operator'), `Deployment #${dep} cancelled`)}>Cancel</button>
                      </span>
                    ))}
                  </div>
                )}
              </Panel>

              <Panel title="Event timeline (linked list)" icon={Activity} bodyClass="scroll">
                {can('manage_incidents') && (
                  <div className="row" style={{ marginBottom: 14, alignItems: 'flex-start' }}>
                    <TextArea value={note} onChange={setNote} placeholder="Add an operational note…" style={{ minHeight: 44 }} />
                    <button className="btn" disabled={!note.trim() || busy} onClick={() => act(async () => { await api.addNote(d.id, note); setNote(''); }, 'Note added')}><MessageSquarePlus size={15} /></button>
                  </div>
                )}
                <div className="timeline">
                  {[...d.timeline].reverse().map((e) => (
                    <div key={e.id || `${e.createdAt}-${e.message}`} className={`tl-item ${e.type}`}>
                      <div className="tl-meta">{clock(e.createdAt)} · {e.type} · {e.actor}</div>
                      <div className="tl-msg">{e.message}</div>
                    </div>
                  ))}
                </div>
              </Panel>
            </div>

            <div className="stack" style={{ gap: 16 }}>
              <Panel title="Priority breakdown" icon={ClipboardList} tone="alert"><PriorityBreakdown p={d.priority} /></Panel>
              <Panel title="Location & nearest hospitals" icon={BedDouble} bodyClass="flush">
                <NexaMap className="short" incidents={incidentsLayer} hospitals={hospitalLayer} fitTo={fitTo} />
                <div>
                  {(hosp.data || []).map((h) => (
                    <div key={h.hospital.id} style={{ padding: '10px 16px', borderTop: '1px solid var(--line)' }}>
                      <div className="row between"><span style={{ fontWeight: 600 }}>{h.hospital.name}</span><span className="mono" style={{ color: 'var(--amber)' }}>{h.score.toFixed(1)}</span></div>
                      <div className="cell-sub">{h.reason}</div>
                      <ScoreBar value={h.score} color="var(--violet)" />
                    </div>
                  ))}
                </div>
              </Panel>
              <Panel title="Emergency requests" icon={ClipboardList} bodyClass="flush">
                {d.requests.length === 0 ? <div className="empty">No requests</div> : d.requests.map((r) => (
                  <div key={r.id} className="row between" style={{ padding: '10px 16px', borderBottom: '1px solid var(--line)' }}>
                    <div><div className="cell-main">#{r.id} · {r.assistanceType}</div><div className="cell-sub">urgency {r.urgency} · score {r.urgencyScore.toFixed(1)}</div></div>
                    <StatusBadge status={r.status} />
                  </div>
                ))}
              </Panel>
              {can('manage_incidents') && (
                <button className="btn danger" onClick={() => act(async () => { const r = await api.deleteIncident(d.id); if (r.result === 'deleted') nav('/incidents'); }, 'Incident removed / archived')}>
                  <Trash2 size={15} />Delete (or archive if it has history)
                </button>
              )}
            </div>
          </div>

          <Modal open={editing} onClose={() => setEditing(false)} title={`Edit ${d.code}`} wide>
            <IncidentForm busy={busy} submitLabel="Save changes" initial={{ ...d, categoryId: String(d.categoryId) }}
              onSubmit={(body) => run(async () => { try { await api.updateIncident(d.id, body); toast.success('Incident updated'); setEditing(false); inc.reload({ silent: true }); } catch (e) { toast.fromError(e); } })} />
          </Modal>
        </>
      )}
    </Async>
  );
}
