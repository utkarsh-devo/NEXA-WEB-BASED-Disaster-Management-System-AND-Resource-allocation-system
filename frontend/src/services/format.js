// Display helpers shared by pages.
export const SEVERITY = {
  1: { label: 'Low', color: 'var(--sev-1)', badge: 'b-green' },
  2: { label: 'Moderate', color: 'var(--sev-2)', badge: 'b-green' },
  3: { label: 'High', color: 'var(--sev-3)', badge: 'b-yellow' },
  4: { label: 'Severe', color: 'var(--sev-4)', badge: 'b-orange' },
  5: { label: 'Critical', color: 'var(--sev-5)', badge: 'b-red' },
};
export const SEV_HEX = { 1: '#3ddc97', 2: '#9be15d', 3: '#ffd23f', 4: '#ff7a3d', 5: '#ff3d57' };

export const STATUS_BADGE = {
  PENDING: 'b-red', ACTIVE: 'b-amber', CONTAINED: 'b-cyan', RESOLVED: 'b-green', ARCHIVED: 'b-grey',
  AVAILABLE: 'b-green', DEPLOYED: 'b-amber', MAINTENANCE: 'b-violet', INACTIVE: 'b-grey',
  QUEUED: 'b-cyan', ESCALATED: 'b-red', PROCESSING: 'b-amber', FULFILLED: 'b-green', CANCELLED: 'b-grey',
  COMPLETED: 'b-green',
};

export const CATEGORY_COLORS = {
  ambulance: '#ff3d57', fire_engine: '#ff7a3d', rescue_vehicle: '#ffb627', medical_supplies: '#ff8fab',
  rescue_equipment: '#ffd23f', food_water: '#4da3ff', shelter: '#9b8cff', personnel: '#2de2e6', special_team: '#3ddc97',
};
export const CATEGORY_LABEL = {
  ambulance: 'Ambulance', fire_engine: 'Fire engine', rescue_vehicle: 'Rescue vehicle', medical_supplies: 'Medical supplies',
  rescue_equipment: 'Rescue equipment', food_water: 'Food & water', shelter: 'Shelter', personnel: 'Personnel', special_team: 'Special team',
};

export function timeAgo(iso) {
  if (!iso) return '—';
  const s = Math.max(0, (Date.now() - new Date(iso).getTime()) / 1000);
  if (s < 60) return `${Math.floor(s)}s ago`;
  if (s < 3600) return `${Math.floor(s / 60)}m ago`;
  if (s < 86400) return `${Math.floor(s / 3600)}h ${Math.floor((s % 3600) / 60)}m ago`;
  return `${Math.floor(s / 86400)}d ago`;
}
export function clock(iso) {
  if (!iso) return '—';
  return new Date(iso).toLocaleString(undefined, { hour: '2-digit', minute: '2-digit', day: '2-digit', month: 'short' });
}
export const num = (v, d = 0) => (v === null || v === undefined || Number.isNaN(v) ? '—' : Number(v).toLocaleString(undefined, { maximumFractionDigits: d, minimumFractionDigits: d }));
export const km = (v) => (v === null || v === undefined ? '—' : `${Number(v).toFixed(1)} km`);
export const mins = (v) => (v === null || v === undefined ? '—' : `${Math.round(v)} min`);
export const pct = (v) => `${(Number(v) * 100).toFixed(0)}%`;
export const titleCase = (s) => (s || '').replace(/_/g, ' ').replace(/\b\w/g, (c) => c.toUpperCase());
