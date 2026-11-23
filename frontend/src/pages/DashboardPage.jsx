import { useMemo } from 'react';
import { useNavigate } from 'react-router-dom';
import { motion } from 'framer-motion';
import { Activity, Ambulance, BedDouble, Clock3, Flame, Gauge as GaugeIcon, LayoutDashboard, ListOrdered, Radio, Siren, Truck, Users } from 'lucide-react';
import { api } from '../services/api';
import { useApi } from '../hooks/useApi';
import { useLiveFeed } from '../hooks/useLiveFeed';
import PageHeader from '../components/PageHeader';
import Panel from '../components/Panel';
import StatTile from '../components/StatTile';
import { Async } from '../components/Feedback';
import { SeverityBadge, StatusBadge } from '../components/Badges';
import NexaMap from '../maps/NexaMap';
import MapLegend from '../maps/MapLegend';
import { CategoryDonut, Gauge, SeverityChart, UtilisationBars } from '../charts/Charts';
import { clock, num, timeAgo } from '../services/format';
import { stagger } from '../components/motion';

export default function DashboardPage() {
  const nav = useNavigate();
  const dash = useApi(() => api.dashboard(), []);
  const layers = useApi(() => api.layers(false), []);
  useLiveFeed(() => { dash.reload({ silent: true }); layers.reload({ silent: true }); }, ['incident.', 'deployment.', 'request.', 'resource.', 'hospital.'], 900);

  const highlight = useMemo(() => ({ incidents: new Set((dash.data?.topPriority || []).slice(0, 3).map((r) => r.incidentId)) }), [dash.data]);

  return (
    <>
      <PageHeader kicker="Command" icon={LayoutDashboard} title="Command Dashboard"
        subtitle="Live operational picture computed by the C++ engine: priority heap, spatial indexes, allocation matrix and segment-tree analytics.">
        <button className="btn" onClick={() => nav('/priority')}><ListOrdered size={15} />Priority queue</button>
        <button className="btn primary" onClick={() => nav('/incidents?new=1')}><Siren size={15} />Register incident</button>
      </PageHeader>

      <Async state={dash} label="Compiling situation report">
        {(d) => (
          <motion.div variants={stagger} initial="initial" animate="animate" className="stack" style={{ gap: 16 }}>
            <div className="grid g-6">
              <StatTile label="Open incidents" value={d.incidents.open} icon={Flame} tone="amber" foot={`${d.incidents.total} registered · ${d.incidents.resolved} resolved`} onClick={() => nav('/incidents')} />
              <StatTile label="Critical" value={d.incidents.critical} icon={Siren} tone="red" alert={d.incidents.critical > 0} foot={`${d.incidents.pending} awaiting first response`} />
              <StatTile label="Units available" value={d.resources.available} icon={Ambulance} tone="green" foot={`${d.resources.deployed} deployed · ${d.resources.maintenance} maintenance`} onClick={() => nav('/resources')} />
              <StatTile label="Pending requests" value={d.requests.pending} icon={Radio} tone="cyan" foot={`${d.requests.escalated} escalated to priority queue`} onClick={() => nav('/requests')} />
              <StatTile label="Free hospital beds" value={d.beds.available} icon={BedDouble} tone="violet" foot={`ICU ${d.beds.icuAvailable}/${d.beds.icuTotal}`} onClick={() => nav('/hospitals')} />
              <StatTile label="Avg response" value={d.avgResponseMinutes} decimals={1} suffix=" min" icon={Clock3} tone="amber" foot={`${d.deploymentsActive} active deployments`} />
            </div>

            <div className="grid g-main">
              <Panel title="Operational map" icon={Activity} hint={`${num(d.incidents.affectedPeopleOpen)} people affected (open incidents)`} bodyClass="flush">
                <Async state={layers} label="Plotting assets">
                  {(l) => (
                    <NexaMap incidents={l.incidents} resources={l.resources} hospitals={l.hospitals} className="short"
                      highlight={highlight} onSelect={(kind, item) => kind === 'incident' && nav(`/incidents/${item.id}`)}>
                      <MapLegend />
                    </NexaMap>
                  )}
                </Async>
              </Panel>
              <Panel title="Top priority (binary heap)" icon={ListOrdered} tone="alert" hint="max-heap order" bodyClass="flush">
                <div className="stack" style={{ gap: 0 }}>
                  {d.topPriority.map((r) => (
                    <motion.div key={r.incidentId} layout className="row between" style={{ padding: '11px 16px', borderBottom: '1px solid var(--line)', cursor: 'pointer' }} onClick={() => nav(`/incidents/${r.incidentId}`)}
                      whileHover={{ backgroundColor: 'rgba(255,182,39,.05)' }}>
                      <div className="row" style={{ gap: 12, minWidth: 0 }}>
                        <div className="mono" style={{ fontSize: 22, color: r.rank === 1 ? 'var(--red)' : 'var(--text-faint)', width: 26 }}>{r.rank}</div>
                        <div style={{ minWidth: 0 }}>
                          <div className="cell-main" style={{ whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis', maxWidth: 230 }}>{r.title}</div>
                          <div className="cell-sub mono">{r.code} · {timeAgo(r.createdAt)}</div>
                        </div>
                      </div>
                      <div style={{ textAlign: 'right' }}>
                        <div className="mono" style={{ fontSize: 18, color: 'var(--amber)', fontWeight: 700 }}>{r.priority.toFixed(1)}</div>
                        <SeverityBadge severity={r.severity} />
                      </div>
                    </motion.div>
                  ))}
                </div>
              </Panel>
            </div>

            <div className="grid g-3">
              <Panel title="Severity distribution" icon={Flame} hint="counting sort, k = 5"><SeverityChart data={d.severity} /></Panel>
              <Panel title="Incidents by category" icon={GaugeIcon} hint="K-ary taxonomy roll-up"><CategoryDonut data={d.categories} /></Panel>
              <Panel title="Fleet utilisation" icon={Truck} hint={`${num(d.resources.unitsDeployed)}/${num(d.resources.unitsTotal)} units`}>
                <Gauge value={d.resources.utilisationPct} label="units deployed" color="var(--cyan)" size={150} />
                <div className="row between faint" style={{ fontSize: 12, marginTop: 6 }}>
                  <span>Hospital occupancy</span>
                  <span className="mono">{d.beds.total ? Math.round(100 * (1 - d.beds.available / d.beds.total)) : 0}%</span>
                </div>
              </Panel>
            </div>

            <div className="grid g-main">
              <Panel title="Resource utilisation by category" icon={Users}><UtilisationBars data={d.resourceUtilisation} /></Panel>
              <Panel title="Recent deployments" icon={Truck} bodyClass="flush scroll">
                {d.recentDeployments.map((dep) => (
                  <div key={dep.id} style={{ padding: '10px 16px', borderBottom: '1px solid var(--line)' }}>
                    <div className="row between"><span className="mono" style={{ color: 'var(--amber)' }}>#{dep.id} → incident {dep.incidentId}</span><StatusBadge status={dep.status} /></div>
                    <div className="cell-sub">{dep.resources.map((r) => r.resourceCode).join(', ')} · {clock(dep.createdAt)}</div>
                  </div>
                ))}
              </Panel>
            </div>

            <Panel title="Operational activity (linked-list timelines)" icon={Activity} bodyClass="scroll">
              <div className="timeline">
                {d.recentActivity.map((e) => (
                  <motion.div key={e.id} className={`tl-item ${e.type}`} initial={{ opacity: 0, x: -6 }} animate={{ opacity: 1, x: 0 }}>
                    <div className="tl-meta">{clock(e.createdAt)} · {e.type} · incident #{e.incidentId} · {e.actor}</div>
                    <div className="tl-msg">{e.message}</div>
                  </motion.div>
                ))}
              </div>
            </Panel>
          </motion.div>
        )}
      </Async>
    </>
  );
}
