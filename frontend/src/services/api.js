// ============================================================================
// NEXA REST client. Every computation shown in the UI comes from these C++
// endpoints — the frontend never re-implements the algorithms.
// ============================================================================
const BASE = (import.meta.env.VITE_API_URL || '').replace(/\/$/, '');
const TOKEN_KEY = 'nexa.token';

let token = null;
try { token = sessionStorage.getItem(TOKEN_KEY); } catch { token = null; }
let onUnauthorized = () => {};

export const auth = {
  get token() { return token; },
  set(t) { token = t; try { t ? sessionStorage.setItem(TOKEN_KEY, t) : sessionStorage.removeItem(TOKEN_KEY); } catch { /* storage unavailable */ } },
  onUnauthorized(fn) { onUnauthorized = fn; },
};

export class ApiError extends Error {
  constructor(status, code, message) { super(message); this.status = status; this.code = code; }
}

function qs(params) {
  if (!params) return '';
  const p = Object.entries(params).filter(([, v]) => v !== undefined && v !== null && v !== '');
  return p.length ? '?' + new URLSearchParams(p).toString() : '';
}

async function request(method, path, { body, params, raw } = {}) {
  const headers = { Accept: 'application/json' };
  if (token) headers.Authorization = `Bearer ${token}`;
  let payload;
  if (body !== undefined) {
    if (typeof body === 'string') { headers['Content-Type'] = 'text/csv'; payload = body; }
    else { headers['Content-Type'] = 'application/json'; payload = JSON.stringify(body); }
  }
  let res;
  try {
    res = await fetch(`${BASE}${path}${qs(params)}`, { method, headers, body: payload });
  } catch {
    throw new ApiError(0, 'NETWORK', 'Cannot reach the NEXA engine. Is the C++ server running on port 8080?');
  }
  if (raw) {
    if (!res.ok) throw new ApiError(res.status, 'HTTP', `Request failed (${res.status})`);
    return res;
  }
  const text = await res.text();
  let data = null;
  try { data = text ? JSON.parse(text) : null; } catch { data = text; }
  if (!res.ok) {
    const err = data && data.error ? data.error : { code: 'HTTP', message: `Request failed (${res.status})` };
    if (res.status === 401) onUnauthorized();
    throw new ApiError(res.status, err.code, err.message);
  }
  return data;
}

const get = (p, params) => request('GET', p, { params });
const post = (p, body, params) => request('POST', p, { body: body ?? {}, params });
const put = (p, body) => request('PUT', p, { body });
const patch = (p, body) => request('PATCH', p, { body });
const del = (p) => request('DELETE', p);

