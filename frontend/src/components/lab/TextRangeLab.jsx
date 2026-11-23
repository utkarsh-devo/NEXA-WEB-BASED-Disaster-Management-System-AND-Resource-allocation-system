import { useState } from 'react';
import { motion } from 'framer-motion';
import { Field, NumberInput, Select, TextArea, TextInput, parseNumbers } from '../Form';
import { ArrayBars, LabResult, useLab } from './LabCommon';

const CORPUS = 'building collapse near market, people trapped under debris. gas leak near school, residents evacuated. flood water rising in low lying colony, families trapped on rooftops. fire in warehouse spreading to market lane';

export function TextLab() {
  const [structure, setStructure] = useState('suffix_array');
  const [corpus, setCorpus] = useState(CORPUS);
  const [pattern, setPattern] = useState('trapped');
  const lab = useLab('text');
  const run = (s = structure) => lab.run({ structure: s, corpus, pattern });
  const highlighted = () => {
    if (!lab.out || !lab.out.result.length || structure === 'trie') return corpus;
    const marks = new Set(); lab.out.result.forEach((o) => { for (let i = 0; i < pattern.length; i++) marks.add(o + i); });
    return corpus.split('').map((ch, i) => (marks.has(i) ? <mark key={i} style={{ background: 'var(--amber)', color: '#111' }}>{ch}</mark> : ch));
  };
  return (
    <div className="stack" style={{ gap: 16 }}>
      <div className="toolbar">
        <Field label="Structure"><Select value={structure} onChange={(s) => { setStructure(s); run(s); }} options={[{ value: 'trie', label: 'Trie (prefix)' }, { value: 'suffix_array', label: 'Suffix array + LCP' }, { value: 'suffix_tree', label: 'Suffix tree' }]} /></Field>
        <Field label={structure === 'trie' ? 'Prefix' : 'Pattern'}><TextInput mono value={pattern} onChange={setPattern} /></Field>
        <button className="btn primary" disabled={lab.busy} onClick={() => run()}>Search</button>
      </div>
      <Field label="Corpus"><TextArea value={corpus} onChange={setCorpus} /></Field>
      <LabResult out={lab.out}>
        {lab.out && (
          <>
            {structure !== 'trie' && <div className="panel" style={{ padding: 12, lineHeight: 1.7 }}>{highlighted()}</div>}
            {structure === 'suffix_array' && (
              <div style={{ maxHeight: 280, overflow: 'auto' }}>
                <table className="data mono" style={{ fontSize: 11 }}><thead><tr><th>rank</th><th>SA[i]</th><th>LCP</th><th>suffix</th></tr></thead>
                  <tbody>{lab.out.table.map((r) => <tr key={r[0]}><td>{r[0]}</td><td>{r[1]}</td><td>{r[2]}</td><td>{r[3]}</td></tr>)}</tbody></table>
              </div>
            )}
          </>
        )}
      </LabResult>
    </div>
  );
}

export function RangeLab() {
  const [intervals, setIntervals] = useState('9 12  10 14  13 18  20 23  1 4  15 30');
  const [qFrom, setQFrom] = useState(11);
  const [qTo, setQTo] = useState(16);
  const [values, setValues] = useState('3 1 4 1 5 9 2 6 5 3 5 8');
  const [seg, setSeg] = useState({ l: 2, r: 7, addL: 0, addR: 4, add: 10 });
  const it = useLab('interval');
  const st = useLab('segment');
  const pairs = parseNumbers(intervals);
  const maxT = Math.max(24, ...pairs);
  return (
    <div className="grid g-2">
      <div className="stack">
        <div className="faint" style={{ fontSize: 11, letterSpacing: '.14em' }}>INTERVAL TREE — resource commitments / bed reservations</div>
        <Field label="Intervals (start end …)"><TextInput mono value={intervals} onChange={setIntervals} /></Field>
        <div className="form-grid"><Field label="Query from"><NumberInput value={qFrom} onChange={setQFrom} /></Field><Field label="Query to"><NumberInput value={qTo} onChange={setQTo} /></Field></div>
        <button className="btn primary" onClick={() => it.run({ intervals: pairs, from: qFrom, to: qTo })}>Find overlaps</button>
        <LabResult out={it.out}>
          {it.out && (
            <svg viewBox="0 0 400 200" style={{ width: '100%' }}>
              <rect x={(qFrom / maxT) * 380 + 10} y={0} width={((qTo - qFrom) / maxT) * 380} height={200} fill="rgba(155,140,255,.12)" stroke="#9b8cff" strokeDasharray="4 3" />
              {it.out.table.map(([id, s, e], i) => {
                const hit = it.out.result.includes(Number(id));
                return <motion.rect key={id} x={(Number(s) / maxT) * 380 + 10} y={12 + i * 22} height={14} rx={4} fill={hit ? '#ffb627' : 'rgba(45,226,230,.4)'}
                  initial={{ width: 0 }} animate={{ width: ((Number(e) - Number(s)) / maxT) * 380 }} transition={{ delay: i * 0.05 }} />;
              })}
            </svg>
          )}
        </LabResult>
      </div>
      <div className="stack">
        <div className="faint" style={{ fontSize: 11, letterSpacing: '.14em' }}>SEGMENT TREE — analytics time buckets</div>
        <Field label="Values"><TextInput mono value={values} onChange={setValues} /></Field>
        <div className="form-grid">
          <Field label="Query l"><NumberInput value={seg.l} onChange={(v) => setSeg((s) => ({ ...s, l: v }))} /></Field>
          <Field label="Query r"><NumberInput value={seg.r} onChange={(v) => setSeg((s) => ({ ...s, r: v }))} /></Field>
          <Field label="Range add [a..b]"><TextInput mono value={`${seg.addL} ${seg.addR}`} onChange={(v) => { const [a, b] = parseNumbers(v); setSeg((s) => ({ ...s, addL: a ?? 0, addR: b ?? 0 })); }} /></Field>
          <Field label="Add value (lazy)"><NumberInput value={seg.add} onChange={(v) => setSeg((s) => ({ ...s, add: v }))} /></Field>
        </div>
        <button className="btn primary" onClick={() => st.run({ values: parseNumbers(values), ...seg })}>Query & update</button>
        <LabResult out={st.out}>{st.out && <ArrayBars frame={{ values: st.out.result, i: -1, j: -1 }} />}</LabResult>
      </div>
    </div>
  );
}
