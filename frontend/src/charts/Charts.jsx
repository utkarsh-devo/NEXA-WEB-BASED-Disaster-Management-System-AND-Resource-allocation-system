import { Area, Bar, BarChart, CartesianGrid, Cell, ComposedChart, Legend, Line, LineChart, Pie, PieChart, PolarAngleAxis, RadialBar, RadialBarChart, ResponsiveContainer, Tooltip, XAxis, YAxis } from 'recharts';
import { PALETTE, axis, grid, tooltip } from './chartTheme';
import { CATEGORY_LABEL, SEV_HEX } from '../services/format';

/** Severity distribution (counting-sort output from the engine). */
export function SeverityChart({ data, height = 220 }) {
  const rows = (data || []).map((d) => ({ name: d.label, value: d.count, sev: d.value }));
  return (
    <ResponsiveContainer width="100%" height={height}>
      <BarChart data={rows} margin={{ top: 10, right: 10, left: -18, bottom: 0 }}>
        <CartesianGrid {...grid} vertical={false} />
        <XAxis dataKey="name" {...axis} />
        <YAxis allowDecimals={false} {...axis} />
        <Tooltip {...tooltip} />
        <Bar dataKey="value" name="Open incidents" radius={[5, 5, 0, 0]} animationDuration={900}>
          {rows.map((r) => <Cell key={r.name} fill={SEV_HEX[r.sev]} />)}
        </Bar>
      </BarChart>
    </ResponsiveContainer>
  );
}

/** Donut of incident categories (K-ary tree roll-up). */
export function CategoryDonut({ data, height = 220 }) {
  const rows = (data || []).map((d) => ({ name: d.label, value: d.count }));
  if (!rows.length) return <div className="empty">No open incidents</div>;
  return (
    <ResponsiveContainer width="100%" height={height}>
      <PieChart>
        <Pie data={rows} dataKey="value" nameKey="name" innerRadius="58%" outerRadius="86%" paddingAngle={3} stroke="none" animationDuration={900}>
          {rows.map((r, i) => <Cell key={r.name} fill={PALETTE[i % PALETTE.length]} />)}
        </Pie>
        <Tooltip {...tooltip} />
        <Legend iconType="circle" wrapperStyle={{ fontSize: 12, color: '#93a3b8' }} />
      </PieChart>
    </ResponsiveContainer>
  );
}

/** Resource utilisation by category, % of units deployed. */
export function UtilisationBars({ data, height = 240 }) {
  const rows = (data || []).map((d) => ({ name: CATEGORY_LABEL[d.label] || d.label, pct: d.value, deployed: d.count }));
  return (
    <ResponsiveContainer width="100%" height={height}>
      <BarChart data={rows} layout="vertical" margin={{ top: 4, right: 20, left: 10, bottom: 0 }}>
        <CartesianGrid {...grid} horizontal={false} />
        <XAxis type="number" domain={[0, 100]} unit="%" {...axis} />
        <YAxis type="category" dataKey="name" width={120} {...axis} />
        <Tooltip {...tooltip} formatter={(v, n) => (n === 'pct' ? [`${v}%`, 'Utilisation'] : [v, n])} />
        <Bar dataKey="pct" name="pct" fill="#2de2e6" radius={[0, 5, 5, 0]} animationDuration={900} background={{ fill: 'rgba(255,255,255,.03)' }} />
      </BarChart>
    </ResponsiveContainer>
  );
}

