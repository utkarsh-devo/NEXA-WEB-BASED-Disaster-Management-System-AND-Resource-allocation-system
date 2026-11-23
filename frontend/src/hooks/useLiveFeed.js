import { useEffect, useRef, useState } from 'react';
import { liveFeed } from '../services/socket';

/**
 * Subscribe to live WebSocket events. `types` filters by prefix
 * (e.g. ['incident.', 'deployment.']). The handler is debounced so a burst of
 * events (simulation step) triggers one refresh.
 */
export function useLiveFeed(handler, types = null, debounceMs = 400) {
  const ref = useRef(handler);
  ref.current = handler;
  const key = types ? types.join('|') : '*';
  useEffect(() => {
    let timer = null;
    let batch = [];
    const off = liveFeed.subscribe((event) => {
      if (types && !types.some((t) => event.type.startsWith(t))) return;
      batch.push(event);
      clearTimeout(timer);
      timer = setTimeout(() => { const b = batch; batch = []; ref.current(b); }, debounceMs);
    });
    return () => { off(); clearTimeout(timer); };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [key, debounceMs]);
}

export function useLiveStatus() {
  const [status, setStatus] = useState('offline');
  useEffect(() => liveFeed.onStatus(setStatus), []);
  return status;
}
