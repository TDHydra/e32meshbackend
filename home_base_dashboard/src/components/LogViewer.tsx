import { FunctionComponent, useState } from 'preact';
import { DeviceLog } from '../hooks/useAPI';

interface LogViewerProps {
  logs: DeviceLog[];
  loading: boolean;
  onRefresh?: () => void;
}

const LogViewer: FunctionComponent<LogViewerProps> = ({ logs, loading, onRefresh }) => {
  const [filter, setFilter] = useState<'all' | 'error' | 'warning' | 'info'>('all');

  const getLevelColor = (level: string) => {
    switch (level.toLowerCase()) {
      case 'error':
        return 'text-red-400 bg-red-900 bg-opacity-20';
      case 'warning':
        return 'text-yellow-400 bg-yellow-900 bg-opacity-20';
      case 'info':
        return 'text-blue-400 bg-blue-900 bg-opacity-20';
      default:
        return 'text-gray-400 bg-gray-900 bg-opacity-20';
    }
  };

  const getLevelIcon = (level: string) => {
    switch (level.toLowerCase()) {
      case 'error':
        return '❌';
      case 'warning':
        return '⚠️';
      case 'info':
        return 'ℹ️';
      default:
        return '📝';
    }
  };

  const filteredLogs = logs.filter((log) =>
    filter === 'all' ? true : log.level.toLowerCase() === filter.toLowerCase()
  );

  const formatTime = (timestamp: string) => {
    const date = new Date(timestamp);
    return date.toLocaleTimeString();
  };

  if (loading) {
    return (
      <div className="bg-gray-800 rounded-lg p-6 border border-gray-700">
        <div className="animate-pulse space-y-3">
          {[...Array(5)].map((_, i) => (
            <div key={i} className="h-8 bg-gray-700 rounded"></div>
          ))}
        </div>
      </div>
    );
  }

  return (
    <div className="bg-gray-800 rounded-lg border border-gray-700 overflow-hidden">
      <div className="bg-gray-700 px-6 py-4 border-b border-gray-600 flex justify-between items-center">
        <h3 className="font-semibold text-white">📋 Device Logs</h3>
        {onRefresh && (
          <button
            onClick={onRefresh}
            className="text-xs px-3 py-1 bg-gray-600 hover:bg-gray-500 rounded transition"
          >
            🔄 Refresh
          </button>
        )}
      </div>

      <div className="px-6 py-4 border-b border-gray-600 flex gap-2">
        {(['all', 'info', 'warning', 'error'] as const).map((level) => (
          <button
            key={level}
            onClick={() => setFilter(level)}
            className={`px-3 py-1 text-sm rounded transition ${
              filter === level
                ? 'bg-blue-600 text-white'
                : 'bg-gray-700 text-gray-300 hover:bg-gray-600'
            }`}
          >
            {level === 'all' ? 'All' : level.charAt(0).toUpperCase() + level.slice(1)}
          </button>
        ))}
      </div>

      <div className="overflow-y-auto max-h-96">
        {filteredLogs.length === 0 ? (
          <div className="p-6 text-center text-gray-400">
            <div className="text-3xl mb-2">📭</div>
            <p>No logs found</p>
          </div>
        ) : (
          <div className="divide-y divide-gray-700">
            {filteredLogs.map((log) => (
              <div key={log.id} className="px-6 py-3 hover:bg-gray-750 transition">
                <div className="flex items-start gap-3 mb-2">
                  <span className="text-lg">{getLevelIcon(log.level)}</span>
                  <div className="flex-1 min-w-0">
                    <div className="flex items-center gap-2 mb-1">
                      <span className={`text-xs font-mono px-2 py-1 rounded ${getLevelColor(log.level)}`}>
                        {log.level}
                      </span>
                      <span className="text-xs text-gray-500">[{log.category}]</span>
                      <span className="text-xs text-gray-500 ml-auto">{formatTime(log.timestamp)}</span>
                    </div>
                    <div className="text-sm text-gray-300 font-mono break-all">
                      {log.device_id}: {log.message}
                    </div>
                  </div>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
};

export default LogViewer;
