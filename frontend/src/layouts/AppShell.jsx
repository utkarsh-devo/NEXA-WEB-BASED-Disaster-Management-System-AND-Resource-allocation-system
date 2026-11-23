import { useEffect, useState } from 'react';
import { Outlet, useLocation } from 'react-router-dom';
import { AnimatePresence, motion } from 'framer-motion';
import Sidebar from './Sidebar';
import Topbar from './Topbar';
import LiveTicker from './LiveTicker';
import { api } from '../services/api';
import { useLiveFeed } from '../hooks/useLiveFeed';
import { page } from '../components/motion';

export default function AppShell() {
  const [collapsed, setCollapsed] = useState(false);
  const [counts, setCounts] = useState({});
  const location = useLocation();

  const refreshCounts = () => api.dashboard().then((d) => setCounts({ critical: d.incidents.critical, requests: d.requests.pending })).catch(() => {});
  useEffect(() => { refreshCounts(); const t = setInterval(refreshCounts, 45000); return () => clearInterval(t); }, []);
  useLiveFeed(() => refreshCounts(), ['incident.', 'request.', 'deployment.'], 1200);

  return (
    <div className={`shell ${collapsed ? 'collapsed' : ''}`}>
      <div className="scanline" />
      <Sidebar counts={counts} />
      <div className="main">
        <Topbar onToggle={() => setCollapsed((c) => !c)} />
        <LiveTicker />
        <main className="content">
          <AnimatePresence mode="wait">
            <motion.div key={location.pathname} variants={page} initial="initial" animate="animate" exit="exit">
              <Outlet />
            </motion.div>
          </AnimatePresence>
        </main>
      </div>
    </div>
  );
}
