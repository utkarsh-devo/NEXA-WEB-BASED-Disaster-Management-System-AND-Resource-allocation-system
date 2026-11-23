import { AnimatePresence, motion } from 'framer-motion';
import { EmptyState } from './Feedback';

/**
 * columns: [{ key, label, render?(row), className?, align? }]
 * Rows animate in/out with `layout` so reordering (e.g. priority changes) slides.
 */
export default function DataTable({ columns, rows, rowKey = (r) => r.id, onRowClick, empty = 'No records', maxHeight, dense = false }) {
  if (!rows || rows.length === 0) return <EmptyState title={empty} />;
  return (
    <div className="table-wrap" style={{ maxHeight }}>
      <table className="data" style={dense ? { fontSize: 12 } : undefined}>
        <thead>
          <tr>{columns.map((c) => <th key={c.key} style={{ textAlign: c.align || 'left' }}>{c.label}</th>)}</tr>
        </thead>
        <tbody>
          <AnimatePresence initial={false}>
            {rows.map((r, i) => (
              <motion.tr key={rowKey(r, i)} layout="position" initial={{ opacity: 0, x: -8 }} animate={{ opacity: 1, x: 0 }} exit={{ opacity: 0 }}
                transition={{ duration: 0.25, delay: Math.min(i, 12) * 0.015 }}
                className={onRowClick ? 'clickable' : ''} onClick={onRowClick ? () => onRowClick(r) : undefined}>
                {columns.map((c) => (
                  <td key={c.key} className={c.className} style={{ textAlign: c.align || 'left' }}>{c.render ? c.render(r, i) : r[c.key]}</td>
                ))}
              </motion.tr>
            ))}
          </AnimatePresence>
        </tbody>
      </table>
    </div>
  );
}
