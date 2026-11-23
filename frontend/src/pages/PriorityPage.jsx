import { useMemo } from 'react';
import { useNavigate } from 'react-router-dom';
import { AnimatePresence, motion } from 'framer-motion';
import { GitFork, ListOrdered, RefreshCw, Sigma } from 'lucide-react';
import { api } from '../services/api';
import { useApi } from '../hooks/useApi';
import { useLiveFeed } from '../hooks/useLiveFeed';
import { useToast } from '../hooks/useToast';
import PageHeader from '../components/PageHeader';
import Panel from '../components/Panel';
import TreeView from '../components/lab/TreeView';
import { Async } from '../components/Feedback';
import { SeverityBadge, StatusBadge } from '../components/Badges';
import { timeAgo } from '../services/format';

const PARTS = [['severity', 'var(--red)'], ['affected', 'var(--orange)'], ['unmet', 'var(--amber)'], ['age', 'var(--violet)'], ['status', 'var(--cyan)'], ['timeSensitivity', 'var(--green)']];

export default function PriorityPage() {
  const nav = useNavigate();
  const toast = useToast();
  const queue = useApi(() => api.priorityQueue(), []);
  const heap = useApi(() => api.priorityHeap(), []);
  useLiveFeed(() => { queue.reload({ silent: true }); heap.reload({ silent: true }); }, ['incident.', 'deployment.', 'priority.'], 500);

  const heapNodes = useMemo(() => (heap.data || []).map((r, i) => ({
    id: `h${i}`, parent: i ? `h${Math.floor((i - 1) / 2)}` : '', edge: i ? (i % 2 ? 'L' : 'R') : '', label: r.priority.toFixed(0),
    color: r.severity >= 5 ? 'red' : '',
  })), [heap.data]);

  const refresh = async () => { try { await api.refreshPriority(); toast.success('Priorities recomputed', 'Aging applied to every open incident'); queue.reload({ silent: true }); heap.reload({ silent: true }); } catch (e) { toast.fromError(e); } };

  return (
    <>
      <PageHeader kicker="Module 5" icon={ListOrdered} title="Priority & Emergency Processing"
        subtitle="Every open incident lives in an indexed binary max-heap (heap array + HashTable of positions). Score changes re-position the incident in O(log n); equal scores are served first-come-first-served.">
        <button className="btn" onClick={refresh}><RefreshCw size={15} />Recompute (aging)</button>
      </PageHeader>
      <Async state={queue} label="Reading heap">
        {(q) => (
          <div className="grid g-main">
            <Panel title="Processing order" icon={ListOrdered} hint={`${q.queue.length} open incidents`} bodyClass="flush">
              <div style={{ maxHeight: 'calc(100vh - 290px)', overflow: 'auto' }}>
                <AnimatePresence initial={false}>
                  {q.queue.map((r) => (
                    <motion.div key={r.incidentId} layout initial={{ opacity: 0, x: -10 }} animate={{ opacity: 1, x: 0 }} exit={{ opacity: 0 }}
                      transition={{ type: 'spring', stiffness: 400, damping: 34 }}
                      onClick={() => nav(`/incidents/${r.incidentId}`)} style={{ padding: '12px 16px', borderBottom: '1px solid var(--line)', cursor: 'pointer' }}>
                      <div className="row between">
                        <div className="row" style={{ gap: 14, minWidth: 0 }}>
                          <span className="mono" style={{ width: 28, fontSize: 18, color: r.rank <= 3 ? 'var(--red)' : 'var(--text-faint)' }}>{String(r.rank).padStart(2, '0')}</span>
                          <div style={{ minWidth: 0 }}>
                            <div className="cell-main">{r.title}</div>
                            <div className="cell-sub mono">{r.code} · registered {timeAgo(r.createdAt)}</div>
                          </div>
                        </div>
                        <div className="row" style={{ gap: 10 }}>
                          <SeverityBadge severity={r.severity} /><StatusBadge status={r.status} />
                          <span className="mono" style={{ fontSize: 20, fontWeight: 700, color: 'var(--amber)', width: 60, textAlign: 'right' }}>{r.priority.toFixed(1)}</span>
                        </div>
                      </div>
                      <div className="row" style={{ height: 6, marginTop: 8, borderRadius: 4, overflow: 'hidden', background: 'rgba(255,255,255,.04)' }}>
                        {PARTS.map(([k, c]) => <motion.div key={k} title={`${k} ${r.breakdown[k].toFixed(1)}`} initial={{ width: 0 }} animate={{ width: `${r.breakdown[k]}%` }} transition={{ duration: 0.7 }} style={{ height: '100%', background: c }} />)}
                      </div>
                    </motion.div>
                  ))}
                </AnimatePresence>
              </div>
            </Panel>
            <div className="stack" style={{ gap: 16 }}>
              <Panel title="Formula (configurable)" icon={Sigma} tone="cyan">
                <div className="formula">{q.formula}</div>
                <div className="grid g-2" style={{ marginTop: 12, fontSize: 12 }}>
                  {PARTS.map(([k, c]) => <div key={k} className="row between"><span className="row" style={{ gap: 6 }}><span className="legend-dot" style={{ background: c }} />{k}</span><span className="mono">w = {q.weights[k]}</span></div>)}
                </div>
                <div className="note" style={{ marginTop: 12 }}>S = severity/5 · A = log₁₀(1+affected)/log₁₀(1+{q.weights.affectedSaturation}) · U = unmet/required · T = age/{q.weights.ageSaturationHours}h · St = status factor · Ts = category time-sensitivity. Change weights in System Settings.</div>
              </Panel>
              <Panel title="Heap array as a tree" icon={GitFork} hint="children of i: 2i+1, 2i+2">
                <TreeView nodes={heapNodes} nodeWidth={40} levelGap={58} />
                <div className="cells" style={{ marginTop: 12 }}>
                  {(heap.data || []).map((r, i) => <div key={i} className={`cell ${i === 0 ? 'top' : ''}`} title={r.code} style={{ flexDirection: 'column', height: 48 }}><span>{r.priority.toFixed(0)}</span><span className="faint" style={{ fontSize: 9 }}>[{i}]</span></div>)}
                </div>
              </Panel>
            </div>
          </div>
        )}
      </Async>
    </>
  );
}