export const api = {
  // auth & users
  login: (username, password) => post('/api/auth/login', { username, password }),
  logout: () => post('/api/auth/logout'),
  me: () => get('/api/auth/me'),
  users: () => get('/api/users'),
  createUser: (u) => post('/api/users', u),
  updateUser: (id, u) => put(`/api/users/${id}`, u),
  resetPassword: (id, password) => post(`/api/users/${id}/password`, { password }),

  // system
  health: () => get('/api/health'),
  dashboard: () => get('/api/dashboard'),
  activity: (limit = 30) => get('/api/activity', { limit }),
  logs: (limit = 100) => get('/api/system/logs', { limit }),
  reload: () => post('/api/system/reload'),
  weights: () => get('/api/settings/weights'),
  saveWeights: (w) => put('/api/settings/weights', w),
  undo: () => post('/api/actions/undo'),

  // incidents
  incidents: (params) => get('/api/incidents', params),
  incident: (id) => get(`/api/incidents/${id}`),
  createIncident: (b) => post('/api/incidents', b),
  updateIncident: (id, b) => put(`/api/incidents/${id}`, b),
  setIncidentStatus: (id, status, note) => patch(`/api/incidents/${id}/status`, { status, note }),
  deleteIncident: (id) => del(`/api/incidents/${id}`),
  addNote: (id, text) => post(`/api/incidents/${id}/notes`, { text }),
  categories: () => get('/api/categories'),

  // resources
  resources: (params) => get('/api/resources', params),
  resource: (id) => get(`/api/resources/${id}`),
  createResource: (b) => post('/api/resources', b),
  updateResource: (id, b) => put(`/api/resources/${id}`, b),
  setResourceStatus: (id, status) => patch(`/api/resources/${id}/status`, { status }),
  deactivateResource: (id) => del(`/api/resources/${id}`),
  resourceTypes: () => get('/api/resource-types'),

  // search
  suggestIncidents: (q) => get('/api/search/incidents/suggest', { q }),
  textSearch: (q) => get('/api/search/incidents/text', { q }),
  suggestResources: (q) => get('/api/search/resources/suggest', { q }),
  lookupResources: (field, value, method) => get('/api/search/resources/lookup', { field, value, method }),

  // requests
  requests: (status) => get('/api/requests', { status }),
  request: (id) => get(`/api/requests/${id}`),
  createRequest: (b) => post('/api/requests', b),
  escalate: (id) => post(`/api/requests/${id}/escalate`),
  cancelRequest: (id) => post(`/api/requests/${id}/cancel`),
  queues: () => get('/api/request-queue'),
  processNext: (mode) => post('/api/request-queue/process', { mode }),

  // priority
  priorityQueue: () => get('/api/priority/queue'),
  priorityHeap: () => get('/api/priority/heap'),
  refreshPriority: () => post('/api/priority/refresh'),

  // ranking & allocation
  ranking: (incidentId, params) => get(`/api/ranking/${incidentId}`, params),
  deployments: (status) => get('/api/deployments', { status }),
  deployment: (id) => get(`/api/deployments/${id}`),
  allocate: (b) => post('/api/deployments', b),
  completeDeployment: (id) => post(`/api/deployments/${id}/complete`),
  cancelDeployment: (id, reason) => post(`/api/deployments/${id}/cancel`, { reason }),
  reassign: (b) => post('/api/allocation/reassign', b),
  matrix: () => get('/api/allocation/matrix'),

  // spatial
  layers: (includeClosed) => get('/api/spatial/layers', { includeClosed: includeClosed ? 1 : undefined }),
  nearest: (params) => get('/api/spatial/nearest', params),
  radius: (params) => get('/api/spatial/radius', params),
  region: (params) => get('/api/spatial/region', params),
  clusters: (eps, minPoints) => get('/api/spatial/clusters', { eps, minPoints }),
  density: () => get('/api/spatial/density'),
  compareSpatial: (params) => get('/api/spatial/compare', params),

  // network
  graph: () => get('/api/network/graph'),
  networkSummary: () => get('/api/network/summary'),
  route: (params) => get('/api/network/route', params),
  compareRoutes: (params) => get('/api/network/compare', params),
  explore: (source, algorithm) => get('/api/network/explore', { source, algorithm }),
  mst: (algorithm, mode) => get('/api/network/mst', { algorithm, mode }),
  components: () => get('/api/network/components'),
  setRoadBlocked: (id, blocked) => patch(`/api/network/roads/${id}`, { blocked }),

  // hospitals
  hospitals: (sortBy) => get('/api/hospitals', { sortBy }),
  hospital: (id) => get(`/api/hospitals/${id}`),
  createHospital: (b) => post('/api/hospitals', b),
  updateHospital: (id, b) => put(`/api/hospitals/${id}`, b),
  deleteHospital: (id) => del(`/api/hospitals/${id}`),
  suitableHospitals: (params) => get('/api/medical/suitable', params),
  reserveBeds: (id, b) => post(`/api/hospitals/${id}/reserve`, b),
  releaseReservation: (id) => post(`/api/reservations/${id}/release`),
  reservations: (hospitalId) => get('/api/reservations', { hospitalId }),
  hospitalLoad: (id) => get(`/api/hospitals/${id}/load`),

  // analytics
  timeline: (params) => get('/api/analytics/timeline', params),
  responseStats: () => get('/api/analytics/response'),
  categoryStats: (includeClosed = true) => get('/api/analytics/categories', { includeClosed: includeClosed ? 1 : 0 }),
  hospitalStats: () => get('/api/analytics/hospitals'),
  statusStats: () => get('/api/analytics/status'),
  importCsv: (entity, csv) => request('POST', `/api/import/${entity}`, { body: csv }),
  exportUrl: (entity) => `${BASE}/api/export/${entity}?token=${encodeURIComponent(token || '')}`,

  // simulation
  simulations: () => get('/api/simulations'),
  createSimulation: (b) => post('/api/simulations', b),
  replay: (id) => get(`/api/simulations/${id}/replay`),
  stepSimulation: (id) => post(`/api/simulations/${id}/step`),
  resolveSimulation: (id) => post(`/api/simulations/${id}/resolve`),

  // DSA lab
  labCatalog: () => get('/api/lab/catalog'),
  lab: (kind, body) => post(`/api/lab/run/${kind}`, body),
  benchmark: (suite, n, repeats) => get('/api/lab/benchmark', { suite, n, repeats }),
};

export function wsUrl() {
  const explicit = import.meta.env.VITE_WS_URL;
  const base = explicit || `${location.protocol === 'https:' ? 'wss' : 'ws'}://${location.host}`;
  return `${base.replace(/\/$/, '')}/ws/live?token=${encodeURIComponent(token || '')}`;
}
