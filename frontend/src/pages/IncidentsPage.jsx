import { useEffect, useState } from 'react';
import { useNavigate, useSearchParams } from 'react-router-dom';
import { Cpu, Flame, Plus, Search, Siren, Undo2 } from 'lucide-react';
import { api } from '../services/api';
import { useApi, useAction } from '../hooks/useApi';
import { useLiveFeed } from '../hooks/useLiveFeed';
import { useToast } from '../hooks/useToast';
import { useAuth } from '../hooks/useAuth';
import PageHeader from '../components/PageHeader';
import Panel from '../components/Panel';
import DataTable from '../components/DataTable';
import Modal from '../components/Modal';
import IncidentForm from '../components/IncidentForm';
import { Async } from '../components/Feedback';
import { Field, Select, TextInput } from '../components/Form';
import { SeverityBadge, SimBadge, StatusBadge } from '../components/Badges';
import { num, timeAgo } from '../services/format';

const STATUSES = [{ value: '', label: 'Open (pending/active/contained)' }, 'PENDING', 'ACTIVE', 'CONTAINED', 'RESOLVED', 'ARCHIVED', { value: 'ALL', label: 'All' }];
const SORTS = [{ value: 'priority', label: 'Priority score' }, { value: 'created', label: 'Newest first' }, { value: 'severity', label: 'Severity' }, { value: 'affected', label: 'People affected' }];
const ALGOS = [{ value: 'merge', label: 'Merge sort (stable)' }, { value: 'quick', label: 'Quick sort' }, { value: 'heap', label: 'Heap sort' }, { value: 'insertion', label: 'Insertion sort' }, { value: 'counting', label: 'Counting sort (severity)' }];

export default function IncidentsPage() {
  const nav = useNavigate();
  const toast = useToast();
  const { can } = useAuth();
  const [params, setParams] = useSearchParams();
  const [filters, setFilters] = useState({ status: '', categoryId: '', minSeverity: 1, sortBy: 'priority', algorithm: 'merge', prefix: params.get('prefix') || '', text: '' });
  const [creating, setCreating] = useState(params.get('new') === '1');
  const [cats, setCats] = useState([]);
  const [busy, run] = useAction();
  const set = (k) => (v) => setFilters((f) => ({ ...f, [k]: v }));

  useEffect(() => { api.categories().then(setCats).catch(() => {}); }, []);
  const list = useApi(() => api.incidents({ ...filters, limit: 300 }), [JSON.stringify(filters)]);
  useLiveFeed(() => list.reload({ silent: true }), ['incident.', 'deployment.'], 800);

  const create = (body) => run(async () => {
    try {
      const inc = await api.createIncident(body);
      toast.success(`${inc.code} registered`, `Priority ${inc.priorityScore.toFixed(1)} · ${inc.categoryPath}`);
      setCreating(false); setParams({});
      nav(`/incidents/${inc.id}`);
    } catch (e) { toast.fromError(e, 'Registration rejected'); }
  });

  const undo = async () => {
    try { const r = await api.undo(); r.undone ? toast.success('Undone', r.message) : toast.info('Nothing to undo', r.message); list.reload({ silent: true }); }
    catch (e) { toast.fromError(e); }
  };

  const columns = [
    { key: 'code', label: 'Incident', render: (r) => <div><div className="cell-main">{r.title} <SimBadge simulated={r.simulated} /></div><div className="cell-sub mono">{r.code} · {r.locationName}</div></div> },
    { key: 'category', label: 'Category', render: (r) => <span className="muted">{r.categoryPath}</span> },
    { key: 'severity', label: 'Severity', render: (r) => <SeverityBadge severity={r.severity} /> },
    { key: 'status', label: 'Status', render: (r) => <StatusBadge status={r.status} /> },
    { key: 'affected', label: 'Affected', align: 'right', className: 'num', render: (r) => num(r.affectedPeople) },
    { key: 'units', label: 'Units', align: 'right', className: 'num', render: (r) => <span style={{ color: r.assignedUnits >= r.requiredUnits ? 'var(--green)' : 'var(--amber)' }}>{r.assignedUnits}/{r.requiredUnits}</span> },
    { key: 'priority', label: 'Priority', align: 'right', className: 'num', render: (r) => <span style={{ color: 'var(--amber)', fontWeight: 700 }}>{r.priorityScore.toFixed(1)}</span> },
    { key: 'age', label: 'Registered', render: (r) => <span className="faint">{timeAgo(r.createdAt)}</span> },
  ];

  const catOptions = [{ value: '', label: 'All categories' }, ...cats.filter((c) => c.depth >= 1).map((c) => ({ value: String(c.id), label: `${c.depth === 2 ? '  └ ' : ''}${c.name}` }))];

  return (
    <>
      <PageHeader kicker="Module 2" icon={Flame} title="Incident Management"
        subtitle="Registration, classification and tracking. Filters run on the AVL index, prefix search on the Trie, free-text search on the Suffix Array — with the selected sorting algorithm.">
        <button className="btn" onClick={undo} disabled={!can('manage_incidents')}><Undo2 size={15} />Undo last status change</button>
        <button className="btn primary" onClick={() => setCreating(true)} disabled={!can('manage_incidents')}><Plus size={15} />Register incident</button>
      </PageHeader>

      <Panel title="Query" icon={Search} className="" bodyClass="">
        <div className="toolbar">
          <Field label="Status"><Select value={filters.status} onChange={set('status')} options={STATUSES} /></Field>
          <Field label="Category (incl. sub-types)"><Select value={filters.categoryId} onChange={set('categoryId')} options={catOptions} /></Field>
          <Field label={`Min severity · ${filters.minSeverity}`}><input type="range" className="range" min={1} max={5} value={filters.minSeverity} onChange={(e) => set('minSeverity')(Number(e.target.value))} /></Field>
          <Field label="Prefix (Trie)"><TextInput value={filters.prefix} onChange={(v) => setFilters((f) => ({ ...f, prefix: v, text: v ? '' : f.text }))} placeholder="e.g. gha, coll" /></Field>
          <Field label="Text (Suffix array)"><TextInput value={filters.text} onChange={(v) => setFilters((f) => ({ ...f, text: v, prefix: v ? '' : f.prefix }))} placeholder="e.g. trapped" /></Field>
          <Field label="Sort by"><Select value={filters.sortBy} onChange={set('sortBy')} options={SORTS} /></Field>
          <Field label="Algorithm"><Select value={filters.algorithm} onChange={set('algorithm')} options={ALGOS} /></Field>
        </div>
      </Panel>

      <div style={{ height: 16 }} />
      <Panel title="Incidents" icon={Siren} bodyClass="flush"
        hint={list.data ? `${list.data.count} rows · index ${list.data.stats.index} · ${list.data.stats.sortAlgorithm} · ${num(list.data.stats.comparisons)} comparisons · ${list.data.stats.microseconds.toFixed(0)} µs` : ''}>
        <Async state={list} label="Querying index">
          {(d) => <DataTable columns={columns} rows={d.items} onRowClick={(r) => nav(`/incidents/${r.id}`)} empty="No incidents match the query" maxHeight="calc(100vh - 420px)" />}
        </Async>
      </Panel>
      <div className="faint row" style={{ marginTop: 10, fontSize: 12 }}><Cpu size={13} />All filtering, searching and sorting above executed in the C++ engine.</div>

      <Modal open={creating} onClose={() => { setCreating(false); setParams({}); }} title="Register disaster incident" icon={Siren} wide>
        <IncidentForm onSubmit={create} busy={busy} />
      </Modal>
    </>
  );
}
