import { useState } from 'react';
import { motion } from 'framer-motion';
import { BarChart3, Download, FileUp, Hospital, Timer, TrendingUp } from 'lucide-react';
import { Bar, BarChart, CartesianGrid, Cell, ResponsiveContainer, Tooltip, XAxis, YAxis } from 'recharts';
import { api } from '../services/api';
import { useApi, useAction } from '../hooks/useApi';
import { useToast } from '../hooks/useToast';
import { useAuth } from '../hooks/useAuth';
import PageHeader from '../components/PageHeader';
import Panel from '../components/Panel';
import { Async } from '../components/Feedback';
import { Field, Select, TextArea } from '../components/Form';
import { CategoryDonut, TimelineChart } from '../charts/Charts';
import { PALETTE, axis, grid, tooltip } from '../charts/chartTheme';
import { STATUS_BADGE } from '../services/format';

const STATUS_COLORS = { PENDING: '#ff3d57', ACTIVE: '#ffb627', CONTAINED: '#2de2e6', RESOLVED: '#3ddc97', ARCHIVED: '#5d6b7f' };

export default function AnalyticsPage() {
  const toast = useToast();
  const { can } = useAuth();
  const [days, setDays] = useState(2);
  const [bucketHours, setBucketHours] = useState(2);
  const [range, setRange] = useState({ from: -1, to: -1 });
  const [csv, setCsv] = useState('');
  const [entity, setEntity] = useState('resources');
  const [busy, run] = useAction();
  const tl = useApi(() => api.timeline({ days, bucketHours, from: range.from, to: range.to }), [days, bucketHours, range.from, range.to]);
  const resp = useApi(() => api.responseStats(), []);
  const cats = useApi(() => api.categoryStats(true), []);
  const hosp = useApi(() => api.hospitalStats(), []);
  const stat = useApi(() => api.statusStats(), []);

  const onBucket = (i) => setRange((r) => (r.from < 0 || r.to >= 0 ? { from: i, to: -1 } : { from: Math.min(r.from, i), to: Math.max(r.from, i) }));
  const download = async (e) => {
    try {
      const res = await fetch(api.exportUrl(e));
      if (!res.ok) throw new Error(`Export failed (${res.status})`);
      const blob = await res.blob();
      const a = document.createElement('a'); a.href = URL.createObjectURL(blob); a.download = `nexa-${e}.csv`; a.click(); URL.revokeObjectURL(a.href);
    } catch (err) { toast.fromError(err); }
  };
  const doImport = () => run(async () => {
    try { const r = await api.importCsv(entity, csv); toast.success(`Imported ${r.imported} ${entity}`, r.failed ? `${r.failed} row(s) rejected: ${r.errors.slice(0, 2).join(' · ')}` : 'All rows valid'); }
    catch (e) { toast.fromError(e, 'Import failed'); }
  });

  return (
    <>
      <PageHeader kicker="Module 11" icon={BarChart3} title="Operational Analytics & Reports"
        subtitle="Computed from stored records: the timeline is a segment tree (range sum / max in O(log n)), response times use median-of-medians selection and quick-select for P90, categories roll up the K-ary taxonomy.">
        {['incidents', 'resources', 'deployments', 'hospitals'].map((e) => <button key={e} className="btn sm" onClick={() => download(e)}><Download size={13} />{e}.csv</button>)}
      </PageHeader>

      <Panel title="Activity timeline (segment tree)" icon={TrendingUp} hint="click two buckets to run a range query"
        actions={<div className="row">
          <Select value={String(days)} onChange={(v) => { setDays(Number(v)); setRange({ from: -1, to: -1 }); }} options={[{ value: '1', label: '24 h' }, { value: '2', label: '48 h' }, { value: '7', label: '7 days' }, { value: '30', label: '30 days' }]} />
          <Select value={String(bucketHours)} onChange={(v) => { setBucketHours(Number(v)); setRange({ from: -1, to: -1 }); }} options={[{ value: '1', label: '1 h buckets' }, { value: '2', label: '2 h' }, { value: '6', label: '6 h' }, { value: '24', label: '1 day' }]} />
        </div>}>
        <Async state={tl}>
          {(t) => (
            <>
              <TimelineChart buckets={t.buckets} onBrush={onBucket} />
              <motion.div key={`${t.rangeQuery.fromBucket}-${t.rangeQuery.toBucket}`} initial={{ opacity: 0 }} animate={{ opacity: 1 }} className="row wrap" style={{ gap: 10, marginTop: 10 }}>
                <span className="chip on">range [{t.rangeQuery.fromBucket} … {t.rangeQuery.toBucket}]</span>
                <span className="chip">Σ incidents = {t.rangeQuery.incidents}</span>
                <span className="chip">Σ deployments = {t.rangeQuery.deployments}</span>
                <span className="chip">peak / bucket = {t.rangeQuery.peakIncidents}</span>
                {(range.from >= 0) && <button className="btn sm ghost" onClick={() => setRange({ from: -1, to: -1 })}>reset range</button>}
              </motion.div>
            </>
          )}
        </Async>
      </Panel>

      <div style={{ height: 16 }} />
      <div className="grid g-3">
        <Panel title="Response time" icon={Timer}>
          <Async state={resp}>
            {(r) => (
              <div className="grid g-2" style={{ gap: 14 }}>
                {[['Average', r.averageMinutes], ['Median', r.medianMinutes], ['P90', r.p90Minutes], ['Fastest', r.fastestMinutes]].map(([l, v]) => (
                  <div key={l}><div className="faint" style={{ fontSize: 11, letterSpacing: '.14em' }}>{l.toUpperCase()}</div><div className="mono" style={{ fontSize: 24, color: 'var(--cyan)' }}>{v.toFixed(1)}<span className="faint" style={{ fontSize: 12 }}> min</span></div></div>
                ))}
                <div className="span-2 faint" style={{ fontSize: 12 }}>{r.samples} incidents with a dispatch · {r.resolvedCount} resolved · mean resolution {r.averageResolutionHours.toFixed(1)} h</div>
              </div>
            )}
          </Async>
        </Panel>
        <Panel title="Incidents by category (all time)" icon={BarChart3}><Async state={cats}>{(c) => <CategoryDonut data={c} />}</Async></Panel>
        <Panel title="Status distribution" icon={BarChart3}>
          <Async state={stat}>
            {(s) => (
              <ResponsiveContainer width="100%" height={220}>
                <BarChart data={s.map((x) => ({ name: x.label, value: x.count }))} margin={{ left: -18, right: 6, top: 10 }}>
                  <CartesianGrid {...grid} vertical={false} /><XAxis dataKey="name" {...axis} /><YAxis allowDecimals={false} {...axis} /><Tooltip {...tooltip} />
                  <Bar dataKey="value" radius={[5, 5, 0, 0]}>{s.map((x) => <Cell key={x.label} fill={STATUS_COLORS[x.label] || PALETTE[0]} />)}</Bar>
                </BarChart>
              </ResponsiveContainer>
            )}
          </Async>
        </Panel>
      </div>

      <div style={{ height: 16 }} />
      <div className="grid g-main">
        <Panel title="Hospital occupancy" icon={Hospital}>
          <Async state={hosp}>
            {(h) => (
              <ResponsiveContainer width="100%" height={Math.max(220, h.length * 26)}>
                <BarChart data={h.map((x) => ({ name: x.label, pct: x.value }))} layout="vertical" margin={{ left: 40, right: 20 }}>
                  <CartesianGrid {...grid} horizontal={false} /><XAxis type="number" domain={[0, 100]} unit="%" {...axis} /><YAxis type="category" dataKey="name" width={200} {...axis} /><Tooltip {...tooltip} />
                  <Bar dataKey="pct" name="Occupancy %" radius={[0, 5, 5, 0]}>{h.map((x) => <Cell key={x.label} fill={x.value > 85 ? '#ff3d57' : x.value > 70 ? '#ffb627' : '#3ddc97'} />)}</Bar>
                </BarChart>
              </ResponsiveContainer>
            )}
          </Async>
        </Panel>
        <Panel title="CSV import" icon={FileUp} tone="violet">
          <div className="stack">
            <Field label="Entity"><Select value={entity} onChange={setEntity} options={['resources', 'incidents']} /></Field>
            <Field label="CSV (header row required)" hint={entity === 'resources' ? 'code,name,type_id,capacity,lat,lon,capabilities,organization' : 'title,description,category_id,severity,lat,lon,location,affected_people,required_capabilities,required_units'}>
              <TextArea value={csv} onChange={setCsv} style={{ minHeight: 140, fontFamily: 'var(--font-mono)', fontSize: 12 }} />
            </Field>
            <input type="file" accept=".csv,text/csv" onChange={(e) => { const f = e.target.files?.[0]; if (f) f.text().then(setCsv); }} />
            <button className="btn violet" disabled={busy || !csv.trim() || !can(entity === 'incidents' ? 'manage_incidents' : 'manage_resources')} onClick={doImport}><FileUp size={14} />Validate & import</button>
            <div className="faint" style={{ fontSize: 11 }}>Each row passes through the same validation as the forms; invalid rows are reported, not silently dropped. Statuses: {Object.keys(STATUS_BADGE).slice(0, 5).join(', ')}.</div>
          </div>
        </Panel>
      </div>
    </>
  );
}
