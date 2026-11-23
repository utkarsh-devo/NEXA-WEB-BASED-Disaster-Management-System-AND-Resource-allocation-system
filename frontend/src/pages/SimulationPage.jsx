import { useEffect, useMemo, useRef, useState } from 'react';
import { AnimatePresence, motion } from 'framer-motion';
import { CheckCircle2, FastForward, Pause, Play, Repeat, SkipForward, Waves } from 'lucide-react';
import { api } from '../services/api';
import { useApi, useAction } from '../hooks/useApi';
import { useToast } from '../hooks/useToast';
import { useAuth } from '../hooks/useAuth';
import PageHeader from '../components/PageHeader';
import Panel from '../components/Panel';
import { Field, NumberInput, Select, TextInput } from '../components/Form';
import NexaMap from '../maps/NexaMap';
import { clock } from '../services/format';

const TYPES = [{ value: 'earthquake', label: 'Earthquake' }, { value: 'flood', label: 'Flood' }, { value: 'industrial_fire', label: 'Industrial fire' }, { value: 'cyclone', label: 'Cyclone' }, { value: 'multi', label: 'Multiple simultaneous' }];
const EVENT_COLOR = { SCENARIO: 'var(--violet)', INCIDENT: 'var(--red)', PRIORITY: 'var(--amber)', SELECTION: 'var(--cyan)', ROUTE: 'var(--blue)', DEPLOYMENT: 'var(--green)', RESOLUTION: 'var(--green)' };
const PLACES = [{ value: '28.6315,77.2167', label: 'Connaught Place' }, { value: '28.5708,77.3260', label: 'Noida Sector 18' }, { value: '28.4950,77.0890', label: 'Gurugram Cyber City' }, { value: '28.6304,77.2777', label: 'Laxmi Nagar (Yamuna)' }, { value: '28.5355,77.2750', label: 'Okhla industrial' }];

