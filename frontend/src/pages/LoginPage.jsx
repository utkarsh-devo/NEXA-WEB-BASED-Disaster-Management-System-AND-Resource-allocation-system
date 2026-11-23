import { useState } from 'react';
import { motion } from 'framer-motion';
import { KeyRound, LogIn, ShieldCheck } from 'lucide-react';
import { useAuth } from '../hooks/useAuth';
import { Field, TextInput } from '../components/Form';

const DEMO = [
  { u: 'admin', p: 'Nexa@2026', role: 'Administrator' },
  { u: 'operator', p: 'Operator@2026', role: 'Incident Operator' },
  { u: 'coordinator', p: 'Coord@2026', role: 'Resource Coordinator' },
  { u: 'medic', p: 'Medic@2026', role: 'Medical Coordinator' },
  { u: 'viewer', p: 'Viewer@2026', role: 'Read-only Viewer' },
];

/** Animated radar-and-grid art panel. */
function RadarArt() {
  const blips = [[120, 140, 5], [260, 210, 4], [180, 300, 3], [330, 120, 5], [90, 260, 2], [300, 330, 4], [220, 90, 3]];
  return (
    <svg viewBox="0 0 420 420" style={{ width: 'min(70vh, 90%)', height: 'auto' }}>
      <defs>
        <radialGradient id="rg" cx="50%" cy="50%" r="50%"><stop offset="0%" stopColor="#2de2e6" stopOpacity=".18" /><stop offset="100%" stopColor="#2de2e6" stopOpacity="0" /></radialGradient>
        <linearGradient id="sweep" x1="0" y1="0" x2="1" y2="0"><stop offset="0%" stopColor="#2de2e6" stopOpacity="0" /><stop offset="100%" stopColor="#2de2e6" stopOpacity=".55" /></linearGradient>
      </defs>
      <circle cx="210" cy="210" r="200" fill="url(#rg)" />
      {[50, 100, 150, 200].map((r) => <circle key={r} cx="210" cy="210" r={r} fill="none" stroke="rgba(45,226,230,.22)" />)}
      <line x1="10" y1="210" x2="410" y2="210" stroke="rgba(45,226,230,.15)" />
      <line x1="210" y1="10" x2="210" y2="410" stroke="rgba(45,226,230,.15)" />
      <g>
        <path d="M210 210 L410 210 A200 200 0 0 0 351 69 Z" fill="url(#sweep)" />
        <animateTransform attributeName="transform" type="rotate" from="0 210 210" to="360 210 210" dur="5s" repeatCount="indefinite" />
      </g>
      {blips.map(([x, y, s], i) => (
        <g key={i}>
          <circle cx={x} cy={y} r={3 + s} fill={s >= 5 ? '#ff3d57' : s >= 4 ? '#ff7a3d' : '#ffb627'}>
            <animate attributeName="opacity" values="0.2;1;0.2" dur={`${2 + (i % 3)}s`} repeatCount="indefinite" begin={`${i * 0.4}s`} />
          </circle>
          <circle cx={x} cy={y} r={4} fill="none" stroke={s >= 5 ? '#ff3d57' : '#ffb627'}>
            <animate attributeName="r" values="4;22" dur="2.4s" repeatCount="indefinite" begin={`${i * 0.4}s`} />
            <animate attributeName="opacity" values="0.8;0" dur="2.4s" repeatCount="indefinite" begin={`${i * 0.4}s`} />
          </circle>
        </g>
      ))}
    </svg>
  );
}

export default function LoginPage() {
  const { login } = useAuth();
  const [username, setUsername] = useState('admin');
  const [password, setPassword] = useState('');
  const [error, setError] = useState('');
  const [busy, setBusy] = useState(false);

  const submit = async (e) => {
    e.preventDefault();
    setBusy(true); setError('');
    try { await login(username, password); }
    catch (err) { setError(err.message); }
    finally { setBusy(false); }
  };

  return (
    <div className="login-wrap">
      <div className="scanline" />
      <div className="login-art">
        <div style={{ position: 'absolute', inset: 0, display: 'grid', placeItems: 'center' }}><RadarArt /></div>
        <motion.div initial={{ opacity: 0, x: -20 }} animate={{ opacity: 1, x: 0 }} transition={{ delay: 0.2, duration: 0.6 }}
          style={{ position: 'absolute', left: 48, bottom: 48, maxWidth: 520 }}>
          <div className="page-kicker">Emergency Operations Platform</div>
          <h1 style={{ fontSize: 54, letterSpacing: '.28em', fontWeight: 700 }}>NEXA</h1>
          <p className="muted" style={{ lineHeight: 1.6, fontSize: 15 }}>
            Disaster response, resource allocation, spatial search and network routing — powered by a C++ engine of hand-built data structures and algorithms.
          </p>
          <div className="row wrap" style={{ marginTop: 14 }}>
            {['AVL · Heap · Hash', 'KD · Quad · Range Trees', 'Dijkstra · Kruskal · Prim', 'Trie · Suffix Array'].map((t) => <span key={t} className="chip">{t}</span>)}
          </div>
        </motion.div>
      </div>
      <div className="login-form">
        <motion.form onSubmit={submit} className="panel" style={{ width: 'min(420px, 100%)' }}
          initial={{ opacity: 0, y: 24 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.55, ease: [0.22, 1, 0.36, 1] }}>
          <header className="panel-head"><h3 className="panel-title"><ShieldCheck size={15} />Secure operator sign-in</h3></header>
          <div className="panel-body stack" style={{ gap: 14 }}>
            <Field label="Username"><TextInput value={username} onChange={setUsername} autoComplete="username" autoFocus /></Field>
            <Field label="Password"><TextInput type="password" value={password} onChange={setPassword} autoComplete="current-password" /></Field>
            {error && <motion.div className="note red" initial={{ opacity: 0, x: -6 }} animate={{ opacity: 1, x: [0, -6, 6, -4, 4, 0] }}>{error}</motion.div>}
            <button className="btn primary" disabled={busy || !username || !password} style={{ height: 42 }}>
              <LogIn size={16} />{busy ? 'Authenticating…' : 'Enter command console'}
            </button>
            <div className="divider" />
            <div className="faint" style={{ fontSize: 11, letterSpacing: '.14em', textTransform: 'uppercase' }}><KeyRound size={12} /> Demo accounts (seeded)</div>
            <div className="stack" style={{ gap: 6 }}>
              {DEMO.map((d) => (
                <button type="button" key={d.u} className="btn sm ghost" style={{ justifyContent: 'space-between', border: '1px solid var(--line)' }}
                  onClick={() => { setUsername(d.u); setPassword(d.p); }}>
                  <span className="mono">{d.u}</span><span className="faint">{d.role}</span>
                </button>
              ))}
            </div>
          </div>
        </motion.form>
      </div>
    </div>
  );
}
