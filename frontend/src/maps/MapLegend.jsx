import { SEV_HEX } from '../services/format';

export default function MapLegend({ showRoads = false }) {
  return (
    <div className="map-overlay map-legend">
      <div><span className="legend-dot" style={{ background: SEV_HEX[5] }} />Incident (colour = severity)</div>
      <div><span className="legend-dot" style={{ background: '#2de2e6', borderRadius: 2, transform: 'rotate(45deg)' }} />Resource (grey = busy)</div>
      <div><span className="legend-dot" style={{ background: '#fff', borderRadius: 3 }} />Hospital</div>
      {showRoads && <div><span className="legend-dot" style={{ background: '#ff3d57', borderRadius: 1, height: 3 }} />Blocked road</div>}
      <div className="faint" style={{ fontSize: 10 }}>Shift + drag: region query</div>
    </div>
  );
}
