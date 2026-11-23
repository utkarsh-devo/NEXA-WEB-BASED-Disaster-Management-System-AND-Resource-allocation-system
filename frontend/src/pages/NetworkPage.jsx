import { useMemo, useState } from 'react';
import { motion } from 'framer-motion';
import { Ban, GitBranch, GitMerge, Network, Route as RouteIcon, Waypoints } from 'lucide-react';
import { api } from '../services/api';
import { useApi } from '../hooks/useApi';
import { useLiveFeed } from '../hooks/useLiveFeed';
import { useToast } from '../hooks/useToast';
import { useAuth } from '../hooks/useAuth';
import PageHeader from '../components/PageHeader';
import Panel from '../components/Panel';
import Tabs from '../components/Tabs';
import { Async } from '../components/Feedback';
import { Field, Select } from '../components/Form';
import NexaMap from '../maps/NexaMap';
import { km, mins, num } from '../services/format';

const TOOLS = [{ id: 'route', label: 'Shortest path', icon: RouteIcon }, { id: 'compare', label: 'Compare routes', icon: Waypoints }, { id: 'explore', label: 'BFS / DFS', icon: Network }, { id: 'mst', label: 'MST', icon: GitMerge }];
const ROUTE_COLORS = { distance: '#ffb627', time: '#2de2e6', cost: '#9b8cff', alternative: '#ff3d57' };
const DEPTH_COLORS = ['#ffb627', '#ff7a3d', '#ff3d57', '#9b8cff', '#4da3ff', '#2de2e6', '#3ddc97'];

