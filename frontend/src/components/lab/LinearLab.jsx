import { useState } from 'react';
import { Field, NumberInput, Select, TextInput, parseNumbers } from '../Form';
import { Cells, LabResult, useLab } from './LabCommon';

const OPS = {
  stack: ['push', 'pop', 'peek'],
  queue: ['enqueue', 'dequeue', 'front', 'rear'],
  linkedlist: ['insertBack', 'insertFront', 'insertAt', 'remove', 'search'],
  array: ['append', 'insertAt', 'removeAt', 'update', 'search'],
};
const SEED = {
  stack: [['push', 12], ['push', 7], ['push', 31], ['pop'], ['push', 5], ['peek']],
  queue: [['enqueue', 101], ['enqueue', 102], ['enqueue', 103], ['dequeue'], ['enqueue', 104], ['front']],
  linkedlist: [['insertBack', 10], ['insertBack', 20], ['insertFront', 5], ['insertAt', 15, 2], ['search', 15], ['remove', 10]],
  array: [['append', 4], ['append', 8], ['append', 15], ['insertAt', 6, 1], ['update', 99, 0], ['removeAt', 0, 2]],
};

export function LinearLab() {
  const [structure, setStructure] = useState('stack');
  const [ops, setOps] = useState(SEED.stack.map(([op, value, position]) => ({ op, value: value ?? 0, position: position ?? -1 })));
  const [op, setOp] = useState('push');
  const [value, setValue] = useState(42);
  const [position, setPosition] = useState(0);
  const lab = useLab('linear');
  const pick = (s) => { setStructure(s); setOp(OPS[s][0]); const seed = SEED[s].map(([o, v, p]) => ({ op: o, value: v ?? 0, position: p ?? -1 })); setOps(seed); lab.setOut(null); };
  const add = () => { const next = [...ops, { op, value, position }]; setOps(next); lab.run({ structure, ops: next }); };

  return (
    <div className="grid g-side">
      <div className="stack">
        <Field label="Structure"><Select value={structure} onChange={pick} options={[{ value: 'stack', label: 'Stack (action history)' }, { value: 'queue', label: 'Queue (FIFO requests)' }, { value: 'linkedlist', label: 'Doubly linked list (timeline)' }, { value: 'array', label: 'Dynamic array (registry)' }]} /></Field>
        <div className="form-grid">
          <Field label="Operation"><Select value={op} onChange={setOp} options={OPS[structure]} /></Field>
          <Field label="Value"><NumberInput value={value} onChange={setValue} /></Field>
          {(op === 'insertAt' || op === 'removeAt' || op === 'update') && <Field label="Position"><NumberInput value={position} onChange={setPosition} min={0} /></Field>}
        </div>
        <div className="row"><button className="btn primary" onClick={add} disabled={lab.busy}>Apply operation</button><button className="btn" onClick={() => lab.run({ structure, ops })}>Replay sequence</button><button className="btn ghost" onClick={() => { setOps([]); lab.setOut(null); }}>Clear</button></div>
        <div className="faint mono" style={{ fontSize: 11 }}>{ops.map((o) => `${o.op}(${['pop', 'peek', 'dequeue', 'front', 'rear'].includes(o.op) ? '' : o.value})`).join(' → ') || 'no operations'}</div>
      </div>
      <LabResult out={lab.out}>
        {lab.out && (
          <div className="stack">
            {lab.out.frames.map((f, i) => (
              <div key={i} className="row" style={{ gap: 12 }}>
                <span className="mono faint" style={{ width: 90, fontSize: 11 }}>{i + 1}. {f.action}</span>
                <Cells values={f.values} highlightFirst={structure === 'stack' || structure === 'queue'} arrows={structure === 'linkedlist'} label={structure === 'stack' ? 'top→' : structure === 'queue' ? 'front→' : ''} />
              </div>
            ))}
          </div>
        )}
      </LabResult>
    </div>
  );
}

export function SparseLab() {
  const [rows, setRows] = useState(1000);
  const [cols, setCols] = useState(500);
  const [entries, setEntries] = useState('3,17,2  3,40,1  8,17,5  120,2,1  499,499,3');
  const [links, setLinks] = useState('1,101 1,102 2,101 3,103 3,101 2,104');
  const sparse = useLab('sparse');
  const multi = useLab('multilist');
  return (
    <div className="grid g-2">
      <div className="stack">
        <div className="form-grid">
          <Field label="Rows"><NumberInput value={rows} onChange={setRows} /></Field>
          <Field label="Cols"><NumberInput value={cols} onChange={setCols} /></Field>
          <Field label="Entries (row,col,value …)" full><TextInput mono value={entries} onChange={setEntries} /></Field>
        </div>
        <button className="btn primary" onClick={() => sparse.run({ rows, cols, entries: parseNumbers(entries) })}>Build sparse matrix</button>
        <LabResult out={sparse.out}>
          {sparse.out && <table className="data"><thead><tr><th>row</th><th>col</th><th>value</th></tr></thead><tbody>{sparse.out.table.map((r, i) => <tr key={i}>{r.map((c, j) => <td key={j} className="num">{c}</td>)}</tr>)}</tbody></table>}
        </LabResult>
      </div>
      <div className="stack">
        <Field label="Incident,resource links" hint="multi-linked list: each node is on an incident chain AND a resource chain"><TextInput mono value={links} onChange={setLinks} /></Field>
        <button className="btn primary" onClick={() => multi.run({ links: parseNumbers(links) })}>Build multi-linked list</button>
        <LabResult out={multi.out} />
      </div>
    </div>
  );
}
