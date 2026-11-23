// Live operational feed (WebSocket /ws/live) with automatic reconnection.
import { wsUrl } from './api';

const listeners = new Set();
let ws = null;
let retry = 0;
let timer = null;
let wanted = false;
let status = 'offline';
const statusListeners = new Set();

function setStatus(s) { status = s; statusListeners.forEach((fn) => fn(s)); }

function connect() {
  if (!wanted) return;
  try { ws = new WebSocket(wsUrl()); } catch { schedule(); return; }
  setStatus('connecting');
  ws.onopen = () => { retry = 0; setStatus('live'); };
  ws.onmessage = (msg) => {
    let event;
    try { event = JSON.parse(msg.data); } catch { return; }
    if (event.type === 'hello' || event.type === 'pong') return;
    listeners.forEach((fn) => fn(event));
  };
  ws.onclose = () => { setStatus('offline'); schedule(); };
  ws.onerror = () => { try { ws.close(); } catch { /* ignore */ } };
}

function schedule() {
  if (!wanted) return;
  clearTimeout(timer);
  const delay = Math.min(15000, 800 * 2 ** retry++);
  timer = setTimeout(connect, delay);
}

export const liveFeed = {
  start() { if (wanted) return; wanted = true; connect(); },
  stop() { wanted = false; clearTimeout(timer); if (ws) ws.close(); ws = null; setStatus('offline'); },
  subscribe(fn) { listeners.add(fn); return () => listeners.delete(fn); },
  onStatus(fn) { statusListeners.add(fn); fn(status); return () => statusListeners.delete(fn); },
};
