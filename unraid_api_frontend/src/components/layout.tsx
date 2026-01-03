import React from 'react';
import { Outlet, Link } from 'react-router-dom';
import { cn } from '@/lib/utils';

export function Layout() {
  return (
    <div className="min-h-screen bg-background font-sans antialiased">
      <div className="border-b">
        <div className="flex h-16 items-center px-4">
          <div className="mr-8 hidden md:flex">
            <Link to="/" className="mr-6 flex items-center space-x-2 font-bold">Unraid Central</Link>
            <nav className="flex items-center space-x-6 text-sm font-medium">
              <Link to="/" className="transition-colors hover:text-foreground/80 text-foreground">Dashboard</Link>
              <Link to="/networks" className="transition-colors hover:text-foreground/80 text-foreground/60">Networks</Link>
              <Link to="/devices" className="transition-colors hover:text-foreground/80 text-foreground/60">Devices</Link>
              <Link to="/logs" className="transition-colors hover:text-foreground/80 text-foreground/60">Logs</Link>
            </nav>
          </div>
          <div className="ml-auto flex items-center space-x-4">
             {/* User Nav will go here */}
             <div className="h-8 w-8 rounded-full bg-slate-200"></div>
          </div>
        </div>
      </div>
      <main className="flex-1 space-y-4 p-8 pt-6">
        <Outlet />
      </main>
    </div>
  );
}
