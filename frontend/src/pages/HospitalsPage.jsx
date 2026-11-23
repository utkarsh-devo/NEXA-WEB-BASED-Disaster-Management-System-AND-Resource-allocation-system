import { useMemo, useState } from 'react';
import { motion } from 'framer-motion';
import { BedDouble, Hospital as HospitalIcon, Plus, Stethoscope } from 'lucide-react';
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
import { Field, NumberInput, Select, TextInput } from '../components/Form';
import { Chips, ScoreBar } from '../components/Badges';
import NexaMap from '../maps/NexaMap';
import { clock } from '../services/format';

const CAPS = ['trauma', 'icu', 'burn', 'pediatric', 'cardiac', 'neuro', 'general'];

function HospitalForm({ onSubmit, busy }) {
  const [f, setF] = useState({ name: '', category: 'general', lat: '', lon: '', totalBeds: 100, icuBeds: 10, capabilities: 'general', contact: '', address: '' });
  const set = (k) => (v) => setF((x) => ({ ...x, [k]: v }));
  return (
    <div className="grid g-2">
      <div className="form-grid">
        <Field label="Name" full><TextInput value={f.name} onChange={set('name')} /></Field>
        <Field label="Category"><Select value={f.category} onChange={set('category')} options={['general', 'trauma', 'burn', 'pediatric', 'multispecialty']} /></Field>
        <Field label="Capabilities (comma)"><TextInput value={f.capabilities} onChange={set('capabilities')} /></Field>
        <Field label="Total beds"><NumberInput value={f.totalBeds} onChange={set('totalBeds')} min={0} /></Field>
        <Field label="ICU beds"><NumberInput value={f.icuBeds} onChange={set('icuBeds')} min={0} /></Field>
        <Field label="Latitude"><NumberInput step="0.0001" value={f.lat} onChange={set('lat')} /></Field>
        <Field label="Longitude"><NumberInput step="0.0001" value={f.lon} onChange={set('lon')} /></Field>
        <Field label="Contact" full><TextInput value={f.contact} onChange={set('contact')} /></Field>
        <div className="full row" style={{ justifyContent: 'flex-end' }}><button className="btn primary" disabled={busy} onClick={() => onSubmit({ ...f, lat: Number(f.lat), lon: Number(f.lon) })}>Register hospital</button></div>
      </div>
      <NexaMap className="short" hospitals={f.lat !== '' ? [{ id: 0, name: f.name, lat: Number(f.lat), lon: Number(f.lon), category: f.category, totalBeds: f.totalBeds, availableBeds: f.totalBeds, icuBeds: f.icuBeds, icuAvailable: f.icuBeds, capabilities: [] }] : []}
        onMapClick={(lat, lon) => setF((x) => ({ ...x, lat: Number(lat.toFixed(6)), lon: Number(lon.toFixed(6)) }))} />
    </div>
  );
}

