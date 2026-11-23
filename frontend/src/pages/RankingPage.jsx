import { useEffect, useMemo, useState } from 'react';
import { useNavigate, useSearchParams } from 'react-router-dom';
import { motion } from 'framer-motion';
import { Hash, Search, Trophy } from 'lucide-react';
import { api } from '../services/api';
import { useApi } from '../hooks/useApi';
import { useToast } from '../hooks/useToast';
import PageHeader from '../components/PageHeader';
import Panel from '../components/Panel';
import { Async } from '../components/Feedback';
import { Field, Select, TextInput } from '../components/Form';
import { Chips, ScoreBar } from '../components/Badges';
import NexaMap from '../maps/NexaMap';
import { CATEGORY_LABEL, km, mins, num } from '../services/format';

const COMPONENTS = [['capability', 'var(--amber)'], ['proximity', 'var(--cyan)'], ['availability', 'var(--green)'], ['capacity', 'var(--violet)']];

function LookupBench() {
  const toast = useToast();
  const [field, setField] = useState('code');
  const [value, setValue] = useState('');
  const [rows, setRows] = useState(null);
  const run = async () => {
    try {
      const methods = field === 'category' || field === 'capability' ? ['linear'] : ['linear', 'binary', 'hash'];
      const res = await Promise.all(methods.map((m) => api.lookupResources(field, value, m)));
      setRows(res);
    } catch (e) { toast.fromError(e); }
  };
  return (
    <Panel title="Lookup: linear vs binary vs hash" icon={Hash} tone="cyan">
      <div className="toolbar">
        <Field label="Field"><Select value={field} onChange={setField} options={['code', 'id', 'category', 'capability']} /></Field>
        <Field label="Value"><TextInput value={value} onChange={setValue} placeholder={field === 'code' ? 'ALS-AI109' : field === 'id' ? '12' : field === 'category' ? 'ambulance' : 'hazmat'} /></Field>
        <button className="btn cyan" onClick={run} disabled={!value}><Search size={14} />Run</button>
      </div>
      {rows && (
        <div className="stack" style={{ marginTop: 14 }}>
          {rows.map((r) => (
            <motion.div key={r.method} initial={{ opacity: 0, y: 6 }} animate={{ opacity: 1, y: 0 }} className="row between" style={{ padding: '8px 0', borderBottom: '1px solid var(--line)' }}>
              <span className="mono" style={{ width: 130 }}>{r.method}</span>
              <span className="muted" style={{ flex: 1 }}>{r.found ? `${r.results.length} match(es): ${r.results.slice(0, 3).map((x) => x.code).join(', ')}` : 'not found'}</span>
              <span className="mono" style={{ color: 'var(--amber)', width: 140, textAlign: 'right' }}>{num(r.comparisons)} comparisons</span>
              <span className="mono faint" style={{ width: 90, textAlign: 'right' }}>{r.microseconds.toFixed(1)} µs</span>
            </motion.div>
          ))}
          {rows.find((r) => r.note) && <div className="note cyan">{rows.find((r) => r.note).note}</div>}
        </div>
      )}
    </Panel>
  );
}

