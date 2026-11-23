import { createContext, useCallback, useContext, useEffect, useMemo, useState } from 'react';
import { api, auth } from '../services/api';
import { liveFeed } from '../services/socket';

const AuthContext = createContext(null);

export function AuthProvider({ children }) {
  const [session, setSession] = useState(null);
  const [checking, setChecking] = useState(Boolean(auth.token));

  const logout = useCallback(async () => {
    try { await api.logout(); } catch { /* token may already be invalid */ }
    auth.set(null);
    liveFeed.stop();
    setSession(null);
  }, []);

  useEffect(() => {
    auth.onUnauthorized(() => { auth.set(null); liveFeed.stop(); setSession(null); });
    if (!auth.token) return;
    api.me()
      .then((me) => { setSession({ ...me, token: auth.token }); liveFeed.start(); })
      .catch(() => auth.set(null))
      .finally(() => setChecking(false));
  }, []);

  const login = useCallback(async (username, password) => {
    const s = await api.login(username, password);
    auth.set(s.token);
    setSession(s);
    liveFeed.start();
    return s;
  }, []);

  const can = useCallback((perm) => Boolean(session && session.permissions && session.permissions.includes(perm)), [session]);

  const value = useMemo(() => ({ session, checking, login, logout, can }), [session, checking, login, logout, can]);
  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>;
}

export function useAuth() {
  const ctx = useContext(AuthContext);
  if (!ctx) throw new Error('useAuth must be used inside <AuthProvider>');
  return ctx;
}
