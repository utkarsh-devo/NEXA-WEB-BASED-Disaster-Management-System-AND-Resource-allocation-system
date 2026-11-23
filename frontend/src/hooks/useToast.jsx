import { createContext, useCallback, useContext, useMemo, useState } from 'react';
import { AnimatePresence, motion } from 'framer-motion';

const ToastContext = createContext(null);
let nextId = 1;

export function ToastProvider({ children }) {
  const [toasts, setToasts] = useState([]);
  const dismiss = useCallback((id) => setToasts((t) => t.filter((x) => x.id !== id)), []);
  const push = useCallback((kind, title, message, ttl = 4500) => {
    const id = nextId++;
    setToasts((t) => [...t.slice(-4), { id, kind, title, message }]);
    setTimeout(() => dismiss(id), ttl);
  }, [dismiss]);

  const value = useMemo(() => ({
    success: (title, message) => push('success', title, message),
    error: (title, message) => push('error', title, message, 7000),
    info: (title, message) => push('info', title, message),
    live: (title, message) => push('live', title, message, 3500),
    fromError: (e, title = 'Operation failed') => push('error', title, e && e.message ? e.message : String(e), 7000),
  }), [push]);

  return (
    <ToastContext.Provider value={value}>
      {children}
      <div className="toasts">
        <AnimatePresence>
          {toasts.map((t) => (
            <motion.div key={t.id} className={`toast ${t.kind}`} layout
              initial={{ opacity: 0, x: 60, scale: 0.96 }} animate={{ opacity: 1, x: 0, scale: 1 }} exit={{ opacity: 0, x: 60 }}
              transition={{ type: 'spring', stiffness: 380, damping: 30 }} onClick={() => dismiss(t.id)}>
              <div className="toast-title">{t.title}</div>
              {t.message && <div className="muted">{t.message}</div>}
            </motion.div>
          ))}
        </AnimatePresence>
      </div>
    </ToastContext.Provider>
  );
}

export function useToast() {
  const ctx = useContext(ToastContext);
  if (!ctx) throw new Error('useToast must be used inside <ToastProvider>');
  return ctx;
}
