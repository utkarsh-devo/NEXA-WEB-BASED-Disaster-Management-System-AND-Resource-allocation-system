import { useState } from 'react';
import { motion } from 'framer-motion';
import { Field, NumberInput, Select, TextInput, parseNumbers } from '../Form';
import { LabResult, useLab } from './LabCommon';

const STRATS = [{ value: 'chaining', label: 'Separate chaining' }, { value: 'linear', label: 'Linear probing' }, { value: 'quadratic', label: 'Quadratic probing' }, { value: 'double', label: 'Double hashing' }];

export default function HashLab() {
  const [strategy, setStrategy] = useState('linear');
  const [keys, setKeys] = useState('12 44 13 88 23 94 11 39 20 16 5 27 101');
  const [deletes, setDeletes] = useState('');
  const [capacity, setCapacity] = useState(16);
  const lab = useLab('hash');
  const run = (s = strategy) => lab.run({ strategy: s, keys: parseNumbers(keys), deletes: parseNumbers(deletes), capacity });
  return (
    <div className="stack" style={{ gap: 16 }}>
      <div className="toolbar">
        <Field label="Collision strategy"><Select value={strategy} onChange={(s) => { setStrategy(s); run(s); }} options={STRATS} /></Field>
        <Field label="Keys"><TextInput mono value={keys} onChange={setKeys} style={{ minWidth: 320 }} /></Field>
        <Field label="Delete"><TextInput mono value={deletes} onChange={setDeletes} placeholder="tombstones…" /></Field>
        <Field label="Initial capacity (power of 2)"><NumberInput value={capacity} onChange={setCapacity} min={8} max={1024} /></Field>
        <button className="btn primary" disabled={lab.busy} onClick={() => run()}>Insert all</button>
      </div>
      <LabResult out={lab.out}>
        {lab.out && (
          <div className="grid" style={{ gridTemplateColumns: 'repeat(auto-fill, minmax(150px, 1fr))', gap: 6 }}>
            {lab.out.table.map(([slot, state, items], i) => (
              <motion.div key={slot} initial={{ opacity: 0, scale: 0.8 }} animate={{ opacity: 1, scale: 1 }} transition={{ delay: i * 0.008 }}
                className="row" style={{ gap: 8, padding: '6px 10px', border: '1px solid var(--line)', borderRadius: 6, background: state === 'occupied' ? 'var(--amber-soft)' : state === 'deleted' ? 'var(--red-soft)' : 'transparent' }}>
                <span className="mono faint" style={{ width: 28, fontSize: 11 }}>[{slot}]</span>
                <span className="mono" style={{ fontSize: 12, color: state === 'deleted' ? 'var(--red)' : undefined }}>{state === 'deleted' ? '✝ tomb' : items || '·'}</span>
              </motion.div>
            ))}
          </div>
        )}
      </LabResult>
    </div>
  );
}
