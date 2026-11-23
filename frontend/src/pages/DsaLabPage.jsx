import { useState } from 'react';
import { AnimatePresence, motion } from 'framer-motion';
import { Binary, BookOpen, Boxes, Gauge, GitFork, Grid3x3, Hash, Layers, ListTree, MapPinned, Puzzle, Search, SortAsc, TextSearch, Timer } from 'lucide-react';
import { api } from '../services/api';
import { useApi } from '../hooks/useApi';
import PageHeader from '../components/PageHeader';
import Panel from '../components/Panel';
import Tabs from '../components/Tabs';
import { Async } from '../components/Feedback';
import { LinearLab, SparseLab } from '../components/lab/LinearLab';
import { SearchLab, SortLab } from '../components/lab/SortSearchLab';
import { HeapLab, TreeLab } from '../components/lab/TreeLab';
import HashLab from '../components/lab/HashLab';
import GraphLab from '../components/lab/GraphLab';
import SpatialLab from '../components/lab/SpatialLab';
import { RangeLab, TextLab } from '../components/lab/TextRangeLab';
import BacktrackLab from '../components/lab/BacktrackLab';
import BenchmarkLab from '../components/lab/BenchmarkLab';

const TABS = [
  { id: 'catalog', label: 'Catalog', icon: BookOpen },
  { id: 'linear', label: 'Linear', icon: Layers },
  { id: 'sparse', label: 'Sparse / Multi-list', icon: Grid3x3 },
  { id: 'sort', label: 'Sorting', icon: SortAsc },
  { id: 'search', label: 'Searching', icon: Search },
  { id: 'trees', label: 'Trees', icon: ListTree },
  { id: 'heap', label: 'Heap / PQ', icon: Boxes },
  { id: 'hash', label: 'Hashing', icon: Hash },
  { id: 'graph', label: 'Graphs', icon: GitFork },
  { id: 'spatial', label: 'Spatial', icon: MapPinned },
  { id: 'text', label: 'Text', icon: TextSearch },
  { id: 'range', label: 'Interval / Segment', icon: Timer },
  { id: 'backtracking', label: 'Backtracking', icon: Puzzle },
  { id: 'bench', label: 'Benchmarks', icon: Gauge },
];

function Catalog({ onOpen }) {
  const cat = useApi(() => api.labCatalog(), []);
  const groupTab = { Linear: 'linear', 'Search & Sort': 'sort', Trees: 'trees', Priority: 'heap', Graphs: 'graph', Hashing: 'hash', Spatial: 'spatial', 'Range & Temporal': 'range', Text: 'text', Backtracking: 'backtracking' };
  return (
    <Async state={cat}>
      {(items) => (
        <div className="grid" style={{ gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))' }}>
          {items.map((c, i) => (
            <motion.div key={c.name} className="panel" style={{ padding: 16, cursor: 'pointer' }} initial={{ opacity: 0, y: 12 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: i * 0.025 }}
              whileHover={{ y: -4, borderColor: 'rgba(255,182,39,.5)' }} onClick={() => onOpen(groupTab[c.group] || 'catalog')}>
              <div className="faint" style={{ fontSize: 10, letterSpacing: '.18em', textTransform: 'uppercase' }}>{c.group}</div>
              <div style={{ fontWeight: 700, fontSize: 16, margin: '4px 0 6px' }}>{c.name}</div>
              <div className="muted" style={{ fontSize: 12, minHeight: 32 }}>{c.application}</div>
              <div className="chips" style={{ marginTop: 10 }}>{c.complexity.map((x) => <span key={x.key} className="chip">{x.key}: {x.value}</span>)}</div>
              <div className="mono faint" style={{ fontSize: 10, marginTop: 10 }}>backend/include/…/{c.file}</div>
            </motion.div>
          ))}
        </div>
      )}
    </Async>
  );
}

export default function DsaLabPage() {
  const [tab, setTab] = useState('catalog');
  const view = {
    catalog: <Catalog onOpen={setTab} />, linear: <LinearLab />, sparse: <SparseLab />, sort: <SortLab />, search: <SearchLab />, trees: <TreeLab />, heap: <HeapLab />,
    hash: <HashLab />, graph: <GraphLab />, spatial: <SpatialLab />, text: <TextLab />, range: <RangeLab />, backtracking: <BacktrackLab />, bench: <BenchmarkLab />,
  }[tab];
  return (
    <>
      <PageHeader kicker="Module 12" icon={Binary} title="DSA Laboratory"
        subtitle="Every structure here is the same hand-written C++ implementation used by the operational modules. The browser only draws the engine's snapshots, animation frames and measured operation counts." />
      <Panel bodyClass="">
        <Tabs tabs={TABS} value={tab} onChange={setTab} id="lab" />
        <AnimatePresence mode="wait">
          <motion.div key={tab} initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }} exit={{ opacity: 0, y: -6 }} transition={{ duration: 0.25 }}>{view}</motion.div>
        </AnimatePresence>
      </Panel>
    </>
  );
}
