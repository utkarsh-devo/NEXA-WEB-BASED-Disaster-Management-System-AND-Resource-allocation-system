import { useState } from 'react';
import { motion } from 'framer-motion';
import { Field, NumberInput, Select } from '../Form';
import { LabResult, useLab } from './LabCommon';

const BOX = { minLat: 28.40, maxLat: 28.85, minLon: 76.95, maxLon: 77.55 };

/** Scatter of generated points; hits glow. Quad-tree cells drawn when present. */
function Scatter({ out, q, radiusKm, structure }) {
  const W = 520, H = 400;
  const x = (lon) => ((lon - BOX.minLon) / (BOX.maxLon - BOX.minLon)) * W;
  const y = (lat) => H - ((lat - BOX.minLat) / (BOX.maxLat - BOX.minLat)) * H;
  const rLat = radiusKm / 111.195, rLon = radiusKm / (111.195 * Math.cos((q.lat * Math.PI) / 180));
  return (
    <svg viewBox={`0 0 ${W} ${H}`} style={{ width: '100%', background: 'rgba(0,0,0,.3)', borderRadius: 8, border: '1px solid var(--line)' }}>
      {structure === 'quad' && out.table.map((c, i) => {
        const [minLat, minLon, maxLat, maxLon] = c.map(Number);
        return <rect key={i} x={x(minLon)} y={y(maxLat)} width={x(maxLon) - x(minLon)} height={y(minLat) - y(maxLat)} fill="none" stroke="rgba(255,182,39,.18)" />;
      })}
      {structure !== 'kd' && <rect x={x(q.lon - rLon)} y={y(q.lat + rLat)} width={x(q.lon + rLon) - x(q.lon - rLon)} height={y(q.lat - rLat) - y(q.lat + rLat)} fill="rgba(155,140,255,.1)" stroke="#9b8cff" strokeDasharray="4 3" />}
      {out.nodes.map((n, i) => (
        <motion.circle key={n.id} cx={x(Number(n.edge))} cy={y(Number(n.parent))} r={n.color === 'hit' ? 4 : 1.8} fill={n.color === 'hit' ? '#ffb627' : 'rgba(45,226,230,.55)'}
          initial={{ opacity: 0 }} animate={{ opacity: 1 }} transition={{ delay: n.color === 'hit' ? 0.4 + i * 0.002 : 0 }} />
      ))}
      <circle cx={x(q.lon)} cy={y(q.lat)} r={6} fill="#ff3d57" />
      <circle cx={x(q.lon)} cy={y(q.lat)} r={12} fill="none" stroke="#ff3d57"><animate attributeName="r" values="6;20" dur="1.6s" repeatCount="indefinite" /><animate attributeName="opacity" values="1;0" dur="1.6s" repeatCount="indefinite" /></circle>
    </svg>
  );
}

export default function SpatialLab() {
  const [structure, setStructure] = useState('kd');
  const [points, setPoints] = useState(800);
  const [seed, setSeed] = useState(7);
  const [k, setK] = useState(8);
  const [radiusKm, setRadiusKm] = useState(6);
  const [q, setQ] = useState({ lat: 28.61, lon: 77.21 });
  const lab = useLab('spatial');
  const run = (s = structure) => lab.run({ structure: s, points, seed, lat: q.lat, lon: q.lon, radiusKm, k });
  return (
    <div className="grid g-side">
      <div className="stack">
        <Field label="Structure"><Select value={structure} onChange={(s) => { setStructure(s); run(s); }} options={[{ value: 'kd', label: 'KD-tree (k-nearest)' }, { value: 'quad', label: 'Quad-tree (box)' }, { value: 'range', label: 'Range tree (box)' }]} /></Field>
        <div className="form-grid">
          <Field label="Points"><NumberInput value={points} onChange={setPoints} min={10} max={3000} /></Field>
          <Field label="Seed"><NumberInput value={seed} onChange={setSeed} /></Field>
          {structure === 'kd' ? <Field label="k"><NumberInput value={k} onChange={setK} min={1} max={50} /></Field> : <Field label="Half-width km"><NumberInput value={radiusKm} onChange={setRadiusKm} min={1} max={30} /></Field>}
          <Field label="Query lat"><NumberInput step="0.01" value={q.lat} onChange={(v) => setQ((p) => ({ ...p, lat: v }))} /></Field>
          <Field label="Query lon"><NumberInput step="0.01" value={q.lon} onChange={(v) => setQ((p) => ({ ...p, lon: v }))} /></Field>
        </div>
        <button className="btn primary" disabled={lab.busy} onClick={() => run()}>Build & query</button>
        <div className="note">Results are verified against a brute-force scan by the engine (“agrees” rows).</div>
      </div>
      <LabResult out={lab.out}>{lab.out && <Scatter out={lab.out} q={q} radiusKm={radiusKm} structure={structure} />}</LabResult>
    </div>
  );
}
