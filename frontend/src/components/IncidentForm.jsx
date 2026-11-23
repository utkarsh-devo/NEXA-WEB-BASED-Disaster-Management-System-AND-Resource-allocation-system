import { useEffect, useMemo, useState } from 'react';
import { MapPin } from 'lucide-react';
import { api } from '../services/api';
import { ChipPicker, Field, NumberInput, Select, TextArea, TextInput } from './Form';
import NexaMap from '../maps/NexaMap';

export const CAPABILITIES = ['rescue', 'heavy_lifting', 'medical', 'trauma', 'transport', 'fire_suppression', 'hazmat', 'evacuation', 'water_rescue',
  'extrication', 'shelter', 'food_water', 'water_supply', 'crowd_control', 'decontamination'];

const blank = { title: '', description: '', categoryId: '', severity: 3, lat: '', lon: '', locationName: '', affectedPeople: 0, requiredCapabilities: [], requiredUnits: 1, reporterContact: '' };

/** Create / edit incident. Clicking the map sets the coordinates. */
export default function IncidentForm({ initial, onSubmit, busy, submitLabel = 'Register incident' }) {
  const [form, setForm] = useState(() => ({ ...blank, ...(initial || {}) }));
  const [cats, setCats] = useState([]);
  const [errors, setErrors] = useState({});
  useEffect(() => { api.categories().then(setCats).catch(() => {}); }, []);
  const set = (k) => (v) => setForm((f) => ({ ...f, [k]: v }));

  const leafCats = useMemo(() => cats.filter((c) => c.depth >= 2).map((c) => {
    const parent = cats.find((p) => p.id === c.parentId);
    return { value: String(c.id), label: `${parent ? parent.name + ' › ' : ''}${c.name}` };
  }), [cats]);

  const validate = () => {
    const e = {};
    if (!form.title.trim()) e.title = 'Title is required';
    if (!form.categoryId) e.categoryId = 'Choose a category';
    if (form.lat === '' || form.lon === '') e.location = 'Click the map or enter coordinates';
    if (form.severity < 1 || form.severity > 5) e.severity = '1 – 5';
    setErrors(e);
    return Object.keys(e).length === 0;
  };

  const submit = (ev) => {
    ev.preventDefault();
    if (!validate()) return;
    onSubmit({ ...form, categoryId: Number(form.categoryId), severity: Number(form.severity), lat: Number(form.lat), lon: Number(form.lon),
      affectedPeople: Number(form.affectedPeople) || 0, requiredUnits: Number(form.requiredUnits) || 1 });
  };

  const point = form.lat !== '' && form.lon !== '' ? [{ id: 0, lat: Number(form.lat), lon: Number(form.lon), severity: Number(form.severity), title: form.title || 'New incident', code: 'NEW', status: 'PENDING', priorityScore: 0, assignedUnits: 0, requiredUnits: form.requiredUnits }] : [];

  return (
    <form onSubmit={submit} className="grid g-2" style={{ alignItems: 'start' }}>
      <div className="form-grid">
        <Field label="Title" error={errors.title} full><TextInput value={form.title} onChange={set('title')} placeholder="e.g. Building collapse near market" /></Field>
        <Field label="Category" error={errors.categoryId}><Select value={String(form.categoryId)} onChange={set('categoryId')} options={leafCats} placeholder="Select…" /></Field>
        <Field label={`Severity · ${form.severity}`} error={errors.severity}>
          <input type="range" min={1} max={5} className="range" value={form.severity} onChange={(e) => set('severity')(Number(e.target.value))} />
        </Field>
        <Field label="Description" full><TextArea value={form.description} onChange={set('description')} placeholder="What is happening? Hazards, access, trapped people…" /></Field>
        <Field label="Affected people"><NumberInput value={form.affectedPeople} onChange={set('affectedPeople')} min={0} /></Field>
        <Field label="Required units"><NumberInput value={form.requiredUnits} onChange={set('requiredUnits')} min={1} max={200} /></Field>
        <Field label="Required capabilities" hint="Leave empty to use the category defaults" full>
          <ChipPicker options={CAPABILITIES} value={form.requiredCapabilities} onChange={set('requiredCapabilities')} />
        </Field>
        <Field label="Locality / landmark"><TextInput value={form.locationName} onChange={set('locationName')} /></Field>
        <Field label="Reporter contact"><TextInput value={form.reporterContact} onChange={set('reporterContact')} /></Field>
        <Field label="Latitude" error={errors.location}><NumberInput step="0.0001" value={form.lat} onChange={set('lat')} /></Field>
        <Field label="Longitude"><NumberInput step="0.0001" value={form.lon} onChange={set('lon')} /></Field>
        <div className="full row" style={{ justifyContent: 'flex-end', marginTop: 6 }}>
          <button className="btn primary" disabled={busy}>{busy ? 'Submitting…' : submitLabel}</button>
        </div>
      </div>
      <div>
        <div className="note cyan" style={{ marginBottom: 10 }}><MapPin size={13} /> Click on the map to drop the incident location.</div>
        <NexaMap className="short" incidents={point} onMapClick={(lat, lon) => setForm((f) => ({ ...f, lat: Number(lat.toFixed(6)), lon: Number(lon.toFixed(6)) }))}
          center={point.length ? { lat: point[0].lat, lon: point[0].lon } : undefined} zoom={point.length ? 12 : 10} />
      </div>
    </form>
  );
}
