import { useEffect, useRef } from 'react';
import maplibregl from 'maplibre-gl';
import 'maplibre-gl/dist/maplibre-gl.css';
import { DELHI, circlePolygon, darkStyle, fc, rectPolygon } from './mapStyle';
import { CATEGORY_COLORS, SEV_HEX } from '../services/format';

const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));

/**
 * Interactive operational map.
 *  incidents/resources/hospitals : entity arrays (lat, lon)
 *  network      : { locations, roads } to draw the road graph
 *  routes       : [{ coords: [[lon,lat]...], color, dashed, width }]
 *  edges        : [{ a:{lat,lon}, b:{lat,lon}, color }] (MST, traversal trees)
 *  circle       : { lat, lon, km }   rect : { minLat, minLon, maxLat, maxLon }
 *  cells        : quad-tree density cells   clusters : DBSCAN clusters
 *  highlight    : { incidents:Set, resources:Set, hospitals:Set }
 *  onMapClick(lat, lon)   onSelect(kind, item)   onBoxSelect(rect) (shift + drag)
 */
export default function NexaMap({
  incidents = [], resources = [], hospitals = [], network, routes = [], edges = [], circle, rect, cells, clusters,
  highlight, dimOthers = false, onMapClick, onSelect, onBoxSelect, fitTo, className = '', children, center = DELHI, zoom = 10,
}) {
  const box = useRef(null);
  const map = useRef(null);
  const ready = useRef(false);
  const markers = useRef([]);
  const handlers = useRef({});
  handlers.current = { onMapClick, onSelect, onBoxSelect };

  // ---------------------------------------------------------------- init once
  useEffect(() => {
    const m = new maplibregl.Map({ container: box.current, style: darkStyle, center: [center.lon, center.lat], zoom, attributionControl: { compact: true }, boxZoom: false });
    map.current = m;
    m.addControl(new maplibregl.NavigationControl({ showCompass: false }), 'top-right');
    m.addControl(new maplibregl.ScaleControl({ unit: 'metric' }), 'bottom-right');
    m.on('load', () => {
      const add = (id) => m.addSource(id, { type: 'geojson', data: fc([]) });
      ['roads', 'routes', 'edges', 'circle', 'rect', 'cells', 'clusters'].forEach(add);
      m.addLayer({ id: 'cells', type: 'fill', source: 'cells', paint: { 'fill-color': '#ffb627', 'fill-opacity': ['interpolate', ['linear'], ['get', 'count'], 1, 0.04, 6, 0.2] } });
      m.addLayer({ id: 'cells-line', type: 'line', source: 'cells', paint: { 'line-color': '#ffb627', 'line-opacity': 0.25, 'line-width': 0.6 } });
      m.addLayer({ id: 'roads', type: 'line', source: 'roads', paint: {
        'line-color': ['case', ['get', 'blocked'], '#ff3d57', '#2de2e6'], 'line-width': ['case', ['get', 'blocked'], 2.2, 1.2],
        'line-opacity': ['case', ['get', 'blocked'], 0.9, 0.32] } });
      m.addLayer({ id: 'clusters', type: 'fill', source: 'clusters', paint: { 'fill-color': '#ff3d57', 'fill-opacity': 0.12 } });
      m.addLayer({ id: 'clusters-line', type: 'line', source: 'clusters', paint: { 'line-color': '#ff3d57', 'line-width': 1.5, 'line-dasharray': [3, 2] } });
      m.addLayer({ id: 'circle', type: 'fill', source: 'circle', paint: { 'fill-color': '#2de2e6', 'fill-opacity': 0.08 } });
      m.addLayer({ id: 'circle-line', type: 'line', source: 'circle', paint: { 'line-color': '#2de2e6', 'line-width': 1.5, 'line-dasharray': [2, 2] } });
      m.addLayer({ id: 'rect', type: 'fill', source: 'rect', paint: { 'fill-color': '#9b8cff', 'fill-opacity': 0.1 } });
      m.addLayer({ id: 'rect-line', type: 'line', source: 'rect', paint: { 'line-color': '#9b8cff', 'line-width': 1.6 } });
      m.addLayer({ id: 'edges', type: 'line', source: 'edges', paint: { 'line-color': ['get', 'color'], 'line-width': 2.4, 'line-opacity': 0.9 } });
      m.addLayer({ id: 'routes-glow', type: 'line', source: 'routes', layout: { 'line-cap': 'round', 'line-join': 'round' }, paint: { 'line-color': ['get', 'color'], 'line-width': ['+', ['get', 'width'], 8], 'line-opacity': 0.16, 'line-blur': 4 } });
      m.addLayer({ id: 'routes', type: 'line', source: 'routes', layout: { 'line-cap': 'round', 'line-join': 'round' }, paint: { 'line-color': ['get', 'color'], 'line-width': ['get', 'width'], 'line-dasharray': [0, 4, 3] } });
      ready.current = true;
      m.fire('nexa:ready');
    });
    m.on('click', (e) => { if (handlers.current.onMapClick) handlers.current.onMapClick(e.lngLat.lat, e.lngLat.lng); });

    // animated "marching ants" along routes
    const dashSeq = [[0, 4, 3], [0.5, 4, 2.5], [1, 4, 2], [1.5, 4, 1.5], [2, 4, 1], [2.5, 4, 0.5], [3, 4, 0], [0, 0.5, 3, 3.5], [0, 1, 3, 3], [0, 1.5, 3, 2.5], [0, 2, 3, 2], [0, 2.5, 3, 1.5], [0, 3, 3, 1], [0, 3.5, 3, 0.5]];
    let step = 0;
    const anim = setInterval(() => {
      if (!ready.current || !m.getLayer('routes')) return;
      step = (step + 1) % dashSeq.length;
      m.setPaintProperty('routes', 'line-dasharray', dashSeq[step]);
    }, 70);

    // shift + drag = rectangle selection (feeds the quad/range-tree region query)
    let start = null; let boxEl = null;
    const canvas = m.getCanvasContainer();
    const pos = (e) => { const r = canvas.getBoundingClientRect(); return new maplibregl.Point(e.clientX - r.left - canvas.clientLeft, e.clientY - r.top - canvas.clientTop); };
    const onDown = (e) => {
      if (!(e.shiftKey && e.button === 0) || !handlers.current.onBoxSelect) return;
      m.dragPan.disable(); start = pos(e);
      document.addEventListener('mousemove', onMove); document.addEventListener('mouseup', onUp);
      e.preventDefault();
    };
    const onMove = (e) => {
      const cur = pos(e);
      if (!boxEl) { boxEl = document.createElement('div'); boxEl.style.cssText = 'position:absolute;border:1.5px dashed #9b8cff;background:rgba(155,140,255,.1);pointer-events:none;z-index:4'; canvas.appendChild(boxEl); }
      const minX = Math.min(start.x, cur.x), minY = Math.min(start.y, cur.y);
      boxEl.style.left = `${minX}px`; boxEl.style.top = `${minY}px`;
      boxEl.style.width = `${Math.abs(cur.x - start.x)}px`; boxEl.style.height = `${Math.abs(cur.y - start.y)}px`;
    };
    const onUp = (e) => {
      document.removeEventListener('mousemove', onMove); document.removeEventListener('mouseup', onUp);
      m.dragPan.enable();
      if (boxEl) { boxEl.remove(); boxEl = null; }
      const end = pos(e);
      if (Math.abs(end.x - start.x) < 6 || Math.abs(end.y - start.y) < 6) return;
      const a = m.unproject(start), b = m.unproject(end);
      handlers.current.onBoxSelect({ minLat: Math.min(a.lat, b.lat), maxLat: Math.max(a.lat, b.lat), minLon: Math.min(a.lng, b.lng), maxLon: Math.max(a.lng, b.lng) });
    };
    canvas.addEventListener('mousedown', onDown, true);

    return () => { clearInterval(anim); canvas.removeEventListener('mousedown', onDown, true); markers.current.forEach((mk) => mk.remove()); m.remove(); ready.current = false; };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // helper: run when style is ready
  const whenReady = (fn) => {
    const m = map.current;
    if (!m) return;
    if (ready.current) fn(m); else m.once('nexa:ready', () => fn(m));
  };
  const setData = (m, id, data) => { const s = m.getSource(id); if (s) s.setData(data); };

  // ---------------------------------------------------------------- markers
  useEffect(() => {
    const m = map.current;
    if (!m) return;
    markers.current.forEach((mk) => mk.remove());
    markers.current = [];
    const hl = highlight || {};
    const mk = (kind, item, el, html) => {
      el.addEventListener('click', (e) => { e.stopPropagation(); if (handlers.current.onSelect) handlers.current.onSelect(kind, item); });
      const popup = new maplibregl.Popup({ offset: 14, closeButton: true, maxWidth: '300px' }).setHTML(html);
      const marker = new maplibregl.Marker({ element: el }).setLngLat([item.lon, item.lat]).setPopup(popup).addTo(m);
      markers.current.push(marker);
    };
    const isHl = (set, id) => set && set.has && set.has(id);
    const anyHl = dimOthers && (hl.incidents?.size || hl.resources?.size || hl.hospitals?.size);

    hospitals.forEach((h) => {
      const el = document.createElement('div');
      el.className = `pin hospital ${isHl(hl.hospitals, h.id) ? 'hl' : ''} ${anyHl && !isHl(hl.hospitals, h.id) ? 'dim' : ''}`;
      mk('hospital', h, el, `<div style="font-weight:700">${esc(h.name)}</div><div style="color:#93a3b8;font-size:12px;margin:4px 0">${esc(h.category)} · ${h.availableBeds}/${h.totalBeds} beds · ICU ${h.icuAvailable}/${h.icuBeds}</div><div style="font-size:11px;color:#2de2e6">${esc((h.capabilities || []).join(', '))}</div>`);
    });
    resources.forEach((r) => {
      const el = document.createElement('div');
      const color = r.status === 'AVAILABLE' ? (CATEGORY_COLORS[r.category] || '#2de2e6') : '#5d6b7f';
      el.className = `pin resource ${isHl(hl.resources, r.id) ? 'hl' : ''} ${anyHl && !isHl(hl.resources, r.id) ? 'dim' : ''}`;
      el.style.background = color;
      mk('resource', r, el, `<div style="font-weight:700">${esc(r.code)}</div><div style="font-size:12px">${esc(r.name)}</div><div style="color:#93a3b8;font-size:12px;margin-top:4px">${esc(r.status)} · ${r.available}/${r.capacity} units</div><div style="font-size:11px;color:#ffb627">${esc(r.organization || '')}</div>`);
    });
    incidents.forEach((i) => {
      const el = document.createElement('div');
      const color = SEV_HEX[i.severity] || '#ffb627';
      el.className = `pin incident ${isHl(hl.incidents, i.id) ? 'hl' : ''} ${anyHl && !isHl(hl.incidents, i.id) ? 'dim' : ''}`;
      el.style.background = color; el.style.color = color;
      if (i.status === 'RESOLVED' || i.status === 'ARCHIVED') { el.style.opacity = '0.45'; el.classList.remove('incident'); }
      mk('incident', i, el, `<div style="font-size:11px;color:#ffb627;font-family:JetBrains Mono,monospace">${esc(i.code)}</div><div style="font-weight:700;margin:2px 0">${esc(i.title)}</div><div style="color:#93a3b8;font-size:12px">${esc(i.categoryPath || i.categoryName)} · severity ${i.severity} · ${esc(i.status)}</div><div style="font-size:12px;margin-top:4px">Priority <b style="color:#ff3d57">${Number(i.priorityScore || 0).toFixed(1)}</b> · ${i.assignedUnits}/${i.requiredUnits} units</div>${i.simulated ? '<div style="font-size:10px;color:#9b8cff;margin-top:4px">SIMULATED DATA</div>' : ''}`);
    });
  }, [incidents, resources, hospitals, highlight, dimOthers]);

  // ---------------------------------------------------------------- geometry layers
  useEffect(() => {
    whenReady((m) => {
      if (!network) { setData(m, 'roads', fc([])); return; }
      const byId = new Map(network.locations.map((l) => [l.id, l]));
      setData(m, 'roads', fc(network.roads.map((r) => {
        const a = byId.get(r.fromId), b = byId.get(r.toId);
        if (!a || !b) return null;
        return { type: 'Feature', properties: { blocked: r.blocked, id: r.id, name: r.name }, geometry: { type: 'LineString', coordinates: [[a.lon, a.lat], [b.lon, b.lat]] } };
      }).filter(Boolean)));
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [network]);

  useEffect(() => {
    whenReady((m) => setData(m, 'routes', fc(routes.filter((r) => r.coords && r.coords.length > 1).map((r) => ({
      type: 'Feature', properties: { color: r.color || '#ffb627', width: r.width || 4 }, geometry: { type: 'LineString', coordinates: r.coords },
    })))));
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [routes]);

  useEffect(() => {
    whenReady((m) => setData(m, 'edges', fc(edges.map((e) => ({
      type: 'Feature', properties: { color: e.color || '#3ddc97' }, geometry: { type: 'LineString', coordinates: [[e.a.lon, e.a.lat], [e.b.lon, e.b.lat]] },
    })))));
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [edges]);

  useEffect(() => { whenReady((m) => setData(m, 'circle', circle ? fc([circlePolygon(circle.lat, circle.lon, circle.km)]) : fc([]))); /* eslint-disable-next-line */ }, [circle]);
  useEffect(() => { whenReady((m) => setData(m, 'rect', rect ? fc([rectPolygon(rect)]) : fc([]))); /* eslint-disable-next-line */ }, [rect]);
  useEffect(() => { whenReady((m) => setData(m, 'cells', fc((cells || []).map((c) => ({ ...rectPolygon(c), properties: { count: c.count } }))))); /* eslint-disable-next-line */ }, [cells]);
  useEffect(() => { whenReady((m) => setData(m, 'clusters', fc((clusters || []).map((c) => circlePolygon(c.centerLat, c.centerLon, Math.max(0.4, c.radiusKm + 0.3)))))); /* eslint-disable-next-line */ }, [clusters]);

  // ---------------------------------------------------------------- fit bounds
  useEffect(() => {
    if (!fitTo || !fitTo.length) return;
    whenReady((m) => {
      const b = new maplibregl.LngLatBounds();
      fitTo.forEach((p) => b.extend([p.lon, p.lat]));
      m.fitBounds(b, { padding: 70, maxZoom: 14, duration: 900 });
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [fitTo]);

  return (
    <div className={`map-wrap ${className}`}>
      <div ref={box} className="map-canvas" />
      {children}
    </div>
  );
}
