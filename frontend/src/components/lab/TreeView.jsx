import { useMemo } from 'react';
import { motion } from 'framer-motion';

/**
 * Draws a rooted tree from engine nodes: [{ id, parent, label, color, edge }].
 * Layout: Reingold–Tilford-lite — leaves get consecutive x slots in DFS order
 * (respecting L/R edge labels for binary trees), parents centre over children.
 */
export default function TreeView({ nodes = [], height: forcedHeight, nodeWidth = 46, levelGap = 66, highlight = [] }) {
  const layout = useMemo(() => {
    if (!nodes.length) return null;
    const byId = new Map(nodes.map((n) => [n.id, { ...n, children: [] }]));
    const roots = [];
    nodes.forEach((n) => {
      const me = byId.get(n.id);
      if (n.parent && byId.has(n.parent)) byId.get(n.parent).children.push(me); else roots.push(me);
    });
    // binary trees: keep L before R and leave a gap for a missing child
    byId.forEach((n) => {
      if (n.children.some((c) => c.edge === 'L' || c.edge === 'R')) {
        const L = n.children.find((c) => c.edge === 'L'); const R = n.children.find((c) => c.edge === 'R');
        n.children = [L || { ghost: true, children: [] }, R || { ghost: true, children: [] }];
      }
    });
    let slot = 0; let maxDepth = 0;
    const place = (n, depth) => {
      n.depth = depth; maxDepth = Math.max(maxDepth, depth);
      const real = n.children.filter((c) => !c.ghost || n.children.some((x) => !x.ghost));
      if (!n.children.length || n.children.every((c) => c.ghost)) { n.x = slot++; return; }
      real.forEach((c) => place(c, depth + 1));
      const xs = real.filter((c) => c.x !== undefined).map((c) => c.x);
      n.x = (Math.min(...xs) + Math.max(...xs)) / 2;
    };
    roots.forEach((r) => { place(r, 0); slot += 0.5; });
    const all = [];
    const walk = (n) => { if (!n.ghost) all.push(n); n.children.forEach(walk); };
    roots.forEach(walk);
    const width = Math.max(1, slot) * nodeWidth + nodeWidth;
    return { all, width, height: (maxDepth + 1) * levelGap + 30 };
  }, [nodes, nodeWidth, levelGap]);

  if (!layout) return <div className="empty">Empty structure</div>;
  const X = (n) => n.x * nodeWidth + nodeWidth;
  const Y = (n) => n.depth * levelGap + 30;
  const multiway = nodes.some((n) => n.label.includes('|') || n.label.length > 6);
  return (
    <div style={{ overflow: 'auto', maxHeight: forcedHeight || 460 }}>
      <svg className="svg-tree" width={layout.width} height={layout.height} style={{ minWidth: '100%' }}>
        {layout.all.map((n) => n.children.filter((c) => !c.ghost).map((c) => (
          <motion.path key={`${n.id}-${c.id}`} className="edge" initial={{ pathLength: 0 }} animate={{ pathLength: 1 }} transition={{ duration: 0.5 }}
            d={`M${X(n)},${Y(n) + 12} C${X(n)},${(Y(n) + Y(c)) / 2} ${X(c)},${(Y(n) + Y(c)) / 2} ${X(c)},${Y(c) - 12}`} />
        )))}
        {layout.all.map((n, i) => {
          const w = multiway ? Math.max(34, n.label.length * 7.2 + 14) : 32;
          return (
            <motion.g key={n.id} initial={{ opacity: 0, scale: 0.3 }} animate={{ opacity: 1, scale: 1 }} transition={{ delay: Math.min(i * 0.02, 0.6), type: 'spring', stiffness: 380, damping: 22 }}
              style={{ transformOrigin: `${X(n)}px ${Y(n)}px` }}>
              {multiway
                ? <rect x={X(n) - w / 2} y={Y(n) - 13} width={w} height={26} rx={6} className={`node ${n.color || ''} ${highlight.includes(n.id) ? 'visited' : ''}`} />
                : <circle cx={X(n)} cy={Y(n)} r={15} className={`node ${n.color || ''} ${highlight.includes(n.id) ? 'visited' : ''}`} />}
              <text x={X(n)} y={Y(n) + 4} textAnchor="middle" style={{ fill: n.color === 'red' ? '#ffd0d6' : undefined }}>{n.label}</text>
            </motion.g>
          );
        })}
      </svg>
    </div>
  );
}
