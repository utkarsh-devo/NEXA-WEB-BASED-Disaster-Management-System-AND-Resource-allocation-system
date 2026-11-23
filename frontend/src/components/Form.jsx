// Small controlled form primitives with consistent styling.
export function Field({ label, hint, error, children, full = false }) {
  return (
    <div className={`field ${full ? 'full' : ''}`}>
      {label && <label>{label}</label>}
      {children}
      {error ? <span className="err">{error}</span> : hint ? <span className="hint">{hint}</span> : null}
    </div>
  );
}

export function TextInput({ value, onChange, mono, ...rest }) {
  return <input className={`input ${mono ? 'mono' : ''}`} value={value ?? ''} onChange={(e) => onChange(e.target.value)} {...rest} />;
}

export function NumberInput({ value, onChange, step = 1, ...rest }) {
  return <input className="input mono" type="number" step={step} value={value ?? ''} onChange={(e) => onChange(e.target.value === '' ? '' : Number(e.target.value))} {...rest} />;
}

export function TextArea({ value, onChange, ...rest }) {
  return <textarea className="textarea" value={value ?? ''} onChange={(e) => onChange(e.target.value)} {...rest} />;
}

/** options: [{ value, label }] or plain strings */
export function Select({ value, onChange, options, placeholder, ...rest }) {
  return (
    <select className="select" value={value ?? ''} onChange={(e) => onChange(e.target.value)} {...rest}>
      {placeholder !== undefined && <option value="">{placeholder}</option>}
      {options.map((o) => {
        const opt = typeof o === 'string' ? { value: o, label: o } : o;
        return <option key={opt.value} value={opt.value}>{opt.label}</option>;
      })}
    </select>
  );
}

export function Checkbox({ checked, onChange, label }) {
  return <label className="checkbox"><input type="checkbox" checked={Boolean(checked)} onChange={(e) => onChange(e.target.checked)} />{label}</label>;
}

/** Toggleable chip list for capability selection. */
export function ChipPicker({ options, value = [], onChange }) {
  const toggle = (o) => onChange(value.includes(o) ? value.filter((x) => x !== o) : [...value, o]);
  return <div className="chips">{options.map((o) => <span key={o} className={`chip click ${value.includes(o) ? 'on' : ''}`} onClick={() => toggle(o)}>{o}</span>)}</div>;
}

/** Parses "5, 3 9 1" into [5, 3, 9, 1]. */
export const parseNumbers = (text) => (text || '').split(/[\s,;]+/).filter(Boolean).map(Number).filter((n) => Number.isFinite(n));
