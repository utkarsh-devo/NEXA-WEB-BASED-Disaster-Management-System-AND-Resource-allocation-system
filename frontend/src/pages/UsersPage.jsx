import { useState } from 'react';
import { KeyRound, Plus, ShieldCheck, Users } from 'lucide-react';
import { api } from '../services/api';
import { useApi, useAction } from '../hooks/useApi';
import { useToast } from '../hooks/useToast';
import { useAuth } from '../hooks/useAuth';
import PageHeader from '../components/PageHeader';
import Panel from '../components/Panel';
import Modal from '../components/Modal';
import DataTable from '../components/DataTable';
import { Async, EmptyState } from '../components/Feedback';
import { Checkbox, Field, Select, TextInput } from '../components/Form';
import { clock } from '../services/format';

const ROLES = [{ value: 'ADMIN', label: 'Administrator' }, { value: 'INCIDENT_OPERATOR', label: 'Incident Operator' }, { value: 'RESOURCE_COORDINATOR', label: 'Resource Coordinator' }, { value: 'MEDICAL_COORDINATOR', label: 'Medical Coordinator' }, { value: 'VIEWER', label: 'Read-only Viewer' }];
const MATRIX = [
  ['View dashboards & data', [1, 1, 1, 1, 1]], ['Manage incidents', [1, 1, 0, 0, 0]], ['Manage resources', [1, 0, 1, 0, 0]], ['Manage requests', [1, 1, 1, 1, 0]],
  ['Allocate / deploy', [1, 1, 1, 0, 0]], ['Hospitals & beds', [1, 0, 0, 1, 0]], ['Road network', [1, 0, 1, 0, 0]], ['Run simulations', [1, 1, 0, 0, 0]], ['Users & settings', [1, 0, 0, 0, 0]],
];

export default function UsersPage() {
  const toast = useToast();
  const { can, session } = useAuth();
  const [editing, setEditing] = useState(null);
  const [pw, setPw] = useState(null);
  const [busy, run] = useAction();
  const users = useApi(() => api.users(), [], { enabled: can('manage_users') });
  const save = () => run(async () => {
    try {
      if (editing.id) await api.updateUser(editing.id, editing);
      else await api.createUser(editing);
      toast.success('User saved'); setEditing(null); users.reload({ silent: true });
    } catch (e) { toast.fromError(e); }
  });
  if (!can('manage_users')) return <EmptyState icon={ShieldCheck} title="Administrator access required" />;
  const columns = [
    { key: 'u', label: 'User', render: (u) => <div><div className="cell-main">{u.displayName}</div><div className="cell-sub mono">{u.username}</div></div> },
    { key: 'r', label: 'Role', render: (u) => <span className="badge b-amber">{u.role.replace(/_/g, ' ')}</span> },
    { key: 'a', label: 'Status', render: (u) => <span className={`badge ${u.active ? 'b-green' : 'b-grey'}`}>{u.active ? 'active' : 'disabled'}</span> },
    { key: 'l', label: 'Last login', render: (u) => <span className="faint">{clock(u.lastLoginAt)}</span> },
    { key: 'x', label: '', render: (u) => <div className="row"><button className="btn sm" onClick={() => setEditing({ ...u })}>Edit</button><button className="btn sm ghost" onClick={() => setPw({ id: u.id, username: u.username, password: '' })}><KeyRound size={13} /></button></div> },
  ];
  return (
    <>
      <PageHeader kicker="§8.3" icon={Users} title="Users & Role Management"
        subtitle="Passwords are salted and iterated SHA-256 (20 000 rounds); sessions are 256-bit bearer tokens stored in a HashTable with expiry; every endpoint checks the role → permission matrix.">
        <button className="btn primary" onClick={() => setEditing({ username: '', displayName: '', role: 'VIEWER', password: '', active: true })}><Plus size={15} />New user</button>
      </PageHeader>
      <div className="grid g-main">
        <Panel title="Accounts" icon={Users} bodyClass="flush"><Async state={users}>{(rows) => <DataTable columns={columns} rows={rows} />}</Async></Panel>
        <Panel title="Permission matrix" icon={ShieldCheck}>
          <table className="data" style={{ fontSize: 12 }}>
            <thead><tr><th>Permission</th>{['ADM', 'OPS', 'RES', 'MED', 'VIEW'].map((r) => <th key={r} style={{ textAlign: 'center' }}>{r}</th>)}</tr></thead>
            <tbody>{MATRIX.map(([p, v]) => <tr key={p}><td>{p}</td>{v.map((x, i) => <td key={i} style={{ textAlign: 'center', color: x ? 'var(--green)' : 'var(--text-faint)' }}>{x ? '●' : '·'}</td>)}</tr>)}</tbody>
          </table>
          <div className="faint" style={{ fontSize: 11, marginTop: 10 }}>Signed in as {session?.username} ({session?.role}).</div>
        </Panel>
      </div>
      <Modal open={Boolean(editing)} onClose={() => setEditing(null)} title={editing?.id ? `Edit ${editing.username}` : 'New user'} footer={<button className="btn primary" disabled={busy} onClick={save}>Save</button>}>
        {editing && (
          <div className="form-grid">
            <Field label="Username"><TextInput value={editing.username} onChange={(v) => setEditing((e) => ({ ...e, username: v }))} disabled={Boolean(editing.id)} /></Field>
            <Field label="Display name"><TextInput value={editing.displayName} onChange={(v) => setEditing((e) => ({ ...e, displayName: v }))} /></Field>
            <Field label="Role"><Select value={editing.role} onChange={(v) => setEditing((e) => ({ ...e, role: v }))} options={ROLES} /></Field>
            {!editing.id && <Field label="Password" hint="≥ 8 chars, letters and digits"><TextInput type="password" value={editing.password} onChange={(v) => setEditing((e) => ({ ...e, password: v }))} /></Field>}
            {editing.id && <Checkbox checked={editing.active} onChange={(v) => setEditing((e) => ({ ...e, active: v }))} label="Account active" />}
          </div>
        )}
      </Modal>
      <Modal open={Boolean(pw)} onClose={() => setPw(null)} title={`Reset password · ${pw?.username || ''}`} icon={KeyRound}
        footer={<button className="btn primary" disabled={busy || !pw?.password} onClick={() => run(async () => { try { await api.resetPassword(pw.id, pw.password); toast.success('Password reset', 'Existing sessions were revoked'); setPw(null); } catch (e) { toast.fromError(e); } })}>Reset</button>}>
        <Field label="New password"><TextInput type="password" value={pw?.password || ''} onChange={(v) => setPw((p) => ({ ...p, password: v }))} /></Field>
      </Modal>
    </>
  );
}
