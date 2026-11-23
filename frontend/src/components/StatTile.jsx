import { motion } from 'framer-motion';
import AnimatedNumber from './AnimatedNumber';
import { rise } from './motion';

/** Big animated metric tile. tone: amber | red | cyan | green | violet */
export default function StatTile({ label, value, icon: Icon, tone = 'amber', foot, decimals = 0, suffix = '', alert = false, onClick }) {
  return (
    <motion.div variants={rise} whileHover={{ y: -3 }} transition={{ type: 'spring', stiffness: 300, damping: 22 }}
      className={`panel stat tone-${tone} ${alert ? 'alert critical-glow' : ''}`} onClick={onClick} style={{ cursor: onClick ? 'pointer' : 'default' }}>
      <div className="stat-label">{Icon && <Icon size={14} />}{label}</div>
      <div className="stat-value"><AnimatedNumber value={value} decimals={decimals} suffix={suffix} /></div>
      {foot && <div className="stat-foot">{foot}</div>}
      {Icon && <Icon className="stat-glyph" size={92} />}
    </motion.div>
  );
}