export default function NetworkPage() {
  const toast = useToast();
  const { can } = useAuth();
  const [tool, setTool] = useState('route');
  const [mode, setMode] = useState('distance');
  const [pts, setPts] = useState([]);
  const [routes, setRoutes] = useState([]);
  const [source, setSource] = useState('');
  const [algo, setAlgo] = useState('bfs');
  const [trav, setTrav] = useState(null);
  const [mst, setMst] = useState(null);
  const graph = useApi(() => api.graph(), []);
  const summary = useApi(() => api.networkSummary(), []);
  useLiveFeed(() => { graph.reload({ silent: true }); summary.reload({ silent: true }); }, ['network.'], 300);

  const locs = graph.data?.locations || [];
  const locById = useMemo(() => new Map(locs.map((l) => [l.id, l])), [locs]);

  const onClick = async (lat, lon) => {
    if (tool !== 'route' && tool !== 'compare') return;
    const next = pts.length >= 2 ? [{ lat, lon }] : [...pts, { lat, lon }];
    setPts(next);
    if (next.length < 2) { setRoutes([]); return; }
    const q = { fromLat: next[0].lat, fromLon: next[0].lon, toLat: next[1].lat, toLon: next[1].lon };
    try {
      if (tool === 'route') { const r = await api.route({ ...q, mode }); setRoutes([r]); if (!r.found) toast.error('No route', r.note); }
      else setRoutes(await api.compareRoutes(q));
    } catch (e) { toast.fromError(e, 'Routing failed'); }
  };

  const runExplore = async (id = source, a = algo) => { if (!id) return; try { setTrav(await api.explore(id, a)); } catch (e) { toast.fromError(e); } };
  const runMst = async (a) => { try { setMst(await api.mst(a, mode)); } catch (e) { toast.fromError(e); } };
  const toggleRoad = async (r) => { try { await api.setRoadBlocked(r.id, !r.blocked); toast.success(r.blocked ? 'Road reopened' : 'Road blocked', r.name); graph.reload({ silent: true }); summary.reload({ silent: true }); } catch (e) { toast.fromError(e); } };

  const lines = useMemo(() => routes.filter((r) => r.found).map((r) => ({ coords: r.stops.map((s) => [s.lon, s.lat]), color: ROUTE_COLORS[r.weightMode] || '#ffb627', width: r.weightMode === 'alternative' ? 3 : 4.5 })), [routes]);
  const edges = useMemo(() => {
    if (tool === 'explore' && trav) {
      return trav.order.map((s, i) => ({ s, parent: trav.parentLocation[i], d: trav.depth[i] })).filter((x) => x.parent && locById.get(x.parent))
        .map((x) => ({ a: locById.get(x.parent), b: x.s, color: DEPTH_COLORS[x.d % DEPTH_COLORS.length] }));
    }
    if (tool === 'mst' && mst) return mst.edges.map((e) => ({ a: e.a, b: e.b, color: '#3ddc97' }));
    return [];
  }, [tool, trav, mst, locById]);
  const pointMarkers = useMemo(() => pts.map((p, i) => ({ id: -1 - i, lat: p.lat, lon: p.lon, severity: i === 0 ? 1 : 5, title: i === 0 ? 'Origin' : 'Destination', code: i === 0 ? 'A' : 'B', status: 'PENDING', priorityScore: 0, assignedUnits: 0, requiredUnits: 0 })), [pts]);

  return (
    <>
      <PageHeader kicker="Module 8" icon={GitBranch} title="Emergency Network & Route Analysis"
        subtitle="Road graph as adjacency lists (matrix built on demand). Dijkstra uses a binary-heap priority queue and supports only non-negative weights; 'time' weights are seeded speed estimates, not live traffic. MSTs describe connectivity cost — they are not routes.">
        <Field label="Weight"><Select value={mode} onChange={setMode} options={[{ value: 'distance', label: 'Distance (km)' }, { value: 'time', label: 'Travel time (min, est.)' }, { value: 'cost', label: 'Cost (km + penalty)' }]} /></Field>
      </PageHeader>
      <Async state={summary}>
        {(s) => (
          <div className="grid g-6" style={{ marginBottom: 16 }}>
            {[['Vertices', s.vertices], ['Directed arcs', s.arcs], ['Roads', s.roads], ['Blocked', s.blockedRoads], ['Components', s.components], ['List vs matrix', `${(s.listBytes / 1024).toFixed(1)} / ${(s.matrixBytes / 1024).toFixed(1)} KB`]].map(([l, v]) => (
              <motion.div key={l} className="panel stat" initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }}>
                <div className="stat-label">{l}</div><div className="stat-value" style={{ fontSize: 22, color: l === 'Blocked' && v ? 'var(--red)' : l === 'Components' && v > 1 ? 'var(--amber)' : 'var(--cyan)' }}>{v}</div>
              </motion.div>
            ))}
          </div>
        )}
      </Async>
      <div className="grid g-side">
        <div className="stack" style={{ gap: 16 }}>
          <Panel title="Tools" icon={Waypoints}>
            <Tabs tabs={TOOLS} value={tool} onChange={(t) => { setTool(t); setRoutes([]); setPts([]); }} id="net" />
            {(tool === 'route' || tool === 'compare') && <div className="note cyan">Click two points on the map: origin, then destination. Coordinates are snapped to the nearest road vertex (KD-tree).</div>}
            {tool === 'explore' && (
              <div className="form-grid">
                <Field label="Source" full><Select value={source} onChange={(v) => { setSource(v); runExplore(v, algo); }} placeholder="Choose a location…" options={locs.map((l) => ({ value: String(l.id), label: `${l.name} (${l.kind})` }))} /></Field>
                <Field label="Algorithm" full><Select value={algo} onChange={(v) => { setAlgo(v); runExplore(source, v); }} options={[{ value: 'bfs', label: 'Breadth-first (queue)' }, { value: 'dfs', label: 'Depth-first (stack)' }]} /></Field>
              </div>
            )}
            {tool === 'mst' && <div className="row"><button className="btn green" onClick={() => runMst('kruskal')}>Kruskal (union-find)</button><button className="btn cyan" onClick={() => runMst('prim')}>Prim (heap)</button></div>}
          </Panel>

          {routes.length > 0 && (
            <Panel title="Routes" icon={RouteIcon} tone="cyan" bodyClass="flush">
              {routes.map((r, i) => (
                <div key={i} style={{ padding: '11px 16px', borderBottom: '1px solid var(--line)' }}>
                  <div className="row between"><span className="row" style={{ gap: 8 }}><span className="legend-dot" style={{ background: ROUTE_COLORS[r.weightMode] }} /><b style={{ textTransform: 'capitalize' }}>{r.weightMode}</b></span>
                    {r.found ? <span className="mono">{km(r.distanceKm)} · {mins(r.minutes)}</span> : <span style={{ color: 'var(--red)' }}>unreachable</span>}</div>
                  {r.found && <div className="cell-sub" style={{ marginTop: 4 }}>{r.stops.map((s) => s.name).join(' → ')}</div>}
                  <div className="faint mono" style={{ fontSize: 11, marginTop: 4 }}>settled {r.settled} · relaxations {r.relaxations} · {r.microseconds.toFixed(0)} µs{r.note ? ` · ${r.note}` : ''}</div>
                </div>
              ))}
            </Panel>
          )}
          {tool === 'explore' && trav && (
            <Panel title={`${trav.algorithm.toUpperCase()} order`} icon={Network} hint={`${trav.reachable} reachable · ${trav.unreachable} unreachable · ${num(trav.operations)} edge checks`} bodyClass="scroll">
              <div className="chips">{trav.order.map((s, i) => <motion.span key={s.locationId} className="chip" initial={{ opacity: 0, scale: 0.6 }} animate={{ opacity: 1, scale: 1 }} transition={{ delay: i * 0.03 }} style={{ borderColor: DEPTH_COLORS[trav.depth[i] % DEPTH_COLORS.length] }}>{i + 1}. {s.name}</motion.span>)}</div>
              {trav.unreachable > 0 && <div className="note red" style={{ marginTop: 10 }}>{trav.unreachable} location(s) are in a disconnected region (blocked or missing roads).</div>}
            </Panel>
          )}
          {tool === 'mst' && mst && (
            <Panel title={`${mst.algorithm} spanning forest`} icon={GitMerge} tone="cyan">
              <dl className="kv"><dt>Edges</dt><dd>{mst.edges.length}</dd><dt>Total weight</dt><dd>{mst.totalWeight.toFixed(2)} ({mode})</dd><dt>Components</dt><dd>{mst.components}</dd><dt>Operations</dt><dd>{num(mst.operations)}</dd><dt>Time</dt><dd>{mst.microseconds.toFixed(0)} µs</dd></dl>
              <div className="note" style={{ marginTop: 10 }}>Minimum network needed to keep every location connected — used for connectivity analysis, not for routing.</div>
            </Panel>
          )}
          {can('manage_network') && (
            <Panel title="Road status" icon={Ban} bodyClass="flush scroll">
              {(graph.data?.roads || []).map((r) => (
                <div key={r.id} className="row between" style={{ padding: '7px 16px', borderBottom: '1px solid var(--line)', fontSize: 12 }}>
                  <span style={{ color: r.blocked ? 'var(--red)' : undefined }}>{r.name}<span className="faint mono"> · {r.distanceKm.toFixed(1)} km</span></span>
                  <button className={`btn sm ${r.blocked ? 'green' : 'danger'}`} onClick={() => toggleRoad(r)}>{r.blocked ? 'Reopen' : 'Block'}</button>
                </div>
              ))}
            </Panel>
          )}
        </div>
        <Panel title="Road network" icon={GitBranch} bodyClass="flush">
          <NexaMap className="tall" network={graph.data} routes={lines} edges={edges} incidents={pointMarkers} onMapClick={onClick}>
            <div className="map-overlay map-hud"><span className="hud-chip">{tool === 'route' || tool === 'compare' ? `${pts.length}/2 points` : tool.toUpperCase()}</span><span className="hud-chip">weight: {mode}</span></div>
          </NexaMap>
        </Panel>
      </div>
    </>
  );
}
