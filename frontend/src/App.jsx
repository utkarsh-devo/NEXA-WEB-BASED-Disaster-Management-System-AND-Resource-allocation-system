import { Navigate, Route, Routes } from 'react-router-dom';
import { useAuth } from './hooks/useAuth';
import AppShell from './layouts/AppShell';
import { Loader } from './components/Feedback';
import LoginPage from './pages/LoginPage';
import DashboardPage from './pages/DashboardPage';
import IncidentsPage from './pages/IncidentsPage';
import IncidentDetailPage from './pages/IncidentDetailPage';
import ResourcesPage from './pages/ResourcesPage';
import ResourceDetailPage from './pages/ResourceDetailPage';
import RequestsPage from './pages/RequestsPage';
import PriorityPage from './pages/PriorityPage';
import RankingPage from './pages/RankingPage';
import SpatialPage from './pages/SpatialPage';
import NetworkPage from './pages/NetworkPage';
import AllocationPage from './pages/AllocationPage';
import HospitalsPage from './pages/HospitalsPage';
import AnalyticsPage from './pages/AnalyticsPage';
import SimulationPage from './pages/SimulationPage';
import DsaLabPage from './pages/DsaLabPage';
import UsersPage from './pages/UsersPage';
import SettingsPage from './pages/SettingsPage';

function Protected({ children }) {
  const { session, checking } = useAuth();
  if (checking) return <div style={{ height: '100vh', display: 'grid', placeItems: 'center' }}><Loader label="Restoring session" /></div>;
  if (!session) return <Navigate to="/login" replace />;
  return children;
}

export default function App() {
  const { session } = useAuth();
  return (
    <Routes>
      <Route path="/login" element={session ? <Navigate to="/" replace /> : <LoginPage />} />
      <Route element={<Protected><AppShell /></Protected>}>
        <Route index element={<DashboardPage />} />
        <Route path="map" element={<SpatialPage />} />
        <Route path="incidents" element={<IncidentsPage />} />
        <Route path="incidents/:id" element={<IncidentDetailPage />} />
        <Route path="resources" element={<ResourcesPage />} />
        <Route path="resources/:id" element={<ResourceDetailPage />} />
        <Route path="requests" element={<RequestsPage />} />
        <Route path="priority" element={<PriorityPage />} />
        <Route path="ranking" element={<RankingPage />} />
        <Route path="allocation" element={<AllocationPage />} />
        <Route path="hospitals" element={<HospitalsPage />} />
        <Route path="network" element={<NetworkPage />} />
        <Route path="analytics" element={<AnalyticsPage />} />
        <Route path="simulation" element={<SimulationPage />} />
        <Route path="lab" element={<DsaLabPage />} />
        <Route path="users" element={<UsersPage />} />
        <Route path="settings" element={<SettingsPage />} />
      </Route>
      <Route path="*" element={<Navigate to="/" replace />} />
    </Routes>
  );
}
