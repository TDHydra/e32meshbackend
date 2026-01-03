import { FunctionComponent } from 'preact';
import { Device } from '../hooks/useAPI';

interface DeviceCardProps {
  device: Device;
  onSelect?: (deviceId: string) => void;
  selected?: boolean;
}

const DeviceCard: FunctionComponent<DeviceCardProps> = ({ device, onSelect, selected = false }) => {
  const getStatusColor = (online: boolean, motionState?: string) => {
    if (!online) return 'bg-red-600';
    if (motionState === 'detected') return 'bg-red-500';
    if (motionState === 'cooldown') return 'bg-yellow-500';
    return 'bg-green-500';
  };

  const getSignalBars = (rssi: number) => {
    const bars = Math.max(1, Math.min(4, Math.ceil((rssi + 100) / 25)));
    return '▂'.repeat(bars) + '▁'.repeat(4 - bars);
  };

  const getLastSeenText = (lastSeen: string) => {
    const date = new Date(lastSeen);
    const now = new Date();
    const seconds = Math.floor((now.getTime() - date.getTime()) / 1000);
    
    if (seconds < 60) return 'now';
    if (seconds < 3600) return `${Math.floor(seconds / 60)}m ago`;
    if (seconds < 86400) return `${Math.floor(seconds / 3600)}h ago`;
    return date.toLocaleDateString();
  };

  return (
    <div
      className={`bg-gray-800 rounded-lg p-6 border-2 transition cursor-pointer
        ${selected ? 'border-blue-500 bg-gray-700' : 'border-gray-700 hover:border-blue-400'}
      `}
      onClick={() => onSelect?.(device.device_id)}
    >
      <div className="flex items-start justify-between mb-4">
        <div className="flex items-center gap-3 flex-1">
          <div className={`w-4 h-4 rounded-full animate-pulse ${getStatusColor(device.online, device.motion_state)}`}></div>
          <div className="flex-1 min-w-0">
            <div className="font-semibold text-white truncate">{device.device_id}</div>
            <div className="text-xs text-gray-400">
              {device.type === 'motion' ? '📡 Motion Sensor' : '📷 Camera'}
            </div>
          </div>
        </div>
      </div>

      <div className="space-y-2 text-sm">
        <div className="flex justify-between text-gray-300">
          <span>Status:</span>
          <span className={device.online ? 'text-green-400' : 'text-red-400'}>
            {device.online ? 'Online' : 'Offline'}
          </span>
        </div>

        <div className="flex justify-between text-gray-300">
          <span>Signal:</span>
          <span className="font-mono text-xs">{device.rssi} dBm {getSignalBars(device.rssi)}</span>
        </div>

        {device.motion_state && (
          <div className="flex justify-between text-gray-300">
            <span>Motion:</span>
            <span className={
              device.motion_state === 'detected' ? 'text-red-400' :
              device.motion_state === 'cooldown' ? 'text-yellow-400' :
              'text-green-400'
            }>
              {device.motion_state === 'detected' ? '🔴 Detected' :
               device.motion_state === 'cooldown' ? '🟡 Cooldown' :
               '🟢 Clear'}
            </span>
          </div>
        )}

        {device.battery_percent !== undefined && (
          <div className="flex justify-between text-gray-300">
            <span>Battery:</span>
            <span>{device.battery_percent}%</span>
          </div>
        )}

        <div className="flex justify-between text-gray-300">
          <span>Last Seen:</span>
          <span className="text-xs">{getLastSeenText(device.last_seen)}</span>
        </div>
      </div>
    </div>
  );
};

export default DeviceCard;
