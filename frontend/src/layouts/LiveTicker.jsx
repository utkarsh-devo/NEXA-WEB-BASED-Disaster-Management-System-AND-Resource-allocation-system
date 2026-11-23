import { useEffect, useState } from 'react';
import { Radio } from 'lucide-react';
import { api } from '../services/api';
import { useLiveFeed } from '../hooks/useLiveFeed';
import { useToast } from '../hooks/useToast';

const LABEL = {
  'incident.created': 'NEW INCIDENT', 'incident.updated': 'UPDATE', 'incident.resolved': 'RESOLVED', 'deployment.created': 'DISPATCH',
  'deployment.closed': 'RELEASE', 'request.created': 'REQUEST', 'resource.updated': 'UNIT', 'hospital.updated': 'MEDICAL',
  'simulation.step': 'SIM', 'network.updated': 'NETWORK',
};

/** Scrolling ticker of recent operational activity, refreshed by the live feed. */
export default function LiveTicker() {
  const [items, setItems] = useState([]);
  const toast = useToast();

  useEffect(() => {
    api.activity(14).then((ev) => setItems(ev.map((e) => ({ tag: e.type, text: e.message })))).catch(() => {});
  }, []);

  useLiveFeed((batch) => {
    const fresh = batch.filter((e) => e.type !== 'resource.updated').map((e) => ({ tag: LABEL[e.type] || e.type, text: `${e.title} — ${e.detail}` }));
    if (fresh.length) setItems((cur) => [...fresh.reverse(), ...cur].slice(0, 20));
    const big = batch.find((e) => e.type === 'incident.created' || e.type === 'deployment.created');
    if (big) toast.live(LABEL[big.type], big.title);
  }, null, 300);

  const loop = items.length ? [...items, ...items] : [{ tag: 'STANDBY', text: 'Awaiting operational events…' }];
  return (
    <div className="ticker">
      <div className="ticker-label"><Radio size={12} className="blink" />LIVE</div>
      <div className="ticker-track">
        {loop.map((it, i) => <span key={i} className="ticker-item"><b>{it.tag}</b>{it.text}</span>)}
      </div>
    </div>
  );
}
