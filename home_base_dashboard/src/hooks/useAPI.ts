import { useState, useEffect } from 'preact/hooks';

export interface SystemStatus {
  uptime_seconds: number;
  device_count: number;
  online_count: number;
  mesh_rssi: number;
  memory_used_mb: number;
}

export interface Device {
  device_id: string;
  type: 'motion' | 'camera';
  online: boolean;
  rssi: number;
  last_seen: string;
  motion_state: 'clear' | 'detected' | 'cooldown';
  battery_percent?: number;
}

export interface DeviceLog {
  id: number;
  device_id: string;
  timestamp: string;
  level: string;
  category: string;
  message: string;
}

export interface MotionEvent {
  id: number;
  device_id: string;
  timestamp: string;
  media_path?: string;
}

const API_BASE = '';

export function useStatus() {
  const [status, setStatus] = useState<SystemStatus | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const fetchStatus = async () => {
    try {
      const response = await fetch(`${API_BASE}/api/v1/status`);
      if (!response.ok) throw new Error('Failed to fetch status');
      const data = await response.json();
      setStatus(data);
      setError(null);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchStatus();
    const interval = setInterval(fetchStatus, 10000);
    return () => clearInterval(interval);
  }, []);

  return { status, loading, error, refetch: fetchStatus };
}

export function useDevices() {
  const [devices, setDevices] = useState<Device[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const fetchDevices = async () => {
    try {
      const response = await fetch(`${API_BASE}/api/v1/devices`);
      if (!response.ok) throw new Error('Failed to fetch devices');
      const data = await response.json();
      setDevices(data || []);
      setError(null);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchDevices();
    const interval = setInterval(fetchDevices, 5000);
    return () => clearInterval(interval);
  }, []);

  return { devices, loading, error, refetch: fetchDevices };
}

export function useLogs(deviceId?: string, limit: number = 50) {
  const [logs, setLogs] = useState<DeviceLog[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const fetchLogs = async () => {
    try {
      let url = `${API_BASE}/api/logs?limit=${limit}`;
      if (deviceId) url += `&device_id=${deviceId}`;
      
      const response = await fetch(url);
      if (!response.ok) throw new Error('Failed to fetch logs');
      const data = await response.json();
      setLogs(data || []);
      setError(null);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchLogs();
    const interval = setInterval(fetchLogs, 30000);
    return () => clearInterval(interval);
  }, [deviceId, limit]);

  return { logs, loading, error, refetch: fetchLogs };
}

export function useMotionEvents(limit: number = 100) {
  const [events, setEvents] = useState<MotionEvent[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const fetchEvents = async () => {
    try {
      const response = await fetch(`${API_BASE}/api/motion?limit=${limit}`);
      if (!response.ok) throw new Error('Failed to fetch motion events');
      const data = await response.json();
      setEvents(data || []);
      setError(null);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchEvents();
    const interval = setInterval(fetchEvents, 60000);
    return () => clearInterval(interval);
  }, [limit]);

  return { events, loading, error, refetch: fetchEvents };
}

export function useWebSocket(onMessage: (data: any) => void) {
  const [connected, setConnected] = useState(false);

  useEffect(() => {
    const protocol = typeof window !== 'undefined' && window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${typeof window !== 'undefined' ? window.location.host : 'localhost'}/ws`;
    
    const ws = new WebSocket(wsUrl);

    ws.onopen = () => {
      setConnected(true);
      console.log('WebSocket connected');
    };

    ws.onclose = () => {
      setConnected(false);
      console.log('WebSocket disconnected');
      // Attempt reconnection in 5 seconds
      setTimeout(() => {
        // This will trigger useEffect again
      }, 5000);
    };

    ws.onerror = (error) => {
      console.error('WebSocket error:', error);
    };

    ws.onmessage = (event) => {
      try {
        const message = JSON.parse(event.data);
        onMessage(message);
      } catch (err) {
        console.error('Failed to parse WebSocket message:', err);
      }
    };

    return () => {
      if (ws.readyState === WebSocket.OPEN) {
        ws.close();
      }
    };
  }, [onMessage]);

  return { connected };
}

export async function sendCommand(deviceId: string, command: string, payload: any) {
  try {
    const response = await fetch(`${API_BASE}/api/v1/command`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ device_id: deviceId, command, payload })
    });
    if (!response.ok) throw new Error('Command failed');
    return await response.json();
  } catch (err) {
    throw err instanceof Error ? err : new Error('Unknown error');
  }
}