export default function SimulationPage() {
  const toast = useToast();
  const { can } = useAuth();
  const [form, setForm] = useState({ name: '', type: 'earthquake', place: PLACES[0].value, incidentCount: 6, radiusKm: 6, seed: 2026 });
  const [scenarioId, setScenarioId] = useState('');
  const [busy, run] = useAction();
  const [cursor, setCursor] = useState(0);
  const [playing, setPlaying] = useState(false);
  const timer = useRef(null);
  const list = useApi(() => api.simulations(), []);
  const replay = useApi(() => api.replay(scenarioId), [scenarioId], { enabled: Boolean(scenarioId) });
  useEffect(() => { if (!scenarioId && list.data?.length) setScenarioId(String(list.data[list.data.length - 1].id)); }, [list.data, scenarioId]);
  const events = replay.data?.events || [];
  useEffect(() => setCursor(events.length), [events.length]);

  useEffect(() => {
    clearInterval(timer.current);
    if (!playing) return undefined;
    timer.current = setInterval(() => setCursor((c) => { if (c >= events.length) { setPlaying(false); return c; } return c + 1; }), 700);
    return () => clearInterval(timer.current);
  }, [playing, events.length]);

  const shown = events.slice(0, cursor);
  const markers = useMemo(() => shown.filter((e) => e.type === 'INCIDENT').map((e) => ({ id: e.entityId, lat: e.lat, lon: e.lon, severity: Number((e.summary.match(/severity (\d)/) || [])[1] || 3), title: e.summary, code: `SIM`, status: shown.some((x) => x.type === 'RESOLUTION' && x.entityId === e.entityId) ? 'RESOLVED' : 'PENDING', priorityScore: 0, assignedUnits: 0, requiredUnits: 0, simulated: true })), [shown]);
  const deployed = useMemo(() => shown.filter((e) => e.type === 'SELECTION' && e.lat).map((e) => ({ id: e.entityId, lat: e.lat, lon: e.lon, code: e.summary.split(' ')[0], name: 'selected unit', status: 'DEPLOYED', category: 'ambulance', available: 0, capacity: 1 })), [shown]);
  const fitTo = useMemo(() => (markers.length ? markers : null), [markers.length]); // eslint-disable-line react-hooks/exhaustive-deps

  const create = () => run(async () => {
    const [lat, lon] = form.place.split(',').map(Number);
    try {
      const s = await api.createSimulation({ name: form.name, type: form.type, centerLat: lat, centerLon: lon, incidentCount: form.incidentCount, radiusKm: form.radiusKm, seed: form.seed });
      toast.success('Scenario generated', s.description);
      await list.reload({ silent: true });
      setScenarioId(String(s.id));
    } catch (e) { toast.fromError(e); }
  });
  const step = (fn, label) => run(async () => { try { const r = await fn(Number(scenarioId)); toast[r.acted ? 'success' : 'info'](label, r.message); await replay.reload({ silent: true }); } catch (e) { toast.fromError(e); } });
  const runAll = () => run(async () => {
    try {
      for (let i = 0; i < 20; i++) { const r = await api.stepSimulation(Number(scenarioId)); if (!r.acted) break; }
      toast.success('Response complete', 'All allocatable incidents resourced'); await replay.reload({ silent: true });
    } catch (e) { toast.fromError(e); }
  });

  return (
    <>
      <PageHeader kicker="Advanced · §8.1 / §8.5" icon={Waves} title="Disaster Simulation & Scenario Replay"
        subtitle="Scenarios are generated with a seeded LCG — the same seed reproduces the same incidents. Each step runs the real workflow (priority heap → ranking → Dijkstra → transactional allocation) and is logged for replay. All generated data is flagged SIMULATED." />
      <div className="grid g-side">
        <div className="stack" style={{ gap: 16 }}>
          <Panel title="Generate scenario" icon={Waves} tone="violet">
            <div className="form-grid">
              <Field label="Name" full><TextInput value={form.name} onChange={(v) => setForm((f) => ({ ...f, name: v }))} placeholder="e.g. Monsoon drill" /></Field>
              <Field label="Disaster type"><Select value={form.type} onChange={(v) => setForm((f) => ({ ...f, type: v }))} options={TYPES} /></Field>
              <Field label="Epicentre"><Select value={form.place} onChange={(v) => setForm((f) => ({ ...f, place: v }))} options={PLACES} /></Field>
              <Field label="Incidents"><NumberInput value={form.incidentCount} onChange={(v) => setForm((f) => ({ ...f, incidentCount: v }))} min={1} max={40} /></Field>
              <Field label="Radius km"><NumberInput value={form.radiusKm} onChange={(v) => setForm((f) => ({ ...f, radiusKm: v }))} min={1} max={50} /></Field>
              <Field label="Seed" hint="same seed → same scenario" full><NumberInput value={form.seed} onChange={(v) => setForm((f) => ({ ...f, seed: v }))} /></Field>
            </div>
            <button className="btn violet" style={{ marginTop: 12, width: '100%' }} disabled={busy || !can('run_simulation')} onClick={create}><Play size={15} />Generate</button>
          </Panel>
          <Panel title="Run" icon={FastForward}>
            <Field label="Scenario"><Select value={scenarioId} onChange={setScenarioId} placeholder="Select…" options={(list.data || []).map((s) => ({ value: String(s.id), label: `#${s.id} ${s.name} (${s.type}, seed ${s.seed})` }))} /></Field>
            <div className="row wrap" style={{ marginTop: 12 }}>
              <button className="btn primary" disabled={!scenarioId || busy || !can('run_simulation')} onClick={() => step(api.stepSimulation, 'Dispatch step')}><SkipForward size={14} />Dispatch step</button>
              <button className="btn" disabled={!scenarioId || busy || !can('run_simulation')} onClick={runAll}><FastForward size={14} />Run to completion</button>
              <button className="btn green" disabled={!scenarioId || busy || !can('run_simulation')} onClick={() => step(api.resolveSimulation, 'Resolution step')}><CheckCircle2 size={14} />Resolve next</button>
            </div>
          </Panel>
          <Panel title="Replay" icon={Repeat} hint={`${cursor}/${events.length} events`}>
            <input type="range" className="range" min={0} max={events.length} value={cursor} onChange={(e) => { setPlaying(false); setCursor(Number(e.target.value)); }} />
            <div className="row" style={{ marginTop: 10 }}>
              <button className="btn sm" onClick={() => { setCursor(0); setPlaying(true); }}><Repeat size={13} />Replay from start</button>
              <button className="btn sm" onClick={() => setPlaying((p) => !p)}>{playing ? <Pause size={13} /> : <Play size={13} />}{playing ? 'Pause' : 'Play'}</button>
            </div>
          </Panel>
        </div>
        <div className="stack" style={{ gap: 16 }}>
          <Panel title="Scenario map" bodyClass="flush"><NexaMap className="short" incidents={markers} resources={deployed} fitTo={fitTo} /></Panel>
          <Panel title="Event log (auditable sequence)" icon={Repeat} bodyClass="scroll">
            <div className="timeline">
              <AnimatePresence initial={false}>
                {[...shown].reverse().map((e) => (
                  <motion.div key={e.id || e.seq} className="tl-item" layout initial={{ opacity: 0, x: -14 }} animate={{ opacity: 1, x: 0 }} exit={{ opacity: 0 }}>
                    <div className="tl-meta"><span style={{ color: EVENT_COLOR[e.type] }}>#{e.seq} {e.type}</span> · {clock(e.createdAt)}</div>
                    <div className="tl-msg">{e.summary}</div>
                  </motion.div>
                ))}
              </AnimatePresence>
            </div>
          </Panel>
        </div>
      </div>
    </>
  );
}
