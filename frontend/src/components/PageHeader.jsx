import { motion } from 'framer-motion';

export default function PageHeader({ kicker, title, subtitle, icon: Icon, children }) {
  return (
    <motion.div className="page-header" initial={{ opacity: 0, y: -8 }} animate={{ opacity: 1, y: 0 }} transition={{ duration: 0.4 }}>
      <div>
        {kicker && <div className="page-kicker">{Icon && <Icon size={13} />}{kicker}</div>}
        <h1 className="page-title">{title}</h1>
        {subtitle && <p className="page-sub">{subtitle}</p>}
      </div>
      {children && <div className="page-actions">{children}</div>}
    </motion.div>
  );
}
