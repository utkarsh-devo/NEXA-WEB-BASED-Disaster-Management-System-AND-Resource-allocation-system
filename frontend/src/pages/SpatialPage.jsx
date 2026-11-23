import { useMemo, useState } from 'react';
import { motion } from 'framer-motion';
import { BoxSelect, Circle, Crosshair, Layers, Map as MapIcon, Radar, ScanSearch } from 'lucide-react';
import { api } from '../services/api';
import { useApi } from '../hooks/useApi';
import { useLiveFeed } from '../hooks/useLiveFeed';
import { useToast } from '../hooks/useToast';
import PageHeader from '../components/PageHeader';
import Panel from '../components/Panel';
import Tabs from '../components/Tabs';
import { Checkbox, Field, NumberInput, Select } from '../components/Form';
import NexaMap from '../maps/NexaMap';
import MapLegend from '../maps/MapLegend';
import { CATEGORY_LABEL, km } from '../services/format';

const MODES = [{ id: 'nearest', label: 'Nearest (KD-tree)', icon: Crosshair }, { id: 'radius', label: 'Radius', icon: Circle }, { id: 'region', label: 'Region (Quad / Range tree)', icon: BoxSelect }];

export default function SpatialPage() {
  const toast = useToast();
  const [mode, setMode] = useState('nearest');
  const [kind, setKind] = useState('resource');
  const [category, setCategory] = useState('');
  const [k, setK] = useState(6);
  const [radiusKm, setRadiusKm] = useState(5);
  const [method, setMethod] = useState('quadtree');
  const [availableOnly, setAvailableOnly] = useState(true);
  const [showDensity, setShowDensity] = useState(false);
  const [showClusters, setShowClusters] = useState(false);
  const [eps, setEps] = useState(3);
  const [point, setPoint] = useState(null);
  const [rect, setRect] = useState(null);
  const [result, setResult] = useState(null);
  const [compare, setCompare] = useState(null);

  const layers = useApi(() => api.layers(false), []);
  const density = useApi(() => api.density(), [], { enabled: showDensity });
  const clusters = useApi(() => api.clusters(eps, 2), [eps], { enabled: showClusters });
  useLiveFeed(() => layers.reload({ silent: true }), ['incident.', 'resource.', 'hospital.'], 1000);

  const runPoint = async (lat, lon) => {
    setPoint({ lat, lon }); setRect(null);
    try {
      const common = { lat, lon, kind, category, availableOnly: availableOnly ? 1 : 0 };
      if (mode === 'nearest') {
        const c = await api.compareSpatial({ ...common, k });
        setResult({ items: c.kdTree.items, stats: c.kdTree.stats }); setCompare(c);
      } else if (mode === 'radius') {
        const r = await api.radius({ ...common, km: radiusKm });
        setResult(r); setCompare(null);
      }
    } catch (e) { toast.fromError(e, 'Spatial query failed'); }
  };
  const runRegion = async (r) => {
    setRect(r); setPoint(null); setMode('region');
    try {
      const all = await Promise.all(['quadtree', 'rangetree', 'linear'].map((m) => api.region({ ...r, kind: kind === 'resource' && m === 'rangetree' ? 'incident' : kind, method: m })));
      const chosen = all[['quadtree', 'rangetree', 'linear'].indexOf(method)];
      setResult(chosen); setCompare({ region: all });
    } catch (e) { toast.fromError(e, 'Region query failed'); }
  };

  const hl = useMemo(() => {
    const ids = new Set((result?.items || []).map((x) => x.id));
    return { resources: kind === 'resource' || kind === 'all' ? ids : new Set(), hospitals: kind === 'hospital' || kind === 'all' ? ids : new Set(), incidents: kind === 'incident' || kind === 'all' ? ids : new Set() };
  }, [result, kind]);
  const circle = useMemo(() => (mode === 'radius' && point ? { ...point, km: radiusKm } : null), [mode, point, radiusKm]);
  const routes = useMemo(() => (point && result?.items ? result.items.slice(0, k).map((x) => ({ coords: [[point.lon, point.lat], [x.lon, x.lat]], color: 'rgba(255,182,39,.55)', width: 1.5 })) : []), [point, result, k]);

  return (
    <>
      <PageHeader kicker="Module 7" icon={MapIcon} title="Spatial Intelligence & Resource Discovery"
        subtitle="Click the map for nearest-neighbour or radius queries (3-D KD-tree on unit-sphere vectors → exact Haversine), or Shift + drag for region queries on the Quad-tree / Range-tree. Proximity informs suitability; it never decides allocation alone." />
      <div className="grid g-side">
        <div className="stack" style={{ gap: 16 }}>
          <Panel title="Query" icon={ScanSearch}>
            <Tabs tabs={MODES} value={mode} onChange={setMode} id="spatial" />
            <div className="form-grid">
              <Field label="Search for"><Select value={kind} onChange={setKind} options={[{ value: 'resource', label: 'Resources' }, { value: 'hospital', label: 'Hospitals' }, { value: 'incident', label: 'Incidents' }, ...(mode !== 'nearest' ? [{ value: 'all', label: 'Everything' }] : [])]} /></Field>
              {kind === 'resource' && <Field label="Category"><Select value={category} onChange={setCategory} options={[{ value: '', label: 'Any' }, ...Object.entries(CATEGORY_LABEL).map(([v, l]) => ({ value: v, label: l }))]} /></Field>}
              {mode === 'nearest' && <Field label="k"><NumberInput value={k} onChange={setK} min={1} max={30} /></Field>}
              {mode === 'radius' && <Field label={`Radius · ${radiusKm} km`}><input type="range" className="range" min={1} max={25} value={radiusKm} onChange={(e) => setRadiusKm(Number(e.target.value))} /></Field>}
              {mode === 'region' && <Field label="Structure"><Select value={method} onChange={setMethod} options={[{ value: 'quadtree', label: 'Quad-tree' }, { value: 'rangetree', label: 'Range-tree (incidents)' }, { value: 'linear', label: 'Linear scan' }]} /></Field>}
              <div className="full"><Checkbox checked={availableOnly} onChange={setAvailableOnly} label="Only available / open" /></div>
            </div>
            <div className="note cyan" style={{ marginTop: 12 }}>{mode === 'region' ? 'Hold Shift and drag a rectangle on the map.' : 'Click anywhere on the map to run the query.'}</div>
          </Panel>

          <Panel title="Overlays" icon={Layers}>
            <div className="stack">
              <Checkbox checked={showDensity} onChange={setShowDensity} label="Quad-tree density cells" />
              <Checkbox checked={showClusters} onChange={setShowClusters} label="Incident clusters (DBSCAN via KD radius)" />
              {showClusters && <Field label={`Cluster radius ε · ${eps} km`}><input type="range" className="range" min={1} max={10} value={eps} onChange={(e) => setEps(Number(e.target.value))} /></Field>}
              {showClusters && clusters.data && <div className="faint" style={{ fontSize: 12 }}>{clusters.data.length} cluster(s): {clusters.data.map((c) => `${c.incidentIds.length} incidents (max sev ${c.maxSeverity})`).join(' · ') || 'none'}</div>}
            </div>
          </Panel>

          {result && (
            <Panel title="Results" icon={Radar} tone="cyan" hint={`${result.stats.method} · ${result.stats.candidatesExamined} examined · ${result.stats.microseconds.toFixed(0)} µs`} bodyClass="flush scroll">
              {result.items.length === 0 && <div className="empty">Nothing found</div>}
              {result.items.map((x, i) => (
                <motion.div key={`${x.kind}-${x.id}`} initial={{ opacity: 0, x: -8 }} animate={{ opacity: 1, x: 0 }} transition={{ delay: i * 0.03 }} className="row between" style={{ padding: '9px 16px', borderBottom: '1px solid var(--line)' }}>
                  <div><div className="cell-main" style={{ fontSize: 13 }}>{x.name}</div><div className="cell-sub">{x.kind} · {x.category} · {x.status}</div></div>
                  <span className="mono" style={{ color: 'var(--cyan)' }}>{km(x.distanceKm)}</span>
                </motion.div>
              ))}
            </Panel>
          )}
          {compare && compare.kdTree && (
            <Panel title="KD-tree vs linear scan" tone="violet">
              <dl className="kv">
                <dt>KD examined</dt><dd>{compare.kdTree.stats.candidatesExamined} nodes · {compare.kdTree.stats.microseconds.toFixed(1)} µs</dd>
                <dt>Linear examined</dt><dd>{compare.linear.stats.candidatesExamined} records · {compare.linear.stats.microseconds.toFixed(1)} µs</dd>
                <dt>Same nearest</dt><dd style={{ color: compare.sameNearest ? 'var(--green)' : 'var(--red)' }}>{compare.sameNearest ? 'yes' : 'no'}</dd>
              </dl>
              <div className="faint" style={{ fontSize: 11, marginTop: 8 }}>KD-tree search is typically logarithmic but can degrade to O(n) on skewed data.</div>
            </Panel>
          )}
          {compare && compare.region && (
            <Panel title="Region query comparison" tone="violet">
              {compare.region.map((r) => <div key={r.stats.method} className="row between" style={{ fontSize: 12, padding: '4px 0' }}><span className="mono">{r.stats.method}</span><span>{r.items.length} hits · {r.stats.candidatesExamined} examined · {r.stats.microseconds.toFixed(1)} µs</span></div>)}
            </Panel>
          )}
        </div>

        <Panel title="Map" icon={MapIcon} bodyClass="flush">
          <NexaMap className="tall" incidents={layers.data?.incidents || []} resources={layers.data?.resources || []} hospitals={layers.data?.hospitals || []}
            highlight={hl} dimOthers={Boolean(result)} onMapClick={(lat, lon) => mode !== 'region' && runPoint(lat, lon)} onBoxSelect={runRegion}
            circle={circle} rect={rect} routes={routes} cells={showDensity ? density.data : null} clusters={showClusters ? clusters.data : null}>
            <MapLegend />
            {point && <div className="map-overlay map-hud"><span className="hud-chip">query @ {point.lat.toFixed(4)}, {point.lon.toFixed(4)}</span></div>}
          </NexaMap>
        </Panel>
      </div>
    </>
  );
}
