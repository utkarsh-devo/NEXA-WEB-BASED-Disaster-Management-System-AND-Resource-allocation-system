import { useEffect, useState } from 'react';
import { api } from '../services/api';
import { ChipPicker, Field, NumberInput, Select, TextInput } from './Form';
import { CAPABILITIES } from './IncidentForm';
import NexaMap from '../maps/NexaMap';

const blank = { code: '', name: '', typeId: '', capacity: 1, lat: '', lon: '', capabilities: [], organization: '', status: 'AVAILABLE' };

export default function ResourceForm({ initial, onSubmit, busy, submitLabel = 'Register resource' }) {
  const [form, setForm] = useState(() => ({ ...blank, ...(initial || {}) }));
  const [types, setTypes] = useState([]);
  useEffect(() => { api.resourceTypes().then(setTypes).catch(() => {}); }, []);
  const set = (k) => (v) => setForm((f) => ({ ...f, [k]: v }));
  const pickType = (v) => {
    const t = types.find((x) => String(x.id) === v);
    setForm((f) => ({ ...f, typeId: v, capabilities: f.capabilities.length ? f.capabilities : (t ? t.defaultCapabilities : []) }));
  };
  const point = form.lat !== '' && form.lon !== '' ? [{ id: 0, lat: Number(form.lat), lon: Number(form.lon), code: form.code || 'NEW', name: form.name, status: 'AVAILABLE', category: types.find((t) => String(t.id) === String(form.typeId))?.category, available: form.capacity, capacity: form.capacity }] : [];

  return (
    <form className="grid g-2" style={{ alignItems: 'start' }} onSubmit={(e) => { e.preventDefault(); onSubmit({ ...form, typeId: Number(form.typeId), capacity: Number(form.capacity), lat: Number(form.lat), lon: Number(form.lon) }); }}>
      <div className="form-grid">
        <Field label="Code" hint="Unique — indexed in a HashTable"><TextInput mono value={form.code} onChange={set('code')} placeholder="ALS-CP201" /></Field>
        <Field label="Type"><Select value={String(form.typeId)} onChange={pickType} placeholder="Select…" options={types.map((t) => ({ value: String(t.id), label: `${t.name} (${t.category})` }))} /></Field>
        <Field label="Name" full><TextInput value={form.name} onChange={set('name')} /></Field>
        <Field label="Capacity (units)"><NumberInput value={form.capacity} onChange={set('capacity')} min={1} /></Field>
        <Field label="Status"><Select value={form.status} onChange={set('status')} options={['AVAILABLE', 'MAINTENANCE', 'INACTIVE']} /></Field>
        <Field label="Organisation / team" full><TextInput value={form.organization} onChange={set('organization')} /></Field>
        <Field label="Capabilities" full><ChipPicker options={CAPABILITIES} value={form.capabilities} onChange={set('capabilities')} /></Field>
        <Field label="Latitude"><NumberInput step="0.0001" value={form.lat} onChange={set('lat')} /></Field>
        <Field label="Longitude"><NumberInput step="0.0001" value={form.lon} onChange={set('lon')} /></Field>
        <div className="full row" style={{ justifyContent: 'flex-end' }}><button className="btn primary" disabled={busy}>{busy ? 'Saving…' : submitLabel}</button></div>
      </div>
      <NexaMap className="short" resources={point} onMapClick={(lat, lon) => setForm((f) => ({ ...f, lat: Number(lat.toFixed(6)), lon: Number(lon.toFixed(6)) }))} />
    </form>
  );
}
