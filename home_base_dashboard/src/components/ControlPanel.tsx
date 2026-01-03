import { FunctionComponent } from 'preact';
import { useState } from 'preact/hooks';
import { Device, sendCommand } from '../hooks/useAPI';

interface ControlPanelProps {
  device: Device | null;
  onCommandSent?: () => void;
}

const ControlPanel: FunctionComponent<ControlPanelProps> = ({ device, onCommandSent }) => {
  const [loading, setLoading] = useState(false);
  const [message, setMessage] = useState<{ type: 'success' | 'error'; text: string } | null>(null);

  if (!device) {
    return (
      <div className="bg-gray-800 rounded-lg p-6 border border-gray-700 text-center text-gray-400">
        <div className="text-3xl mb-2">🎮</div>
        <p>Select a device to show controls</p>
      </div>
    );
  }

  const handleCommand = async (command: string, payload: any = {}) => {
    setLoading(true);
    setMessage(null);
    try {
      await sendCommand(device.device_id, command, payload);
      setMessage({ type: 'success', text: `Command sent: ${command}` });
      onCommandSent?.();
    } catch (err) {
      setMessage({
        type: 'error',
        text: `Failed to send command: ${err instanceof Error ? err.message : 'Unknown error'}`,
      });
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="bg-gray-800 rounded-lg border border-gray-700 overflow-hidden">
      <div className="bg-gray-700 px-6 py-4 border-b border-gray-600">
        <h3 className="font-semibold text-white">🎮 Device Controls</h3>
        <p className="text-xs text-gray-400 mt-1">{device.device_id}</p>
      </div>

      <div className="p-6 space-y-4">
        {message && (
          <div
            className={`p-3 rounded text-sm ${
              message.type === 'success'
                ? 'bg-green-900 bg-opacity-30 text-green-300 border border-green-700'
                : 'bg-red-900 bg-opacity-30 text-red-300 border border-red-700'
            }`}
          >
            {message.text}
          </div>
        )}

        {device.type === 'motion' && (
          <div className="space-y-3">
            <h4 className="font-semibold text-sm text-gray-300">Motion Sensor</h4>
            <button
              onClick={() => handleCommand('led_color', { color: '00FF00' })}
              disabled={loading || !device.online}
              className="w-full px-4 py-2 bg-green-600 hover:bg-green-500 disabled:bg-gray-600 disabled:cursor-not-allowed text-white rounded transition"
            >
              🟢 Set LED Green (Clear)
            </button>
            <button
              onClick={() => handleCommand('led_color', { color: 'FF0000' })}
              disabled={loading || !device.online}
              className="w-full px-4 py-2 bg-red-600 hover:bg-red-500 disabled:bg-gray-600 disabled:cursor-not-allowed text-white rounded transition"
            >
              🔴 Set LED Red (Alert)
            </button>
          </div>
        )}

        {device.type === 'camera' && (
          <div className="space-y-3">
            <h4 className="font-semibold text-sm text-gray-300">Camera</h4>
            <button
              onClick={() => handleCommand('capture', { mode: 'single' })}
              disabled={loading || !device.online}
              className="w-full px-4 py-2 bg-blue-600 hover:bg-blue-500 disabled:bg-gray-600 disabled:cursor-not-allowed text-white rounded transition"
            >
              📸 Capture Image
            </button>
            <button
              onClick={() => handleCommand('capture', { mode: 'burst', frames: 5 })}
              disabled={loading || !device.online}
              className="w-full px-4 py-2 bg-blue-600 hover:bg-blue-500 disabled:bg-gray-600 disabled:cursor-not-allowed text-white rounded transition"
            >
              🎥 Capture Burst (5 frames)
            </button>
          </div>
        )}

        <div className="border-t border-gray-700 pt-4">
          <h4 className="font-semibold text-sm text-gray-300 mb-3">General</h4>
          <div className="space-y-2">
            <button
              onClick={() => handleCommand('reboot')}
              disabled={loading || !device.online}
              className="w-full px-4 py-2 bg-gray-600 hover:bg-gray-500 disabled:bg-gray-700 disabled:cursor-not-allowed text-white rounded transition"
            >
              🔄 Reboot Device
            </button>
            <button
              onClick={() => handleCommand('status')}
              disabled={loading}
              className="w-full px-4 py-2 bg-gray-600 hover:bg-gray-500 disabled:bg-gray-700 disabled:cursor-not-allowed text-white rounded transition"
            >
              📊 Request Status
            </button>
          </div>
        </div>

        {!device.online && (
          <div className="p-3 bg-orange-900 bg-opacity-30 border border-orange-700 rounded text-orange-300 text-xs">
            ⚠️ Device is offline. Commands will be queued when it reconnects.
          </div>
        )}
      </div>
    </div>
  );
};

export default ControlPanel;
