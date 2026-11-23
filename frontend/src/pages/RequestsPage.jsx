import { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { AnimatePresence, motion } from 'framer-motion';
import { ArrowRight, ChevronsUp, Play, Plus, Radio, XCircle } from 'lucide-react';
import { api } from '../services/api';
import { useApi, useAction } from '../hooks/useApi';
import { useLiveFeed } from '../hooks/useLiveFeed';
import { useToast } from '../hooks/useToast';
import { useAuth } from '../hooks/useAuth';
import PageHeader from '../components/PageHeader';
import Panel from '../components/Panel';
import Modal from '../components/Modal';
import DataTable from '../components/DataTable';
import { Async } from '../components/Feedback';
import { Field, NumberInput, Select, TextArea } from '../components/Form';
import { StatusBadge } from '../components/Badges';
import { clock, timeAgo } from '../services/format';

const TYPES = ['medical', 'rescue', 'fire', 'evacuation', 'supplies', 'shelter'];

function QueueCard({ r, onEscalate, onCancel, canEdit, accent }) {
  return (
    <motion.div layout initial={{ opacity: 0, scale: 0.8, y: 10 }} animate={{ opacity: 1, scale: 1, y: 0 }} exit={{ opacity: 0, scale: 0.6, x: 40 }}
      transition={{ type: 'spring', stiffness: 420, damping: 28 }} className="panel" style={{ padding: 12, minWidth: 190, borderColor: accent }}>
      <div className="row between"><span className="mono" style={{ color: accent }}>#{r.id}</span><span className="mono faint">{r.urgencyScore.toFixed(0)}</span></div>
      <div className="cell-main" style={{ marginTop: 4, textTransform: 'capitalize' }}>{r.assistanceType}</div>
      <div className="cell-sub">incident #{r.incidentId} · urgency {r.urgency} · {timeAgo(r.createdAt)}</div>
      {canEdit && (
        <div className="row" style={{ marginTop: 8, gap: 6 }}>
          {onEscalate && <button className="btn sm danger" onClick={() => onEscalate(r.id)}><ChevronsUp size={13} />Escalate</button>}
          <button className="btn sm ghost" onClick={() => onCancel(r.id)}><XCircle size={13} /></button>
        </div>
      )}
    </motion.div>
  );
}

export default function RequestsPage() {
  const nav = useNavigate();
  const toast = useToast();
  const { can } = useAuth();
  const [busy, run] = useAction();
  const [creating, setCreating] = useState(false);
  const [form, setForm] = useState({ incidentId: '', assistanceType: 'medical', urgency: 3, peopleAffected: 10, description: '' });
  const [last, setLast] = useState(null);
  const queues = useApi(() => api.queues(), []);
  const all = useApi(() => api.requests(), []);
  const open = useApi(() => api.incidents({ limit: 200 }), []);
  useLiveFeed(() => { queues.reload({ silent: true }); all.reload({ silent: true }); }, ['request.', 'deployment.'], 500);
  const refresh = () => { queues.reload({ silent: true }); all.reload({ silent: true }); };

  const act = (fn, ok) => run(async () => { try { const r = await fn(); if (ok) toast.success(ok); refresh(); return r; } catch (e) { toast.fromError(e); return null; } });
  const process = (mode) => act(async () => {
    const r = await api.processNext(mode);
    setLast(r);
    if (!r.processed) toast.info('Queues empty', r.message); else if (r.allocated) toast.success(`Request #${r.request.id} served from ${r.queueUsed} queue`, r.message); else toast.error('Re-queued', r.message);
    return r;
  });

  const columns = [
    { key: 'id', label: '#', render: (r) => <span className="mono">#{r.id}</span> },
    { key: 'type', label: 'Assistance', render: (r) => <span style={{ textTransform: 'capitalize' }}>{r.assistanceType}</span> },
    { key: 'inc', label: 'Incident', render: (r) => <a onClick={(e) => { e.stopPropagation(); nav(`/incidents/${r.incidentId}`); }} style={{ cursor: 'pointer' }}>#{r.incidentId}</a> },
    { key: 'urg', label: 'Urgency', className: 'num', align: 'right', render: (r) => r.urgency },
    { key: 'score', label: 'Score', className: 'num', align: 'right', render: (r) => <span style={{ color: 'var(--amber)' }}>{r.urgencyScore.toFixed(1)}</span> },
    { key: 'status', label: 'Status', render: (r) => <StatusBadge status={r.status} /> },
    { key: 'dep', label: 'Deployment', render: (r) => (r.deploymentId ? <span className="mono">#{r.deploymentId}</span> : <span className="faint">—</span>) },
    { key: 'at', label: 'Received', render: (r) => <span className="faint">{clock(r.createdAt)}</span> },
  ];

  return (
    <>
      <PageHeader kicker="Module 4" icon={Radio} title="Emergency Request Management"
        subtitle="Ordinary requests wait in a FIFO circular-buffer Queue; urgent or escalated ones enter an indexed Priority Queue. Processing serves the priority queue first and then the FIFO queue, and auto-allocates through the ranking engine.">
        <button className="btn" disabled={!can('allocate') || busy} onClick={() => process('fifo')}>Process FIFO only</button>
        <button className="btn" disabled={!can('allocate') || busy} onClick={() => process('priority')}>Process priority only</button>
        <button className="btn primary" disabled={!can('allocate') || busy} onClick={() => process('auto')}><Play size={15} />Process next</button>
        <button className="btn cyan" disabled={!can('manage_requests')} onClick={() => setCreating(true)}><Plus size={15} />New request</button>
      </PageHeader>

      <Async state={queues} label="Reading queues">
        {(q) => (
          <div className="grid g-2">
            <Panel title="Priority queue (served first)" icon={ChevronsUp} tone="alert" hint={`${q.priority.length} waiting · highest score first`}>
              <div className="row" style={{ gap: 10, overflowX: 'auto', paddingBottom: 6, minHeight: 130 }}>
                <AnimatePresence>{q.priority.map((r) => <QueueCard key={r.id} r={r} accent="var(--red)" canEdit={can('manage_requests')} onCancel={(id) => act(() => api.cancelRequest(id), 'Cancelled')} />)}</AnimatePresence>
                {!q.priority.length && <div className="faint">No escalated requests</div>}
              </div>
            </Panel>
            <Panel title="FIFO queue (arrival order)" icon={ArrowRight} tone="cyan" hint={`${q.fifo.length} waiting · front → rear`}>
              <div className="row" style={{ gap: 10, overflowX: 'auto', paddingBottom: 6, minHeight: 130 }}>
                <AnimatePresence>{q.fifo.map((r) => <QueueCard key={r.id} r={r} accent="var(--cyan)" canEdit={can('manage_requests')} onEscalate={(id) => act(() => api.escalate(id), 'Escalated to priority queue')} onCancel={(id) => act(() => api.cancelRequest(id), 'Cancelled')} />)}</AnimatePresence>
                {!q.fifo.length && <div className="faint">Queue empty</div>}
              </div>
            </Panel>
          </div>
        )}
      </Async>

      <AnimatePresence>
        {last && last.processed && (
          <motion.div initial={{ opacity: 0, height: 0 }} animate={{ opacity: 1, height: 'auto' }} exit={{ opacity: 0, height: 0 }} style={{ marginTop: 16 }}>
            <Panel title={`Last processed: request #${last.request.id} (${last.queueUsed} queue)`} icon={Play} tone={last.allocated ? 'cyan' : 'alert'}
              actions={<button className="btn sm ghost" onClick={() => setLast(null)}>Dismiss</button>}>
              <div className="muted">{last.message}</div>
              {last.deployment && last.deployment.resources.map((d) => <div key={d.resourceId} className="note cyan" style={{ marginTop: 8 }}><b className="mono">{d.resourceCode}</b> — {d.reason}</div>)}
            </Panel>
          </motion.div>
        )}
      </AnimatePresence>

      <div style={{ height: 16 }} />
      <Panel title="All requests (history)" icon={Radio} bodyClass="flush">
        <Async state={all}>{(rows) => <DataTable columns={columns} rows={[...rows].reverse()} maxHeight={420} />}</Async>
      </Panel>

      <Modal open={creating} onClose={() => setCreating(false)} title="New emergency request" icon={Radio}
        footer={<button className="btn primary" disabled={busy || !form.incidentId} onClick={() => act(async () => { const r = await api.createRequest({ ...form, incidentId: Number(form.incidentId) }); toast.success(`Request #${r.id} → ${r.status === 'ESCALATED' ? 'priority' : 'FIFO'} queue`, `score ${r.urgencyScore.toFixed(1)}`); setCreating(false); return r; })}>Submit request</button>}>
        <div className="form-grid">
          <Field label="Incident" full><Select value={form.incidentId} onChange={(v) => setForm((f) => ({ ...f, incidentId: v }))} placeholder="Select open incident…"
            options={(open.data?.items || []).map((i) => ({ value: String(i.id), label: `${i.code} — ${i.title}` }))} /></Field>
          <Field label="Assistance type"><Select value={form.assistanceType} onChange={(v) => setForm((f) => ({ ...f, assistanceType: v }))} options={TYPES} /></Field>
          <Field label={`Urgency · ${form.urgency}`} hint="5 always escalates"><input type="range" className="range" min={1} max={5} value={form.urgency} onChange={(e) => setForm((f) => ({ ...f, urgency: Number(e.target.value) }))} /></Field>
          <Field label="People affected"><NumberInput value={form.peopleAffected} onChange={(v) => setForm((f) => ({ ...f, peopleAffected: v }))} min={0} /></Field>
          <Field label="Description" full><TextArea value={form.description} onChange={(v) => setForm((f) => ({ ...f, description: v }))} /></Field>
        </div>
        <div className="formula" style={{ marginTop: 12 }}>R = 50·(urgency/5) + 20·min(1, log₁₀(1+people)/3) + 30·(incident priority/100)   → priority queue if R ≥ threshold or urgency = 5</div>
      </Modal>
    </>
  );
}
