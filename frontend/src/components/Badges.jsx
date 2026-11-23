import { SEVERITY, STATUS_BADGE, titleCase } from '../services/format';

export function StatusBadge({ status }) {
  const pulse = status === 'PENDING' || status === 'ESCALATED' || status === 'ACTIVE';
  return <span className={`badge ${STATUS_BADGE[status] || 'b-grey'} ${pulse ? 'pulse' : ''}`}><span className="dot" />{titleCase((status || '').toLowerCase())}</span>;
}

export function SeverityBadge({ severity }) {
  const s = SEVERITY[severity] || { label: 'Unknown', badge: 'b-grey' };
  return <span className={`badge ${s.badge} ${severity === 5 ? 'pulse' : ''}`}><span className="dot" />{severity} · {s.label}</span>;
}

export function SimBadge({ simulated }) {
  return simulated ? <span className="badge b-violet" title="Generated / demo data — not real">Simulated</span> : null;
}

export function Chips({ items = [], highlight = [], missing = [] }) {
  if (!items.length && !missing.length) return <span className="faint">—</span>;
  return (
    <div className="chips">
      {items.map((c) => <span key={c} className={`chip ${highlight.includes(c) ? 'on' : ''}`}>{c}</span>)}
      {missing.map((c) => <span key={`m-${c}`} className="chip missing" title="required but not offered">✕ {c}</span>)}
    </div>
  );
}

export function ScoreBar({ value, max = 100, color }) {
  const w = Math.max(0, Math.min(100, (value / max) * 100));
  return <div className="bar"><span style={{ width: `${w}%`, background: color }} /></div>;
}