export default function RankingPage() {
  const nav = useNavigate();
  const [params, setParams] = useSearchParams();
  const [incidentId, setIncidentId] = useState(params.get('incident') || '');
  const [opts, setOpts] = useState({ algorithm: 'merge', category: '', limit: 10 });
  const open = useApi(() => api.incidents({ limit: 200 }), []);
  useEffect(() => { if (!incidentId && open.data?.items?.length) setIncidentId(String(open.data.items[0].id)); }, [open.data, incidentId]);
  const rank = useApi(() => api.ranking(incidentId, opts), [incidentId, JSON.stringify(opts)], { enabled: Boolean(incidentId) });

  const incident = open.data?.items?.find((i) => String(i.id) === String(incidentId));
  const resources = useMemo(() => (rank.data?.candidates || []).map((c) => ({ id: c.resourceId, code: c.code, name: c.name, lat: c.lat, lon: c.lon, status: 'AVAILABLE', category: c.category, available: c.available, capacity: c.capacity, organization: `score ${c.score.toFixed(1)}` })), [rank.data]);
  const incidents = useMemo(() => (incident ? [incident] : []), [incident]);
  const fitTo = useMemo(() => (incident ? [incident, ...resources.slice(0, 5)] : null), [incident, resources]);
  const highlight = useMemo(() => ({ resources: new Set(resources.slice(0, 3).map((r) => r.id)) }), [resources]);

  return (
    <>
      <PageHeader kicker="Module 6" icon={Trophy} title="Resource Search & Ranking"
        subtitle="KD-tree pre-selection → hard filters → one Dijkstra from the incident for road distance → weighted suitability score → sorting. The nearest unit is not automatically the best one.">
        {incident && <button className="btn primary" onClick={() => nav(`/allocation?incident=${incident.id}`)}>Allocate for {incident.code}</button>}
      </PageHeader>
      <Panel title="Ranking query" icon={Search}>
        <div className="toolbar">
          <Field label="Incident"><Select value={incidentId} onChange={(v) => { setIncidentId(v); setParams({ incident: v }); }} options={(open.data?.items || []).map((i) => ({ value: String(i.id), label: `${i.code} · ${i.title}` }))} /></Field>
          <Field label="Resource category"><Select value={opts.category} onChange={(v) => setOpts((o) => ({ ...o, category: v }))} options={[{ value: '', label: 'Any' }, ...Object.entries(CATEGORY_LABEL).map(([v, l]) => ({ value: v, label: l }))]} /></Field>
          <Field label="Sorting algorithm"><Select value={opts.algorithm} onChange={(v) => setOpts((o) => ({ ...o, algorithm: v }))} options={['merge', 'quick', 'heap', 'insertion']} /></Field>
          <Field label="Show top"><Select value={String(opts.limit)} onChange={(v) => setOpts((o) => ({ ...o, limit: Number(v) }))} options={['5', '10', '20']} /></Field>
        </div>
      </Panel>
      <div style={{ height: 16 }} />
      <Async state={rank} label="Scoring candidates">
        {(r) => (
          <div className="grid g-main">
            <Panel title={`Ranked candidates for ${incident?.code || ''}`} icon={Trophy}
              hint={`${r.considered} examined · ${r.rejectedUnavailable} unavailable · ${r.rejectedCapability} lacking capability · ${r.algorithm} sort · ${num(r.comparisons)} cmp · median road ${km(r.medianRoadKm)}`} bodyClass="flush">
              {r.candidates.length === 0 && <div className="empty">No resource passes the hard filters for this incident.</div>}
              {r.candidates.map((c, i) => (
                <motion.div key={c.resourceId} layout initial={{ opacity: 0, x: -12 }} animate={{ opacity: 1, x: 0 }} transition={{ delay: i * 0.04 }}
                  style={{ padding: '13px 16px', borderBottom: '1px solid var(--line)' }}>
                  <div className="row between">
                    <div className="row" style={{ gap: 12 }}>
                      <span className="mono" style={{ fontSize: 20, width: 28, color: i === 0 ? 'var(--amber)' : 'var(--text-faint)' }}>{i + 1}</span>
                      <div><div className="cell-main mono">{c.code}</div><div className="cell-sub">{c.name} · {CATEGORY_LABEL[c.category]}</div></div>
                    </div>
                    <div className="row" style={{ gap: 18 }}>
                      <div className="mono faint" style={{ textAlign: 'right', fontSize: 12 }}>{km(c.roadKm)} {c.routable ? 'road' : 'est.'}<br />ETA {mins(c.etaMinutes)}</div>
                      <div className="mono" style={{ fontSize: 22, fontWeight: 700, color: 'var(--amber)', width: 64, textAlign: 'right' }}>{c.score.toFixed(1)}</div>
                    </div>
                  </div>
                  <div className="grid g-4" style={{ marginTop: 10, gap: 10 }}>
                    {COMPONENTS.map(([k, color]) => (
                      <div key={k}><div className="row between faint" style={{ fontSize: 10, letterSpacing: '.1em', textTransform: 'uppercase' }}><span>{k}</span><span className="mono">{(c.components[k] * 100).toFixed(0)}%</span></div><ScoreBar value={c.components[k] * 100} color={color} /></div>
                    ))}
                  </div>
                  <div className="row between" style={{ marginTop: 8 }}><Chips items={c.matched} highlight={c.matched} missing={c.missing} /><span className="faint" style={{ fontSize: 12 }}>{c.available}/{c.capacity} units</span></div>
                  <div className="cell-sub" style={{ marginTop: 6 }}>{c.reason}</div>
                </motion.div>
              ))}
            </Panel>
            <div className="stack" style={{ gap: 16 }}>
              <Panel title="Candidates on the map" bodyClass="flush"><NexaMap className="short" incidents={incidents} resources={resources} highlight={highlight} fitTo={fitTo} /></Panel>
              <Panel title="Suitability formula" tone="cyan">
                <div className="formula">{r.formula}</div>
                <div className="grid g-2" style={{ marginTop: 10, fontSize: 12 }}>
                  {COMPONENTS.map(([k, color]) => <div key={k} className="row between"><span className="row" style={{ gap: 6 }}><span className="legend-dot" style={{ background: color }} />{k}</span><span className="mono">w = {r.weights[k]}</span></div>)}
                </div>
                <div className="note" style={{ marginTop: 10 }}>Required: <Chips items={r.requiredCapabilities} /> · units needed: <b className="mono">{r.unitsNeeded}</b> · proximity scale {r.weights.proximityScaleKm} km</div>
              </Panel>
              <LookupBench />
            </div>
          </div>
        )}
      </Async>
    </>
  );
}
