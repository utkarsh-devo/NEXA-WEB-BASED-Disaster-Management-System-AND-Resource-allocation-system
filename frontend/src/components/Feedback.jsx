import { AlertTriangle, Inbox } from 'lucide-react';

export function Loader({ label = 'Acquiring data' }) {
  return <div className="loader"><div className="radar" /><span>{label}<span className="blink">_</span></span></div>;
}

export function EmptyState({ icon: Icon = Inbox, title = 'Nothing here yet', children }) {
  return <div className="empty"><Icon size={34} /><div style={{ fontWeight: 600, color: 'var(--text-dim)' }}>{title}</div>{children && <div style={{ marginTop: 6 }}>{children}</div>}</div>;
}

export function ErrorState({ error, onRetry }) {
  return (
    <div className="empty">
      <AlertTriangle size={34} style={{ color: 'var(--red)', opacity: 1 }} />
      <div style={{ fontWeight: 600, color: 'var(--red)' }}>{error?.code === 'NETWORK' ? 'Engine offline' : 'Request failed'}</div>
      <div style={{ marginTop: 6, maxWidth: 480, marginInline: 'auto' }}>{error?.message || String(error)}</div>
      {onRetry && <button className="btn sm" style={{ marginTop: 14 }} onClick={() => onRetry()}>Retry</button>}
    </div>
  );
}

/** Renders loader / error / children depending on a useApi() state. */
export function Async({ state, children, label }) {
  if (state.loading && !state.data) return <Loader label={label} />;
  if (state.error && !state.data) return <ErrorState error={state.error} onRetry={state.reload} />;
  if (!state.data) return null;
  return children(state.data);
}
