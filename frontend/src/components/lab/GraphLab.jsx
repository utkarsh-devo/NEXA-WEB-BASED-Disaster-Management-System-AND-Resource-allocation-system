import { useMemo, useState } from 'react';
import { motion } from 'framer-motion';
import { Field, NumberInput, Select, TextInput } from '../Form';
import { LabResult, useLab } from './LabCommon';

const DEFAULT_EDGES = '0-1:4 0-2:1 2-1:2 1-3:5 2-3:8 3-4:3 4-5:1 2-5:10 6-7:2';
const parseEdges = (t) => t.split(/[\s,]+/).filter(Boolean).map((e) => { const m = e.match(/^(\d+)-(\d+)(?::([\d.]+))?$/); return m ? { u: Number(m[1]), v: Number(m[2]), w: m[3] ? Number(m[3]) : 1 } : null; }).filter(Boolean);
const COLORS = ['#ffb627', '#2de2e6', '#ff3d57', '#3ddc97', '#9b8cff', '#ff7a3d'];

/** Circular layout; result edges (tree / MST / path) highlighted. */
function GraphView({ n, edges, nodes, path }) {
  const R = 150, cx = 200, cy = 180;
  const pos = (i) => ({ x: cx + R * Math.cos((2 * Math.PI * i) / n - Math.PI / 2), y: cy + R * Math.sin((2 * Math.PI * i) / n - Math.PI / 2) });
  const parent = new Map((nodes || []).map((x) => [Number(x.id), x.parent === '' ? -1 : Number(x.parent)]));
  const color = new Map((nodes || []).map((x) => [Number(x.id), x.color]));
  const onPath = (u, v) => { for (let i = 0; i + 1 < path.length; i++) if ((path[i] === u && path[i + 1] === v) || (path[i] === v && path[i + 1] === u)) return true; return false; };
  const treeEdge = (u, v) => parent.get(v) === u || parent.get(u) === v;
  return (
    <svg viewBox="0 0 400 360" style={{ width: '100%', maxWidth: 520 }} className="svg-tree">
      {edges.map((e, i) => {
        const a = pos(e.u), b = pos(e.v);
        const hl = path.length ? onPath(e.u, e.v) : treeEdge(e.u, e.v);
        return (
          <g key={i}>
            <motion.line x1={a.x} y1={a.y} x2={b.x} y2={b.y} stroke={hl ? '#ffb627' : 'rgba(120,170,230,.25)'} strokeWidth={hl ? 3.2 : 1.4}
              initial={{ pathLength: 0 }} animate={{ pathLength: 1 }} transition={{ duration: 0.6, delay: i * 0.03 }} />
            <text x={(a.x + b.x) / 2} y={(a.y + b.y) / 2 - 4} textAnchor="middle" style={{ fontSize: 10, fill: '#93a3b8' }}>{e.w}</text>
          </g>
        );
      })}
      {Array.from({ length: n }, (_, i) => {
        const p = pos(i); const c = color.get(i) || '';
        const fill = c.startsWith('c') ? COLORS[Number(c.slice(1)) % COLORS.length] : undefined;
        return (
          <motion.g key={i} initial={{ scale: 0 }} animate={{ scale: 1 }} transition={{ delay: 0.2 + i * 0.04, type: 'spring' }} style={{ transformOrigin: `${p.x}px ${p.y}px` }}>
            <circle cx={p.x} cy={p.y} r={16} className={`node ${c === 'unreached' ? 'unreached' : c ? 'visited' : ''}`} style={fill ? { stroke: fill } : undefined} />
            <text x={p.x} y={p.y + 4} textAnchor="middle">V{i}</text>
          </motion.g>
        );
      })}
    </svg>
  );
}

export default function GraphLab() {
  const [vertices, setVertices] = useState(8);
  const [edgeText, setEdgeText] = useState(DEFAULT_EDGES);
  const [algorithm, setAlgorithm] = useState('dijkstra');
  const [source, setSource] = useState(0);
  const [target, setTarget] = useState(5);
  const lab = useLab('graph');
  const edges = useMemo(() => parseEdges(edgeText), [edgeText]);
  const run = (a = algorithm) => lab.run({ vertices, edges, algorithm: a, source, target });
  return (
    <div className="stack" style={{ gap: 16 }}>
      <div className="toolbar">
        <Field label="Algorithm"><Select value={algorithm} onChange={(a) => { setAlgorithm(a); run(a); }} options={[{ value: 'bfs', label: 'BFS' }, { value: 'dfs', label: 'DFS' }, { value: 'dijkstra', label: 'Dijkstra' }, { value: 'kruskal', label: 'Kruskal MST' }, { value: 'prim', label: 'Prim MST' }, { value: 'components', label: 'Connected components' }]} /></Field>
        <Field label="Vertices"><NumberInput value={vertices} onChange={setVertices} min={1} max={60} /></Field>
        <Field label="Edges u-v:w"><TextInput mono value={edgeText} onChange={setEdgeText} style={{ minWidth: 340 }} /></Field>
        <Field label="Source"><NumberInput value={source} onChange={setSource} min={0} /></Field>
        {algorithm === 'dijkstra' && <Field label="Target"><NumberInput value={target} onChange={setTarget} min={0} /></Field>}
        <button className="btn primary" disabled={lab.busy} onClick={() => run()}>Run</button>
      </div>
      <LabResult out={lab.out}>
        {lab.out && (
          <div className="grid g-2">
            <GraphView n={vertices} edges={edges} nodes={lab.out.nodes} path={algorithm === 'dijkstra' ? lab.out.result : []} />
            <div>
              {lab.out.table.length > 0 && (
                <>
                  <div className="faint" style={{ fontSize: 11, letterSpacing: '.14em', marginBottom: 6 }}>ADJACENCY MATRIX (built from the list)</div>
                  <div style={{ overflow: 'auto' }}>
                    <table className="data mono" style={{ fontSize: 11 }}>
                      <thead><tr><th />{lab.out.table.map((_, i) => <th key={i}>V{i}</th>)}</tr></thead>
                      <tbody>{lab.out.table.map((row, i) => <tr key={i}><td className="faint">V{i}</td>{row.map((c, j) => <td key={j} style={{ color: c === '∞' ? 'var(--text-faint)' : 'var(--cyan)' }}>{c}</td>)}</tr>)}</tbody>
                    </table>
                  </div>
                </>
              )}
            </div>
          </div>
        )}
      </LabResult>
    </div>
  );
}
