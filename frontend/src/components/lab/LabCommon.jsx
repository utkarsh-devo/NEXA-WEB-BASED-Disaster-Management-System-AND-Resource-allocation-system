import { useCallback, useEffect, useRef, useState } from 'react';
import { motion } from 'framer-motion';
import { Pause, Play, RotateCcw, StepForward } from 'lucide-react';
import { api } from '../../services/api';
import { useToast } from '../../hooks/useToast';
import Complexity from '../Complexity';

/** Runs a lab module on the C++ engine. */
export function useLab(kind) {
  const toast = useToast();
  const [out, setOut] = useState(null);
  const [busy, setBusy] = useState(false);
  const run = useCallback(async (body) => {
    setBusy(true);
    try { const r = await api.lab(kind, body); setOut(r); return r; }
    catch (e) { toast.fromError(e, 'Engine rejected input'); return null; }
    finally { setBusy(false); }
  }, [kind, toast]);
  return { out, busy, run, setOut };
}

/** Measured stats, theoretical complexity and the step log side by side. */
export function LabResult({ out, children }) {
  if (!out) return <div className="empty">Run the operation to see the engine's output.</div>;
  return (
    <div className="stack" style={{ gap: 16 }}>
      {children}
      <div className="grid g-2">
        <Complexity title="Measured by the engine" items={out.stats} />
        <Complexity title="Theoretical complexity" items={out.complexity} />
      </div>
      {out.log && out.log.length > 0 && (
        <div className="mono" style={{ fontSize: 12, background: 'rgba(0,0,0,.35)', border: '1px solid var(--line)', borderRadius: 6, padding: 12, maxHeight: 220, overflow: 'auto', lineHeight: 1.7 }}>
          {out.log.map((l, i) => <motion.div key={i} initial={{ opacity: 0 }} animate={{ opacity: 1 }} transition={{ delay: Math.min(i * 0.02, 0.5) }}><span className="faint">{String(i + 1).padStart(2, '0')} </span>{l}</motion.div>)}
        </div>
      )}
    </div>
  );
}

/** Plays engine animation frames: { values, i, j, action }. */
export function FramePlayer({ frames = [], render, speed: initial = 60 }) {
  const [idx, setIdx] = useState(0);
  const [playing, setPlaying] = useState(true);
  const [speed, setSpeed] = useState(initial);
  const t = useRef(null);
  useEffect(() => { setIdx(0); setPlaying(true); }, [frames]);
  useEffect(() => {
    clearInterval(t.current);
    if (!playing || frames.length < 2) return undefined;
    t.current = setInterval(() => setIdx((i) => { if (i >= frames.length - 1) { setPlaying(false); return i; } return i + 1; }), Math.max(10, 400 - speed * 3.8));
    return () => clearInterval(t.current);
  }, [playing, speed, frames]);
  if (!frames.length) return null;
  const f = frames[Math.min(idx, frames.length - 1)];
  return (
    <div>
      {render(f)}
      <div className="row" style={{ marginTop: 10, gap: 8 }}>
        <button className="btn sm" onClick={() => setPlaying((p) => !p)}>{playing ? <Pause size={13} /> : <Play size={13} />}</button>
        <button className="btn sm" onClick={() => { setPlaying(false); setIdx((i) => Math.min(frames.length - 1, i + 1)); }}><StepForward size={13} /></button>
        <button className="btn sm" onClick={() => { setIdx(0); setPlaying(true); }}><RotateCcw size={13} /></button>
        <input type="range" className="range" style={{ maxWidth: 160 }} min={1} max={100} value={speed} onChange={(e) => setSpeed(Number(e.target.value))} title="speed" />
        <span className="mono faint" style={{ fontSize: 12 }}>frame {idx + 1}/{frames.length} · {f.action}{f.i >= 0 ? ` [${f.i}${f.j >= 0 ? `, ${f.j}` : ''}]` : ''}</span>
      </div>
    </div>
  );
}

/** Animated bar chart of an array frame. */
export function ArrayBars({ frame, showValues = true }) {
  const vals = frame.values || [];
  const max = Math.max(1, ...vals.map((v) => Math.abs(v)));
  return (
    <div className="bars">
      {vals.map((v, k) => (
        <motion.div key={k} layout className={`bar-col ${k === frame.i ? 'hi-i' : ''} ${k === frame.j ? 'hi-j' : ''}`}
          animate={{ height: `${Math.max(3, (Math.abs(v) / max) * 100)}%` }} transition={{ type: 'spring', stiffness: 500, damping: 40 }}>
          {showValues && vals.length <= 40 && <span className="bar-v">{v}</span>}
        </motion.div>
      ))}
    </div>
  );
}

/** Array of boxes (stack/queue/list snapshots). */
export function Cells({ values = [], highlightFirst = false, arrows = false, label }) {
  return (
    <div className="row wrap" style={{ gap: 6, alignItems: 'center' }}>
      {label && <span className="faint mono" style={{ fontSize: 11, marginRight: 4 }}>{label}</span>}
      {values.map((v, i) => (
        <span key={`${i}-${v}`} className="row" style={{ gap: 6 }}>
          <motion.span className={`cell ${highlightFirst && i === 0 ? 'top' : ''}`} initial={{ scale: 0.5, opacity: 0 }} animate={{ scale: 1, opacity: 1 }}>{v}</motion.span>
          {arrows && i < values.length - 1 && <span className="faint">→</span>}
        </span>
      ))}
      {values.length === 0 && <span className="faint">∅ empty</span>}
    </div>
  );
}

export const randomValues = (n = 24, max = 99) => Array.from({ length: n }, () => 1 + Math.floor(Math.random() * max));
