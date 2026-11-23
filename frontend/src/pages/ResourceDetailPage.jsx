import { useMemo, useState } from 'react';
import { Link, useParams } from 'react-router-dom';
import { Ambulance, ArrowLeft, CalendarClock, Edit3, History, Wrench } from 'lucide-react';
import { api } from '../services/api';
import { useApi, useAction } from '../hooks/useApi';
import { useLiveFeed } from '../hooks/useLiveFeed';
import { useToast } from '../hooks/useToast';
import { useAuth } from '../hooks/useAuth';
import PageHeader from '../components/PageHeader';
import Panel from '../components/Panel';
import Modal from '../components/Modal';
import ResourceForm from '../components/ResourceForm';
import { Async } from '../components/Feedback';
import { Chips, StatusBadge } from '../components/Badges';
import { Gauge } from '../charts/Charts';
import NexaMap from '../maps/NexaMap';
import { CATEGORY_LABEL, clock } from '../services/format';

export default function ResourceDetailPage() {
  const { id } = useParams();
  const toast = useToast();
  const { can } = useAuth();
  const [editing, setEditing] = useState(false);
  const [busy, run] = useAction();
  const res = useApi(() => api.resource(id), [id]);
  useLiveFeed(() => res.reload({ silent: true }), ['resource.', 'deployment.'], 600);
  const layer = useMemo(() => (res.data ? [res.data] : []), [res.data]);
  const act = (fn, ok) => run(async () => { try { await fn(); toast.success(ok); res.reload({ silent: true }); } catch (e) { toast.fromError(e); } });

  return (
    <Async state={res} label="Loading unit">
      {(r) => (
        <>
          <PageHeader kicker={<Link to="/resources" className="row" style={{ gap: 4 }}><ArrowLeft size={12} />Resources</Link>} title={`${r.code} · ${r.name}`}
            subtitle={<span className="row" style={{ gap: 8 }}><StatusBadge status={r.status} /><span className="faint">{CATEGORY_LABEL[r.category]} · {r.typeName} · {r.organization}</span></span>}>
            {can('manage_resources') && r.status !== 'MAINTENANCE' && <button className="btn violet" disabled={busy} onClick={() => act(() => api.setResourceStatus(r.id, 'MAINTENANCE'), 'Sent to maintenance')}><Wrench size={15} />Maintenance</button>}
            {can('manage_resources') && r.status !== 'AVAILABLE' && r.status !== 'DEPLOYED' && <button className="btn green" disabled={busy} onClick={() => act(() => api.setResourceStatus(r.id, 'AVAILABLE'), 'Back in service')}>Return to service</button>}
            {can('manage_resources') && <button className="btn" onClick={() => setEditing(true)}><Edit3 size={15} />Edit</button>}
          </PageHeader>
          <div className="grid g-main">
            <div className="stack" style={{ gap: 16 }}>
              <div className="grid g-2">
                <Panel title="Availability" icon={Ambulance}>
                  <Gauge value={r.capacity ? (100 * r.available) / r.capacity : 0} label={`${r.available}/${r.capacity} units free`} color="var(--green)" size={160} />
                </Panel>
                <Panel title="Capabilities" icon={Wrench}>
                  <Chips items={r.capabilities} />
                  <div className="divider" />
                  <dl className="kv"><dt>Updated</dt><dd>{clock(r.updatedAt)}</dd><dt>Coordinates</dt><dd>{r.lat.toFixed(5)}, {r.lon.toFixed(5)}</dd><dt>Utilisation</dt><dd>{Math.round(r.utilisation * 100)}%</dd></dl>
                </Panel>
              </div>
              <Panel title="History (linked list)" icon={History} bodyClass="scroll">
                <div className="timeline">
                  {[...r.history].reverse().map((h, i) => (
                    <div key={i} className={`tl-item ${h.action === 'DEPLOYED' ? 'DEPLOYED' : h.action === 'RELEASED' ? 'RELEASED' : ''}`}>
                      <div className="tl-meta">{clock(h.at)} · {h.action}</div><div className="tl-msg">{h.detail}</div>
                    </div>
                  ))}
                </div>
              </Panel>
            </div>
            <div className="stack" style={{ gap: 16 }}>
              <Panel title="Position" bodyClass="flush"><NexaMap className="short" resources={layer} fitTo={layer} /></Panel>
              <Panel title="Commitment windows (interval tree)" icon={CalendarClock}>
                {r.commitments.length === 0 ? <div className="faint">No active commitments — free for any window.</div> : r.commitments.map((c) => (
                  <div key={c.deploymentId} className="row between" style={{ padding: '6px 0' }}><span className="mono">Deployment #{c.deploymentId}</span><span className="faint">{clock(c.from)} → {c.until === 'open' ? 'open' : clock(c.until)}</span></div>
                ))}
              </Panel>
            </div>
          </div>
          <Modal open={editing} onClose={() => setEditing(false)} title={`Edit ${r.code}`} wide>
            <ResourceForm busy={busy} submitLabel="Save" initial={{ ...r, typeId: String(r.typeId) }}
              onSubmit={(b) => run(async () => { try { await api.updateResource(r.id, b); toast.success('Saved'); setEditing(false); res.reload({ silent: true }); } catch (e) { toast.fromError(e); } })} />
          </Modal>
        </>
      )}
    </Async>
  );
}
