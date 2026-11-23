import { NavLink } from 'react-router-dom';
import { motion } from 'framer-motion';
import { Activity, Ambulance, BarChart3, Binary, Crosshair, Flame, GitBranch, Hospital, LayoutDashboard, ListOrdered, Map as MapIcon, Radio, Settings, ShieldAlert, Trophy, Users, Waves } from 'lucide-react';
import { useAuth } from '../hooks/useAuth';

export const NAV = [
  { group: 'Command', items: [
    { to: '/', label: 'Command Dashboard', icon: LayoutDashboard, end: true },
    { to: '/map', label: 'Spatial Discovery', icon: MapIcon },
  ] },
  { group: 'Operations', items: [
    { to: '/incidents', label: 'Incidents', icon: Flame, badge: 'critical' },
    { to: '/requests', label: 'Emergency Requests', icon: Radio, badge: 'requests' },
    { to: '/priority', label: 'Priority Engine', icon: ListOrdered },
    { to: '/resources', label: 'Resources', icon: Ambulance },
    { to: '/ranking', label: 'Search & Ranking', icon: Trophy },
    { to: '/allocation', label: 'Allocation', icon: Crosshair },
    { to: '/hospitals', label: 'Hospitals', icon: Hospital },
  ] },
  { group: 'Intelligence', items: [
    { to: '/network', label: 'Network & Routes', icon: GitBranch },
    { to: '/analytics', label: 'Analytics & Reports', icon: BarChart3 },
    { to: '/simulation', label: 'Disaster Simulation', icon: Waves },
  ] },
  { group: 'Engineering', items: [
    { to: '/lab', label: 'DSA Laboratory', icon: Binary },
  ] },
  { group: 'Administration', items: [
    { to: '/users', label: 'Users & Roles', icon: Users, perm: 'manage_users' },
    { to: '/settings', label: 'System Settings', icon: Settings },
  ] },
];

export default function Sidebar({ counts = {} }) {
  const { can } = useAuth();
  return (
    <aside className="sidebar">
      <div className="brand">
        <svg className="brand-mark" viewBox="0 0 64 64" aria-hidden>
          <circle cx="32" cy="32" r="24" fill="none" stroke="#ffb627" strokeWidth="3" strokeDasharray="9 6">
            <animateTransform attributeName="transform" type="rotate" from="0 32 32" to="360 32 32" dur="18s" repeatCount="indefinite" />
          </circle>
          <path d="M21 44V20l22 24V20" fill="none" stroke="#ff3d57" strokeWidth="5" strokeLinecap="round" strokeLinejoin="round" />
        </svg>
        <div className="brand-text">
          <div className="brand-name">NEXA</div>
          <div className="brand-sub">Disaster Response Grid</div>
        </div>
      </div>
      <nav className="nav">
        {NAV.map((g) => (
          <div className="nav-group" key={g.group}>
            <div className="nav-group-title">{g.group}</div>
            {g.items.filter((i) => !i.perm || can(i.perm)).map((i) => (
              <NavLink key={i.to} to={i.to} end={i.end} className={({ isActive }) => `nav-link ${isActive ? 'active' : ''}`} title={i.label}>
                {({ isActive }) => (
                  <>
                    {isActive && <motion.span layoutId="nav-indicator" className="nav-indicator" transition={{ type: 'spring', stiffness: 500, damping: 38 }} />}
                    <i.icon size={17} />
                    <span>{i.label}</span>
                    {i.badge && counts[i.badge] > 0 && <span className="nav-badge">{counts[i.badge]}</span>}
                  </>
                )}
              </NavLink>
            ))}
          </div>
        ))}
      </nav>
      <div className="sidebar-foot">
        <ShieldAlert size={14} style={{ color: 'var(--violet)', flex: 'none' }} />
        <span className="sidebar-foot-text">C++ DSA engine · demo data is simulated</span>
        <Activity size={13} style={{ marginLeft: 'auto', color: 'var(--green)' }} className="blink" />
      </div>
    </aside>
  );
}
