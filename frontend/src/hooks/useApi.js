import { useCallback, useEffect, useRef, useState } from 'react';

/**
 * Loads data from a NEXA endpoint.
 *   const { data, loading, error, reload, setData } = useApi(() => api.dashboard(), [deps])
 * `reload({ silent: true })` refreshes without flashing the loader (used by live updates).
 */
export function useApi(fetcher, deps = [], { enabled = true } = {}) {
  const [data, setData] = useState(null);
  const [loading, setLoading] = useState(enabled);
  const [error, setError] = useState(null);
  const seq = useRef(0);
  const fetcherRef = useRef(fetcher);
  fetcherRef.current = fetcher;

  const reload = useCallback(async ({ silent = false } = {}) => {
    const mine = ++seq.current;
    if (!silent) setLoading(true);
    try {
      const result = await fetcherRef.current();
      if (mine === seq.current) { setData(result); setError(null); }
      return result;
    } catch (e) {
      if (mine === seq.current) setError(e);
      return null;
    } finally {
      if (mine === seq.current) setLoading(false);
    }
  }, []);

  useEffect(() => {
    if (enabled) reload();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [enabled, ...deps]);

  return { data, loading, error, reload, setData };
}

/** Runs a mutation with busy state and error capture. */
export function useAction() {
  const [busy, setBusy] = useState(false);
  const run = useCallback(async (fn) => {
    setBusy(true);
    try { return await fn(); } finally { setBusy(false); }
  }, []);
  return [busy, run];
}
