import { useState } from 'react';
import { motion } from 'framer-motion';
import { Field, NumberInput, Select } from '../Form';
import { LabResult, useLab } from './LabCommon';

const MAZE = [
  [1, 1, 0, 1, 1, 1, 1, 0],
  [0, 1, 0, 1, 0, 0, 1, 1],
  [1, 1, 1, 1, 0, 1, 0, 1],
  [1, 0, 0, 0, 1, 1, 0, 1],
  [1, 1, 1, 0, 1, 0, 1, 1],
  [0, 0, 1, 1, 1, 0, 1, 0],
  [1, 1, 0, 0, 1, 1, 1, 1],
  [1, 1, 1, 1, 0, 0, 0, 1],
];

export default function BacktrackLab() {
  const [problem, setProblem] = useState('maze');
  const [n, setN] = useState(8);
  const [grid, setGrid] = useState(MAZE);
  const lab = useLab('backtracking');
  const rows = grid.length, cols = grid[0].length;
  const path = new Set(problem === 'maze' && lab.out ? lab.out.result : []);
  const order = new Map(problem === 'maze' && lab.out ? lab.out.result.map((c, i) => [c, i]) : []);
  const toggle = (r, c) => setGrid((g) => g.map((row, i) => row.map((v, j) => (i === r && j === c && !(i === 0 && j === 0) && !(i === rows - 1 && j === cols - 1) ? 1 - v : v))));
  return (
    <div className="grid g-side">
      <div className="stack">
        <Field label="Problem"><Select value={problem} onChange={(p) => { setProblem(p); lab.setOut(null); }} options={[{ value: 'maze', label: 'Rat in a Maze (rescue robot)' }, { value: 'nqueens', label: 'N-Queens' }]} /></Field>
        {problem === 'nqueens' && <Field label="n"><NumberInput value={n} onChange={setN} min={1} max={12} /></Field>}
        {problem === 'maze' && <div className="note">Click cells to toggle walls. The robot starts top-left and must reach bottom-right (collapsed-building drill).</div>}
        <button className="btn primary" disabled={lab.busy} onClick={() => lab.run(problem === 'maze' ? { problem, grid: grid.flat(), rows, cols } : { problem, n })}>Solve</button>
      </div>
      <LabResult out={lab.out}>
        {problem === 'maze' ? (
          <div className="board" style={{ gridTemplateColumns: `repeat(${cols}, 34px)` }}>
            {grid.flatMap((row, r) => row.map((v, c) => {
              const idx = r * cols + c;
              const on = path.has(idx);
              return (
                <motion.div key={idx} onClick={() => toggle(r, c)} style={{ cursor: 'pointer', background: v === 0 ? '#1c2a3d' : on ? 'rgba(255,182,39,.25)' : 'rgba(255,255,255,.03)', border: '1px solid rgba(120,160,210,.08)' }}
                  animate={on ? { scale: [0.7, 1] } : {}} transition={{ delay: (order.get(idx) || 0) * 0.04 }}>
                  {on ? <span className="mono" style={{ fontSize: 11, color: 'var(--amber)' }}>{order.get(idx)}</span> : v === 0 ? '▦' : ''}
                </motion.div>
              );
            }))}
          </div>
        ) : lab.out && (
          <div className="board" style={{ gridTemplateColumns: `repeat(${n}, 34px)` }}>
            {lab.out.table.flatMap((row, r) => row.map((cell, c) => (
              <motion.div key={`${r}-${c}`} className={`${(r + c) % 2 ? 'dark' : ''} ${cell === 'Q' ? 'q' : ''}`} initial={cell === 'Q' ? { scale: 0 } : false} animate={{ scale: 1 }} transition={{ delay: r * 0.08 }}>{cell === 'Q' ? '♛' : ''}</motion.div>
            )))}
          </div>
        )}
      </LabResult>
    </div>
  );
}
