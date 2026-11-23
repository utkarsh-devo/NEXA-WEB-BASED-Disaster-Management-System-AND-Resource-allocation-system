// Shared framer-motion variants so every page animates consistently.
export const page = {
  initial: { opacity: 0, y: 14, filter: 'blur(4px)' },
  animate: { opacity: 1, y: 0, filter: 'blur(0px)', transition: { duration: 0.45, ease: [0.22, 1, 0.36, 1], staggerChildren: 0.045 } },
  exit: { opacity: 0, y: -8, filter: 'blur(3px)', transition: { duration: 0.2 } },
};

export const stagger = {
  animate: { transition: { staggerChildren: 0.055, delayChildren: 0.05 } },
};

export const rise = {
  initial: { opacity: 0, y: 16 },
  animate: { opacity: 1, y: 0, transition: { duration: 0.45, ease: [0.22, 1, 0.36, 1] } },
};

export const pop = {
  initial: { opacity: 0, scale: 0.6 },
  animate: { opacity: 1, scale: 1, transition: { type: 'spring', stiffness: 420, damping: 24 } },
  exit: { opacity: 0, scale: 0.6 },
};

export const rowIn = {
  initial: { opacity: 0, x: -10 },
  animate: { opacity: 1, x: 0 },
  exit: { opacity: 0, x: 10 },
};
