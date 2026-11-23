import { useState } from 'react';
import { useNavigate, useSearchParams } from 'react-router-dom';
import { Ambulance, Plus, Search } from 'lucide-react';
import { api } from '../services/api';
import { useApi, useAction } from '../hooks/useApi';
import { useLiveFeed } from '../hooks/useLiveFeed';
import { useToast } from '../hooks/useToast';
import { useAuth } from '../hooks/useAuth';
import PageHeader from '../components/PageHeader';
import Panel from '../components/Panel';
import DataTable from '../components/DataTable';
import Modal from '../components/Modal';
import ResourceForm from '../components/ResourceForm';
import { Async } from '../components/Feedback';
import { Checkbox, Field, Select, TextInput } from '../components/Form';
import { Chips, ScoreBar, StatusBadge } from '../components/Badges';
import { CATEGORY_COLORS, CATEGORY_LABEL, num } from '../services/format';
import { CAPABILITIES } from '../components/IncidentForm';

const SORTS = [{ value: 'code', label: 'Code' }, { value: 'capacity', label: 'Capacity' }, { value: 'available', label: 'Available units' }, { value: 'utilisation', label: 'Utilisation' }, { value: 'distance', label: 'Distance from CP' }];
const ALGOS = [{ value: 'merge', label: 'Merge sort' }, { value: 'quick', label: 'Quick sort' }, { value: 'heap', label: 'Heap sort' }, { value: 'radix', label: 'Radix sort (capacity/available)' }, { value: 'bucket', label: 'Bucket sort (distance)' }];

export default function ResourcesPage() {
  const nav = useNavigate();
  const toast = useToast();
  const { can } = useAuth();
  const [params] = useSearchParams();
  const [f, setF] = useState({ category: '', status: '', capability: '', prefix: params.get('prefix') || '', availableOnly: false, sortBy: 'code', algorithm: 'merge' });
  const [creating, setCreating] = useState(false);
  const [busy, run] = useAction();
  const set = (k) => (v) => setF((x) => ({ ...x, [k]: v }));
  const list = useApi(() => api.resources({ ...f, availableOnly: f.availableOnly ? 1 : undefined, lat: f.sortBy === 'distance' ? 28.6315 : undefined, lon: f.sortBy === 'distance' ? 77.2167 : undefined }), [JSON.stringify(f)]);
  useLiveFeed(() => list.reload({ silent: true }), ['resource.', 'deployment.'], 700);

  const create = (body) => run(async () => {
    try { const r = await api.createResource(body); toast.success(`${r.code} registered`); setCreating(false); list.reload({ silent: true }); }
    catch (e) { toast.fromError(e, 'Registration rejected'); }
  });

  const columns = [
    { key: 'code', label: 'Unit', render: (r) => <div className="row" style={{ gap: 10 }}><span className="legend-dot" style={{ background: CATEGORY_COLORS[r.category] }} /><div><div className="cell-main mono">{r.code}</div><div className="cell-sub">{r.name}</div></div></div> },
    { key: 'category', label: 'Category', render: (r) => <span className="muted">{CATEGORY_LABEL[r.category]}</span> },
    { key: 'status', label: 'Status', render: (r) => <StatusBadge status={r.status} /> },
    { key: 'units', label: 'Available / capacity', render: (r) => <div style={{ minWidth: 120 }}><div className="mono" style={{ fontSize: 12 }}>{r.available} / {r.capacity}</div><ScoreBar value={r.available} max={r.capacity} color="var(--green)" /></div> },
    { key: 'caps', label: 'Capabilities', render: (r) => <Chips items={r.capabilities} highlight={f.capability ? [f.capability] : []} /> },
    { key: 'org', label: 'Organisation', render: (r) => <span className="faint">{r.organization}</span> },
  ];

  return (
    <>
      <PageHeader kicker="Module 3" icon={Ambulance} title="Resource Management"
        subtitle="Inventory of vehicles, teams, supplies and shelters. Registry = array + hash index + AVL; codes in a double-hashing HashTable; names in a Trie; positions in KD/Quad trees.">
        <button className="btn primary" disabled={!can('manage_resources')} onClick={() => setCreating(true)}><Plus size={15} />Register resource</button>
      </PageHeader>

      <Panel title="Filter & rank" icon={Search}>
        <div className="toolbar">
          <Field label="Category"><Select value={f.category} onChange={set('category')} options={[{ value: '', label: 'All' }, ...Object.entries(CATEGORY_LABEL).map(([v, l]) => ({ value: v, label: l }))]} /></Field>
          <Field label="Status"><Select value={f.status} onChange={set('status')} options={[{ value: '', label: 'Any' }, 'AVAILABLE', 'DEPLOYED', 'MAINTENANCE', 'INACTIVE']} /></Field>
          <Field label="Capability"><Select value={f.capability} onChange={set('capability')} options={[{ value: '', label: 'Any' }, ...CAPABILITIES]} /></Field>
          <Field label="Prefix (Trie)"><TextInput value={f.prefix} onChange={set('prefix')} placeholder="ALS, noida…" /></Field>
          <Field label="Sort by"><Select value={f.sortBy} onChange={set('sortBy')} options={SORTS} /></Field>
          <Field label="Algorithm"><Select value={f.algorithm} onChange={set('algorithm')} options={ALGOS} /></Field>
          <Checkbox checked={f.availableOnly} onChange={set('availableOnly')} label="Available only" />
        </div>
      </Panel>
      <div style={{ height: 16 }} />
      <Panel title="Inventory" icon={Ambulance} bodyClass="flush"
        hint={list.data ? `${list.data.count} units · ${list.data.stats.index} · ${list.data.stats.sortAlgorithm} · ${num(list.data.stats.comparisons)} comparisons` : ''}>
        <Async state={list} label="Loading inventory">
          {(d) => <DataTable columns={columns} rows={d.items} onRowClick={(r) => nav(`/resources/${r.id}`)} maxHeight="calc(100vh - 400px)" />}
        </Async>
      </Panel>

      <Modal open={creating} onClose={() => setCreating(false)} title="Register resource" icon={Ambulance} wide>
        <ResourceForm onSubmit={create} busy={busy} />
      </Modal>
    </>
  );
}
