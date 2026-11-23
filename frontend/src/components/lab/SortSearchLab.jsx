import { useState } from 'react';
import { Shuffle } from 'lucide-react';
import { Field, NumberInput, Select, TextInput, parseNumbers } from '../Form';
import { ArrayBars, FramePlayer, LabResult, randomValues, useLab } from './LabCommon';

const SORTS = [{ value: 'merge', label: 'Merge sort' }, { value: 'quick', label: 'Quick sort (median-of-3)' }, { value: 'heap', label: 'Heap sort' }, { value: 'insertion', label: 'Insertion sort' },
  { value: 'counting', label: 'Counting sort' }, { value: 'radix', label: 'Radix sort (LSD)' }, { value: 'bucket', label: 'Bucket sort' }];

export function SortLab() {
  const [algorithm, setAlgorithm] = useState('quick');
  const [text, setText] = useState(randomValues(28).join(' '));
  const lab = useLab('sort');
  return (
    <div className="stack" style={{ gap: 16 }}>
      <div className="toolbar">
        <Field label="Algorithm"><Select value={algorithm} onChange={setAlgorithm} options={SORTS} /></Field>
        <Field label="Values (≤ 300)"><TextInput mono value={text} onChange={setText} style={{ minWidth: 420 }} /></Field>
        <button className="btn" onClick={() => setText(randomValues(28).join(' '))}><Shuffle size={14} />Random</button>
        <button className="btn" onClick={() => setText(Array.from({ length: 28 }, (_, i) => i + 1).join(' '))}>Sorted</button>
        <button className="btn" onClick={() => setText(Array.from({ length: 28 }, (_, i) => 28 - i).join(' '))}>Reversed</button>
        <button className="btn primary" disabled={lab.busy} onClick={() => lab.run({ algorithm, values: parseNumbers(text) })}>Run in C++</button>
      </div>
      <LabResult out={lab.out}>{lab.out && <FramePlayer frames={lab.out.frames} render={(f) => <ArrayBars frame={f} />} />}</LabResult>
    </div>
  );
}

export function SearchLab() {
  const [algorithm, setAlgorithm] = useState('binary');
  const [text, setText] = useState(randomValues(24).join(' '));
  const [target, setTarget] = useState(42);
  const lab = useLab('search');
  return (
    <div className="stack" style={{ gap: 16 }}>
      <div className="toolbar">
        <Field label="Algorithm"><Select value={algorithm} onChange={setAlgorithm} options={[{ value: 'linear', label: 'Linear search' }, { value: 'binary', label: 'Binary search' }, { value: 'median', label: 'Median search (median-of-medians)' }, { value: 'quickselect', label: 'Quick-select (k-th)' }]} /></Field>
        <Field label="Values"><TextInput mono value={text} onChange={setText} style={{ minWidth: 380 }} /></Field>
        <Field label={algorithm === 'quickselect' ? 'k (0-based)' : 'Target'}><NumberInput value={target} onChange={setTarget} /></Field>
        <button className="btn primary" disabled={lab.busy} onClick={() => lab.run({ algorithm, values: parseNumbers(text), target })}>Search</button>
      </div>
      {algorithm === 'median' && <div className="note">Median Search here = selection of the median (k-th order statistic) with the median-of-medians pivot rule — O(n) worst case without sorting. If your syllabus defines it differently, only <span className="mono">medianSearch()</span> in Searching.hpp needs to change.</div>}
      <LabResult out={lab.out}>{lab.out && <FramePlayer frames={lab.out.frames.length ? lab.out.frames : [{ values: lab.out.result, i: -1, j: -1, action: 'result' }]} speed={30} render={(f) => <ArrayBars frame={f} />} />}</LabResult>
    </div>
  );
}
