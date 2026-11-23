import { motion } from 'framer-motion';
import { rise } from './motion';

/** HUD-framed glass panel. tone: '' | 'alert' | 'cyan' | 'violet' */
export default function Panel({ title, icon: Icon, hint, actions, tone = '', bodyClass = '', className = '', children, noAnim = false }) {
  const Wrapper = noAnim ? 'section' : motion.section;
  const animProps = noAnim ? {} : { variants: rise };
  return (
    <Wrapper className={`panel ${tone} ${className}`} {...animProps}>
      {(title || actions) && (
        <header className="panel-head">
          {title && <h3 className="panel-title">{Icon && <Icon size={15} />}{title}</h3>}
          {hint && <span className="panel-hint">{hint}</span>}
          {actions && <div className="row" style={{ marginLeft: hint ? 10 : 'auto' }}>{actions}</div>}
        </header>
      )}
      <div className={`panel-body ${bodyClass}`}>{children}</div>
    </Wrapper>
  );
}
