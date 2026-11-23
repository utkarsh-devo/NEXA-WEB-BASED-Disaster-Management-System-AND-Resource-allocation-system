import { useState } from 'react';
import { Field, NumberInput, Select, TextInput, parseNumbers } from '../Form';
import { Cells, FramePlayer, LabResult, useLab } from './LabCommon';
import TreeView from './TreeView';

const TYPES = [{ value: 'bst', label: 'Binary search tree' }, { value: 'avl', label: 'AVL tree' }, { value: 'rb', label: 'Red-black tree' }, { value: 'btree', label: 'B-tree' },
  { value: 'bplus', label: 'B+ tree' }, { value: 'threaded', label: 'Threaded binary tree' }, { value: 'kary', label: 'K-ary tree' }];

export function TreeLab() {
  const [type, setType] = useState('avl');
  const [values, setValues] = useState('50 30 70 20 40 60 80 10 25 35 45 5 1');
  const [deletes, setDeletes] = useState('');
  const [search, setSearch] = useState(35);
  const [order, setOrder] = useState(3);
  const lab = useLab('tree');
  const run = (t = type) => lab.run({ type: t, values: parseNumbers(values), deletes: parseNumbers(deletes), search, order });
  return (
    <div className="stack" style={{ gap: 16 }}>
      <div className="toolbar">
        <Field label="Tree"><Select value={type} onChange={(t) => { setType(t); run(t); }} options={TYPES} /></Field>
        <Field label="Insert (in order)"><TextInput mono value={values} onChange={setValues} style={{ minWidth: 320 }} /></Field>
        {type !== 'kary' && type !== 'threaded' && <Field label="Then delete"><TextInput mono value={deletes} onChange={setDeletes} placeholder="e.g. 30 70" /></Field>}
        <Field label="Search key"><NumberInput value={search} onChange={setSearch} /></Field>
        {(type === 'btree' || type === 'bplus' || type === 'kary') && <Field label={type === 'btree' ? 'Min degree t' : type === 'bplus' ? 'Max keys / node' : 'k'}><NumberInput value={order} onChange={setOrder} min={2} max={8} /></Field>}
        <button className="btn primary" disabled={lab.busy} onClick={() => run()}>Build</button>
        <button className="btn" onClick={() => { setValues(Array.from({ length: 15 }, (_, i) => i + 1).join(' ')); }}>Sorted input</button>
      </div>
      {type === 'bst' && <div className="note">Try “Sorted input”: the unbalanced BST degenerates into a list (height = n) — compare with the AVL / red-black trees.</div>}
      <LabResult out={lab.out}>{lab.out && <div className="panel" style={{ padding: 10 }}><TreeView nodes={lab.out.nodes} /></div>}</LabResult>
    </div>
  );
}

export function HeapLab() {
  const [mode, setMode] = useState('max');
  const [values, setValues] = useState('42 17 93 8 61 25 77 3 55 30');
  const [extract, setExtract] = useState(2);
  const lab = useLab('heap');
  const nodesOf = (vals) => vals.map((v, i) => ({ id: `i${i}`, parent: i ? `i${Math.floor((i - 1) / 2)}` : '', edge: i ? (i % 2 ? 'L' : 'R') : '', label: String(v) }));
  return (
    <div className="stack" style={{ gap: 16 }}>
      <div className="toolbar">
        <Field label="Heap"><Select value={mode} onChange={setMode} options={[{ value: 'max', label: 'Max-heap' }, { value: 'min', label: 'Min-heap' }]} /></Field>
        <Field label="Insert"><TextInput mono value={values} onChange={setValues} style={{ minWidth: 320 }} /></Field>
        <Field label="Then extract ×"><NumberInput value={extract} onChange={setExtract} min={0} /></Field>
        <button className="btn primary" disabled={lab.busy} onClick={() => lab.run({ mode, values: parseNumbers(values), extract })}>Run</button>
      </div>
      <LabResult out={lab.out}>
        {lab.out && <FramePlayer frames={lab.out.frames} speed={20} render={(f) => (
          <div className="stack">
            <TreeView nodes={nodesOf(f.values)} height={300} />
            <Cells values={f.values} highlightFirst label="array:" />
          </div>
        )} />}
      </LabResult>
    </div>
  );
}
