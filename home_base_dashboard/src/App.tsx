import { FunctionComponent } from 'preact';
import { useState } from 'preact/hooks';
import DeviceCard from './components/DeviceCard';
import MotionTimeline from './components/MotionTimeline';
import LogViewer from './components/LogViewer';
import ControlPanel from './components/ControlPanel';
import { useStatus, useDevices, useLogs, useMotionEvents, useWebSocket, type Device } from './hooks/useAPI';

const App: FunctionComponent = () => {
  const { status, loading: statusLoading, refetch: refetchStatus } = useStatus();
  const { devices, refetch: refetchDevices } = useDevices();
  const { logs, refetch: refetchLogs } = useLogs();
  const { events: motionEvents, refetch: refetchMotion } = useMotionEvents();
  const [selectedDevice, setSelectedDevice] = useState<Device | null>(null);
  const [activeTab, setActiveTab] = useState<'overview' | 'timeline' | 'logs'>('overview');

  // WebSocket for real-time updates
  useWebSocket((message) => {
    if (message.type === 'device_status') {
      refetchDevices();
      if (selectedDevice?.device_id === message.data.device_id) {
        setSelectedDevice((prev) => prev ? { ...prev, ...message.data } : null);
      }
    } else if (message.type === 'motion_event') {
      refetchMotion();
    } else if (message.type === 'status_update') {
      refetchStatus();
    }
  });

  if (statusLoading && !status) {
    return (
      <div className="flex items-center justify-center h-screen bg-gray-900">
        <div className="text-white text-center">
          <div className="text-4xl mb-4">⏳</div>
          <p>Loading Home Base Dashboard...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-gray-900 text-white">
      <header className="bg-gray-800 border-b border-gray-700 sticky top-0 z-10">
        <div className="max-w-7xl mx-auto px-6 py-4 flex items-center justify-between">
          <div className="flex items-center gap-3">
            <div className="text-2xl">🏠</div>
            <div>
              <h1 className="text-xl font-bold">Home Base</h1>
              <p className="text-xs text-gray-400">Local Mesh Network</p>
            </div>
          </div>
          <div className="flex items-center gap-4">
            <button
              onClick={refetchStatus}
              className="text-xs px-3 py-1 bg-gray-700 hover:bg-gray-600 rounded transition"
            >
              🔄 Refresh
            </button>
            <div className="text-xs text-gray-400">
              {new Date().toLocaleTimeString()}
            </div>
          </div>
        </div>
      </header>

      <main className="max-w-7xl mx-auto px-6 py-8">
        {status && (
          <div className="grid grid-cols-1 md:grid-cols-4 gap-4 mb-8">
            <div className="bg-gray-800 rounded-lg p-6 border border-gray-700">
              <div className="text-gray-400 text-sm mb-2">📊 Uptime</div>
              <div className="text-2xl font-bold">
                {Math.floor(status.uptime_seconds / 3600)}h {Math.floor((status.uptime_seconds % 3600) / 60)}m
              </div>
            </div>
            <div className="bg-gray-800 rounded-lg p-6 border border-gray-700">
              <div className="text-gray-400 text-sm mb-2">🔗 Devices</div>
              <div className="text-2xl font-bold">
                {status.online_count}/{status.device_count}
              </div>
            </div>
            <div className="bg-gray-800 rounded-lg p-6 border border-gray-700">
              <div className="text-gray-400 text-sm mb-2">📶 Avg RSSI</div>
              <div className="text-2xl font-bold">{status.mesh_rssi} dBm</div>
            </div>
            <div className="bg-gray-800 rounded-lg p-6 border border-gray-700">
              <div className="text-gray-400 text-sm mb-2">💾 Memory</div>
              <div className="text-2xl font-bold">{status.memory_used_mb} MB</div>
            </div>
          </div>
        )}

        <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
          <div className="lg:col-span-2 space-y-6">
            <div>
              <h2 className="text-xl font-bold mb-4">📡 Connected Devices</h2>
              <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
                {devices.length > 0 ? (
                  devices.map((device) => (
                    <DeviceCard
                      key={device.device_id}
                      device={device}
                      selected={selectedDevice?.device_id === device.device_id}
                      onSelect={() => setSelectedDevice(device)}
                    />
                  ))
                ) : (
                  <div className="col-span-2 text-center py-12 text-gray-400">
                    <div className="text-4xl mb-4">🔍</div>
                    <p>No devices connected yet</p>
                  </div>
                )}
              </div>
            </div>

            {selectedDevice && (
              <div>
                <div className="flex gap-2 mb-4 border-b border-gray-700">
                  <button
                    onClick={() => setActiveTab('overview')}
                    className={`px-4 py-2 text-sm font-medium transition ${
                      activeTab === 'overview'
                        ? 'border-b-2 border-blue-500 text-white'
                        : 'text-gray-400 hover:text-white'
                    }`}
                  >
                    ℹ️ Overview
                  </button>
                  <button
                    onClick={() => setActiveTab('timeline')}
                    className={`px-4 py-2 text-sm font-medium transition ${
                      activeTab === 'timeline'
                        ? 'border-b-2 border-blue-500 text-white'
                        : 'text-gray-400 hover:text-white'
                    }`}
                  >
                    📅 Motion Timeline
                  </button>
                  <button
                    onClick={() => setActiveTab('logs')}
                    className={`px-4 py-2 text-sm font-medium transition ${
                      activeTab === 'logs'
                        ? 'border-b-2 border-blue-500 text-white'
                        : 'text-gray-400 hover:text-white'
                    }`}
                  >
                    📋 Logs
                  </button>
                </div>

                {activeTab === 'overview' && <ControlPanel device={selectedDevice} onCommandSent={refetchDevices} />}
                {activeTab === 'timeline' && <MotionTimeline events={motionEvents} loading={false} />}
                {activeTab === 'logs' && <LogViewer logs={logs} loading={false} onRefresh={refetchLogs} />}
              </div>
            )}
          </div>

          <div className="space-y-6">
            <div>
              <h3 className="text-lg font-bold mb-4">🔴 Recent Motion</h3>
              <MotionTimeline events={motionEvents.slice(0, 5)} loading={false} />
            </div>
          </div>
        </div>
      </main>
    </div>
  );
};

export default App;
