import { FunctionComponent } from 'preact';
import { MotionEvent } from '../hooks/useAPI';

interface MotionTimelineProps {
  events: MotionEvent[];
  loading: boolean;
}

const MotionTimeline: FunctionComponent<MotionTimelineProps> = ({ events, loading }) => {
  const formatTime = (timestamp: string) => {
    const date = new Date(timestamp);
    return date.toLocaleTimeString();
  };

  const formatDate = (timestamp: string) => {
    const date = new Date(timestamp);
    return date.toLocaleDateString();
  };

  if (loading) {
    return (
      <div className="bg-gray-800 rounded-lg p-6 border border-gray-700">
        <div className="animate-pulse space-y-4">
          {[...Array(3)].map((_, i) => (
            <div key={i} className="h-12 bg-gray-700 rounded"></div>
          ))}
        </div>
      </div>
    );
  }

  if (events.length === 0) {
    return (
      <div className="bg-gray-800 rounded-lg p-6 border border-gray-700 text-center text-gray-400">
        <div className="text-4xl mb-2">🚫</div>
        <p>No motion events yet</p>
      </div>
    );
  }

  // Group events by date
  const groupedEvents = events.reduce((groups, event) => {
    const date = formatDate(event.timestamp);
    if (!groups[date]) {
      groups[date] = [];
    }
    groups[date].push(event);
    return groups;
  }, {} as Record<string, MotionEvent[]>);

  return (
    <div className="space-y-6">
      {Object.entries(groupedEvents).map(([date, dateEvents]) => (
        <div key={date} className="bg-gray-800 rounded-lg border border-gray-700 overflow-hidden">
          <div className="bg-gray-700 px-6 py-3 border-b border-gray-600">
            <h3 className="font-semibold text-white">📅 {date}</h3>
          </div>
          <div className="divide-y divide-gray-700">
            {dateEvents.map((event) => (
              <div key={event.id} className="p-6 hover:bg-gray-750 transition">
                <div className="flex items-start justify-between mb-3">
                  <div className="flex-1">
                    <div className="font-semibold text-white mb-1">{event.device_id}</div>
                    <div className="text-sm text-gray-400">🔴 Motion Detected</div>
                  </div>
                  <div className="text-right text-sm text-gray-400">
                    {formatTime(event.timestamp)}
                  </div>
                </div>
                {event.media_path && (
                  <div className="mt-3 pt-3 border-t border-gray-700">
                    <a 
                      href={event.media_path}
                      target="_blank"
                      rel="noopener noreferrer"
                      className="text-blue-400 hover:text-blue-300 text-sm"
                    >
                      📸 View Media
                    </a>
                  </div>
                )}
              </div>
            ))}
          </div>
        </div>
      ))}
    </div>
  );
};

export default MotionTimeline;
