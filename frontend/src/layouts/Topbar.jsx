import { useEffect, useRef, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { AnimatePresence, motion } from 'framer-motion';
import { LogOut, Menu, Search, Wifi, WifiOff } from 'lucide-react';
import { api } from '../services/api';
import { useAuth } from '../hooks/useAuth';
import { useLiveStatus } from '../hooks/useLiveFeed';

/** Global search: Trie prefix suggestions from the C++ engine (incidents + resources). */
function GlobalSearch() {
  const [q, setQ] = useState('');
  const [items, setItems] = useState([]);
  const [open, setOpen] = useState(false);
  const [cursor, setCursor] = useState(0);
  const nav = useNavigate();
  const inputRef = useRef(null);

  useEffect(() => {
    const onKey = (e) => { if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'k') { e.preventDefault(); inputRef.current?.focus(); } };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, []);

  useEffect(() => {
    if (q.trim().length < 2) { setItems([]); return undefined; }
    const t = setTimeout(async () => {
      try {
        const [inc, res] = await Promise.all([api.suggestIncidents(q), api.suggestResources(q)]);
        setItems([
          ...inc.map((m) => ({ kind: 'incident', word: m.word, ids: m.ids })),
          ...res.map((m) => ({ kind: 'resource', word: m.word, ids: m.ids })),
        ].slice(0, 10));
        setCursor(0);
      } catch { setItems([]); }
    }, 160);
    return () => clearTimeout(t);
  }, [q]);

  const go = (it) => {
    setOpen(false); setQ('');
    if (it.ids.length === 1) nav(it.kind === 'incident' ? `/incidents/${it.ids[0]}` : `/resources/${it.ids[0]}`);
    else nav(it.kind === 'incident' ? `/incidents?prefix=${encodeURIComponent(it.word)}` : `/resources?prefix=${encodeURIComponent(it.word)}`);
  };

  return (
    <div className="search-box">
      <Search size={15} className="lead" />
      <input ref={inputRef} className="input" placeholder="Search incidents, units, places…" value={q}
        onChange={(e) => { setQ(e.target.value); setOpen(true); }} onFocus={() => setOpen(true)} onBlur={() => setTimeout(() => setOpen(false), 150)}
        onKeyDown={(e) => {
          if (e.key === 'ArrowDown') setCursor((c) => Math.min(items.length - 1, c + 1));
          if (e.key === 'ArrowUp') setCursor((c) => Math.max(0, c - 1));
          if (e.key === 'Enter' && items[cursor]) go(items[cursor]);
        }} />
      <span className="kbd" style={{ position: 'absolute', right: 10, top: 10 }}>Ctrl K</span>
      <AnimatePresence>
        {open && items.length > 0 && (
          <motion.div className="search-pop" initial={{ opacity: 0, y: -6 }} animate={{ opacity: 1, y: 0 }} exit={{ opacity: 0, y: -6 }}>
            {items.map((it, i) => (
              <div key={`${it.kind}-${it.word}`} className={`search-item ${i === cursor ? 'on' : ''}`} onMouseDown={() => go(it)}>
                <span><span className="mono">{it.word}</span></span>
                <span className="faint" style={{ fontSize: 11 }}>{it.kind} · {it.ids.length} match{it.ids.length > 1 ? 'es' : ''}</span>
              </div>
            ))}
            <div className="faint" style={{ fontSize: 10, padding: '6px 12px', borderTop: '1px solid var(--line)' }}>Trie prefix search · C++ engine</div>
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  );
}

function Clock() {
  const [now, setNow] = useState(new Date());
  useEffect(() => { const t = setInterval(() => setNow(new Date()), 1000); return () => clearInterval(t); }, []);
  return <div className="clock" title="Local time">{now.toLocaleTimeString(undefined, { hour12: false })}<span className="faint"> IST</span></div>;
}

export default function Topbar({ onToggle }) {
  const { session, logout } = useAuth();
  const status = useLiveStatus();
  const initials = (session?.displayName || '?').split(' ').map((w) => w[0]).join('').slice(0, 2).toUpperCase();
  return (
    <header className="topbar">
      <button className="btn ghost icon" onClick={onToggle} aria-label="Toggle sidebar"><Menu size={18} /></button>
      <GlobalSearch />
      <div className="topbar-spacer" />
      <span className="sim-banner">DEMO · SIMULATED DATA</span>
      <span className={`badge ${status === 'live' ? 'b-green pulse' : status === 'connecting' ? 'b-amber' : 'b-grey'}`} title="WebSocket live feed">
        {status === 'live' ? <Wifi size={12} /> : <WifiOff size={12} />}{status}
      </span>
      <Clock />
      <div className="user-chip">
        <div className="avatar">{initials}</div>
        <div style={{ lineHeight: 1.1 }}>
          <div style={{ fontWeight: 600, fontSize: 13 }}>{session?.displayName}</div>
          <div className="faint" style={{ fontSize: 10, letterSpacing: '.12em' }}>{(session?.role || '').replace(/_/g, ' ')}</div>
        </div>
        <button className="btn ghost icon sm" onClick={logout} title="Sign out"><LogOut size={15} /></button>
      </div>
    </header>
  );
}