/** Segment-tree backed timeline (incidents, deployments, resolved per bucket). */
export function TimelineChart({ buckets, height = 260, onBrush }) {
  const rows = (buckets || []).map((b, i) => ({
    i, label: new Date(b.start).toLocaleString(undefined, { day: '2-digit', hour: '2-digit' }),
    incidents: b.incidents, deployments: b.deployments, resolved: b.resolved,
  }));
  return (
    <ResponsiveContainer width="100%" height={height}>
      <ComposedChart data={rows} margin={{ top: 10, right: 10, left: -18, bottom: 0 }} onClick={(e) => onBrush && e && e.activeTooltipIndex !== undefined && onBrush(e.activeTooltipIndex)}>
        <defs>
          <linearGradient id="gInc" x1="0" y1="0" x2="0" y2="1"><stop offset="0%" stopColor="#ff3d57" stopOpacity={0.5} /><stop offset="100%" stopColor="#ff3d57" stopOpacity={0} /></linearGradient>
          <linearGradient id="gDep" x1="0" y1="0" x2="0" y2="1"><stop offset="0%" stopColor="#2de2e6" stopOpacity={0.45} /><stop offset="100%" stopColor="#2de2e6" stopOpacity={0} /></linearGradient>
        </defs>
        <CartesianGrid {...grid} vertical={false} />
        <XAxis dataKey="label" {...axis} minTickGap={24} />
        <YAxis allowDecimals={false} {...axis} />
        <Tooltip {...tooltip} />
        <Area type="monotone" dataKey="incidents" name="Incidents" stroke="#ff3d57" fill="url(#gInc)" strokeWidth={2} animationDuration={1000} />
        <Area type="monotone" dataKey="deployments" name="Deployments" stroke="#2de2e6" fill="url(#gDep)" strokeWidth={2} animationDuration={1000} />
        <Line type="monotone" dataKey="resolved" name="Resolved" stroke="#3ddc97" strokeWidth={2} dot={false} />
      </ComposedChart>
    </ResponsiveContainer>
  );
}

/** Radial gauge 0..100 */
export function Gauge({ value, label, color = '#ffb627', size = 170 }) {
  const data = [{ name: label, value: Math.max(0, Math.min(100, value || 0)), fill: color }];
  return (
    <div style={{ position: 'relative', width: '100%', height: size }}>
      <ResponsiveContainer width="100%" height={size}>
        <RadialBarChart innerRadius="72%" outerRadius="100%" data={data} startAngle={220} endAngle={-40}>
          <PolarAngleAxis type="number" domain={[0, 100]} tick={false} />
          <RadialBar dataKey="value" cornerRadius={8} background={{ fill: 'rgba(255,255,255,.05)' }} animationDuration={1100} />
        </RadialBarChart>
      </ResponsiveContainer>
      <div style={{ position: 'absolute', inset: 0, display: 'grid', placeItems: 'center', textAlign: 'center' }}>
        <div><div className="mono" style={{ fontSize: 28, fontWeight: 700, color }}>{Math.round(value || 0)}%</div><div className="faint" style={{ fontSize: 11, letterSpacing: '.14em', textTransform: 'uppercase' }}>{label}</div></div>
      </div>
    </div>
  );
}

/** Benchmark comparison (median µs + operation counts). */
export function BenchmarkChart({ rows, metric = 'microseconds', height = 280 }) {
  const data = (rows || []).map((r) => ({ name: r.variant, value: r[metric] }));
  return (
    <ResponsiveContainer width="100%" height={height}>
      <BarChart data={data} margin={{ top: 10, right: 10, left: 0, bottom: 40 }}>
        <CartesianGrid {...grid} vertical={false} />
        <XAxis dataKey="name" {...axis} angle={-18} textAnchor="end" interval={0} height={60} />
        <YAxis {...axis} />
        <Tooltip {...tooltip} />
        <Bar dataKey="value" name={metric} radius={[5, 5, 0, 0]} animationDuration={900}>
          {data.map((d, i) => <Cell key={d.name} fill={PALETTE[i % PALETTE.length]} />)}
        </Bar>
      </BarChart>
    </ResponsiveContainer>
  );
}

/** Generic small line chart */
export function MiniLine({ data, dataKey, color = '#ffb627', height = 80 }) {
  return (
    <ResponsiveContainer width="100%" height={height}>
      <LineChart data={data}><Line type="monotone" dataKey={dataKey} stroke={color} strokeWidth={2} dot={false} animationDuration={800} /></LineChart>
    </ResponsiveContainer>
  );
}
