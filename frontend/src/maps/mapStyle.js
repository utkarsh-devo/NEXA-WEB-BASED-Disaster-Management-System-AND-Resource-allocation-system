// Dark basemap: OpenStreetMap data rendered by CARTO "Dark Matter" raster tiles.
// Map rendering only — every route, distance and spatial query comes from the
// C++ engine (spec §2.4: rendering and route computation stay separate).
export const DELHI = { lon: 77.2167, lat: 28.6139 };

export const darkStyle = {
  version: 8,
  sources: {
    basemap: {
      type: 'raster',
      tiles: ['a', 'b', 'c', 'd'].map((s) => `https://${s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}.png`),
      tileSize: 256,
      attribution: '© <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors © <a href="https://carto.com/attributions">CARTO</a>',
      maxzoom: 19,
    },
  },
  layers: [
    { id: 'bg', type: 'background', paint: { 'background-color': '#05080d' } },
    { id: 'basemap', type: 'raster', source: 'basemap', paint: { 'raster-opacity': 0.92, 'raster-saturation': -0.2, 'raster-contrast': 0.08 } },
  ],
};

/** Polygon approximating a circle of `km` radius (for radius queries). */
export function circlePolygon(lat, lon, km, steps = 64) {
  const coords = [];
  for (let i = 0; i <= steps; i++) {
    const a = (i / steps) * 2 * Math.PI;
    const dLat = (km / 111.195) * Math.cos(a);
    const dLon = (km / (111.195 * Math.cos((lat * Math.PI) / 180))) * Math.sin(a);
    coords.push([lon + dLon, lat + dLat]);
  }
  return { type: 'Feature', geometry: { type: 'Polygon', coordinates: [coords] }, properties: {} };
}

export function rectPolygon(r) {
  const c = [[r.minLon, r.minLat], [r.maxLon, r.minLat], [r.maxLon, r.maxLat], [r.minLon, r.maxLat], [r.minLon, r.minLat]];
  return { type: 'Feature', geometry: { type: 'Polygon', coordinates: [c] }, properties: r };
}

export const fc = (features) => ({ type: 'FeatureCollection', features });
