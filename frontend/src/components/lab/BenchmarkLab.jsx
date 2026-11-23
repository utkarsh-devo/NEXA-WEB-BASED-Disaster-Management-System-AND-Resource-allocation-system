import { useState } from 'react';
import { Gauge } from 'lucide-react';
import { api } from '../../services/api';
import { useToast } from '../../hooks/useToast';
import { Field, NumberInput, Select } from '../Form';
import DataTable from '../DataTable';
import { BenchmarkChart } from '../../charts/Charts';
import { num } from '../../services/format';

const SUITES = [{ value: 'search', label: 'Linear vs binary vs hash search' }, { value: 'bst_avl', label: 'BST vs AVL (random & sorted)' }, { value: 'sorting', label: 'Sorting algorithms' },
  { value: 'hashing', label: 'Hash collision strategies' }, { value: 'graph_repr', label: 'Adjacency list vs matrix' }, { value: 'spatial', label: 'Linear vs KD-tree nearest' }];

export default function BenchmarkLab() {
  const toast = useToast();
  const [suite, setSuite] = useState('sorting');
  const [n, setN] = useState(5000);
  const [repeats, setRepeats] = useState(5);
  const [metric, setMetric] = useState('microseconds');
  const [rows, setRows] = useState(null);
  const [busy, setBusy] = useState(false);
  const run = async () => { setBusy(true); try { setRows(await api.benchmark(suite, n, repeats)); } catch (e) { toast.fromError(e); } finally { setBusy(false); } };
  const columns = [
    { key: 'variant', label: 'Variant', render: (r) => <b>{r.variant}</b> },
    { key: 'n', label: 'n', className: 'num', align: 'right', render: (r) => num(r.n) },
    { key: 'us', label: 'Median µs', className: 'num', align: 'right', render: (r) => <span style={{ color: 'var(--amber)' }}>{num(r.microseconds, 1)}</span> },
    { key: 'cmp', label: 'Comparisons', className: 'num', align: 'right', render: (r) => num(r.comparisons) },
    { key: 'ops', label: 'Other ops', className: 'num', align: 'right', render: (r) => num(r.operations) },
    { key: 'mem', label: 'Memory (est.)', className: 'num', align: 'right', render: (r) => (r.memoryBytes ? `${num(r.memoryBytes / 1024, 1)} KB` : '—') },
    { key: 'note', label: 'Note', render: (r) => <span className="faint">{r.note}</span> },
  ];
  return (
    <div className="stack" style={{ gap: 16 }}>
      <div className="toolbar">
        <Field label="Suite"><Select value={suite} onChange={setSuite} options={SUITES} /></Field>
        <Field label="Dataset size n"><NumberInput value={n} onChange={setN} min={10} max={50000} /></Field>
        <Field label="Repeats (median)"><NumberInput value={repeats} onChange={setRepeats} min={1} max={15} /></Field>
        <Field label="Chart metric"><Select value={metric} onChange={setMetric} options={[{ value: 'microseconds', label: 'Time (µs)' }, { value: 'comparisons', label: 'Comparisons' }, { value: 'operations', label: 'Operations' }, { value: 'memoryBytes', label: 'Memory' }]} /></Field>
        <button className="btn primary" disabled={busy} onClick={run}><Gauge size={14} />{busy ? 'Running in C++…' : 'Run benchmark'}</button>
      </div>
      <div className="note">Controlled, seeded datasets; each variant runs {repeats}× and the <b>median</b> time is reported to reduce noise. Timings depend on your machine and build type (use a Release build).</div>
      {rows && <><BenchmarkChart rows={rows} metric={metric} /><DataTable columns={columns} rows={rows} rowKey={(r) => r.variant} /></>}
    </div>
  );
}
