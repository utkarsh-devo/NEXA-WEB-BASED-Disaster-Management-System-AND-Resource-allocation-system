import { motion } from 'framer-motion';

/** tabs: [{ id, label, icon? }] — animated underline shared via layoutId */
export default function Tabs({ tabs, value, onChange, id = 'tabs' }) {
  return (
    <div className="tabs" role="tablist">
      {tabs.map((t) => (
        <button key={t.id} role="tab" aria-selected={value === t.id} className={`tab ${value === t.id ? 'active' : ''}`} onClick={() => onChange(t.id)}>
          <span className="row" style={{ gap: 7 }}>{t.icon && <t.icon size={14} />}{t.label}</span>
          {value === t.id && <motion.span layoutId={`${id}-line`} className="tab-line" transition={{ type: 'spring', stiffness: 500, damping: 36 }} />}
        </button>
      ))}
    </div>
  );
}