export default function HospitalsPage() {
  const toast = useToast();
  const { can } = useAuth();
  const [sortBy, setSortBy] = useState('name');
  const [find, setFind] = useState({ incidentId: '', capability: 'trauma', beds: 2 });
  const [beds, setBeds] = useState(2);
  const [creating, setCreating] = useState(false);
  const [busy, run] = useAction();
  const list = useApi(() => api.hospitals(sortBy), [sortBy]);
  const res = useApi(() => api.reservations(), []);
  const open = useApi(() => api.incidents({ limit: 200 }), []);
  const suitable = useApi(() => api.suitableHospitals({ ...find, limit: 6 }), [JSON.stringify(find)], { enabled: Boolean(find.incidentId) });
  useLiveFeed(() => { list.reload({ silent: true }); res.reload({ silent: true }); suitable.reload({ silent: true }); }, ['hospital.'], 500);

  const act = (fn, ok) => run(async () => { try { await fn(); toast.success(ok); list.reload({ silent: true }); res.reload({ silent: true }); suitable.reload({ silent: true }); } catch (e) { toast.fromError(e); } });
  const incident = open.data?.items?.find((i) => String(i.id) === String(find.incidentId));
  const incidents = useMemo(() => (incident ? [incident] : []), [incident]);
  const highlight = useMemo(() => ({ hospitals: new Set((suitable.data || []).map((s) => s.hospital.id)) }), [suitable.data]);
  const hospById = useMemo(() => new Map((list.data || []).map((h) => [h.id, h])), [list.data]);

  const columns = [
    { key: 'name', label: 'Hospital', render: (h) => <div><div className="cell-main">{h.name}</div><div className="cell-sub">{h.category}</div></div> },
    { key: 'beds', label: 'Beds free', render: (h) => <div style={{ minWidth: 140 }}><div className="mono" style={{ fontSize: 12 }}>{h.availableBeds}/{h.totalBeds}</div><ScoreBar value={h.availableBeds} max={h.totalBeds || 1} color={h.availableBeds / (h.totalBeds || 1) < 0.15 ? 'var(--red)' : 'var(--green)'} /></div> },
    { key: 'icu', label: 'ICU', className: 'num', align: 'right', render: (h) => `${h.icuAvailable}/${h.icuBeds}` },
    { key: 'caps', label: 'Capabilities', render: (h) => <Chips items={h.capabilities} highlight={[find.capability]} /> },
  ];

  return (
    <>
      <PageHeader kicker="Module 10" icon={HospitalIcon} title="Hospital & Medical Coordination"
        subtitle="Hospital registry indexed by a BST (alphabetical in-order traversal), KD-tree for nearest facilities, road graph for accessibility, and per-hospital interval trees for bed-reservation windows. Capacity changes only through reservations and releases.">
        <Field label="Order"><Select value={sortBy} onChange={setSortBy} options={[{ value: 'name', label: 'Name (BST in-order)' }, { value: 'beds', label: 'Free beds (quick sort)' }]} /></Field>
        <button className="btn primary" disabled={!can('manage_hospitals')} onClick={() => setCreating(true)}><Plus size={15} />Add hospital</button>
      </PageHeader>

      <div className="grid g-main">
        <Panel title="Facilities" icon={BedDouble} bodyClass="flush"><Async state={list}>{(rows) => <DataTable columns={columns} rows={rows} maxHeight={440} />}</Async></Panel>
        <Panel title="Find a receiving hospital" icon={Stethoscope} tone="alert">
          <div className="form-grid">
            <Field label="For incident" full><Select value={find.incidentId} onChange={(v) => setFind((f) => ({ ...f, incidentId: v }))} placeholder="Select…" options={(open.data?.items || []).map((i) => ({ value: String(i.id), label: `${i.code} · ${i.title}` }))} /></Field>
            <Field label="Capability"><Select value={find.capability} onChange={(v) => setFind((f) => ({ ...f, capability: v }))} options={[{ value: '', label: 'Any' }, ...CAPS]} /></Field>
            <Field label="Min free beds"><NumberInput value={find.beds} onChange={(v) => setFind((f) => ({ ...f, beds: v || 1 }))} min={1} /></Field>
          </div>
          <div className="divider" />
          {!find.incidentId && <div className="faint">Choose an incident to rank hospitals by road access (60%) and spare capacity (40%).</div>}
          {find.incidentId && <Async state={suitable}>{(rows) => rows.length === 0 ? <div className="empty">No hospital meets the criteria</div> : rows.map((c, i) => (
            <motion.div key={c.hospital.id} initial={{ opacity: 0, y: 6 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: i * 0.05 }} style={{ padding: '9px 0', borderBottom: '1px solid var(--line)' }}>
              <div className="row between"><b>{i + 1}. {c.hospital.name}</b><span className="mono" style={{ color: 'var(--amber)' }}>{c.score.toFixed(1)}</span></div>
              <div className="cell-sub">{c.reason}</div>
              {can('manage_hospitals') && <div className="row" style={{ marginTop: 6 }}>
                <div style={{ width: 80 }}><NumberInput value={beds} onChange={(v) => setBeds(v || 1)} min={1} /></div>
                <button className="btn sm violet" disabled={busy} onClick={() => act(() => api.reserveBeds(c.hospital.id, { incidentId: Number(find.incidentId), beds, hours: 12 }), `${beds} bed(s) reserved at ${c.hospital.name}`)}>Reserve beds (12 h)</button>
              </div>}
            </motion.div>
          ))}</Async>}
        </Panel>
      </div>
      <div style={{ height: 16 }} />
      <div className="grid g-main">
        <Panel title="Medical network map" bodyClass="flush"><NexaMap className="short" hospitals={list.data || []} incidents={incidents} highlight={highlight} dimOthers={Boolean(find.incidentId)} /></Panel>
        <Panel title="Bed reservations (interval windows)" icon={BedDouble} bodyClass="flush scroll">
          <Async state={res}>{(rows) => rows.length === 0 ? <div className="empty">No reservations</div> : [...rows].reverse().map((r) => (
            <div key={r.id} className="row between" style={{ padding: '10px 16px', borderBottom: '1px solid var(--line)', opacity: r.released ? 0.5 : 1 }}>
              <div><div className="cell-main">{hospById.get(r.hospitalId)?.name || `Hospital #${r.hospitalId}`} · {r.beds} bed(s)</div><div className="cell-sub mono">incident #{r.incidentId} · {clock(r.from)} → {clock(r.until)}</div></div>
              {r.released ? <span className="badge b-grey">released</span> : can('manage_hospitals') && <button className="btn sm green" onClick={() => act(() => api.releaseReservation(r.id), 'Beds released')}>Release</button>}
            </div>
          ))}</Async>
        </Panel>
      </div>
      <Modal open={creating} onClose={() => setCreating(false)} title="Register hospital" icon={HospitalIcon} wide>
        <HospitalForm busy={busy} onSubmit={(b) => act(async () => { await api.createHospital(b); setCreating(false); }, 'Hospital registered')} />
      </Modal>
    </>
  );
}
