/** Key/value table for measured stats or theoretical complexity returned by the engine. */
export default function Complexity({ items = [], title }) {
  if (!items.length) return null;
  return (
    <div>
      {title && <div className="faint" style={{ fontSize: 11, letterSpacing: '.16em', textTransform: 'uppercase', marginBottom: 8 }}>{title}</div>}
      <dl className="kv">
        {items.map((s) => [<dt key={`k-${s.key}`}>{s.key}</dt>, <dd key={`v-${s.key}`}>{s.value}</dd>])}
      </dl>
    </div>
  );
}
