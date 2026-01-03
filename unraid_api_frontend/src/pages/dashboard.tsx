import React from 'react';

export function Dashboard() {
  return (
    <div>
       <h2 className="text-3xl font-bold tracking-tight">Dashboard</h2>
       <p className="text-muted-foreground">Overview of your mesh networks.</p>
       <div className="grid gap-4 md:grid-cols-2 lg:grid-cols-4 mt-4">
          <div className="rounded-xl border bg-card text-card-foreground shadow-sm p-6">
             <div className="text-sm font-medium">Total Networks</div>
             <div className="text-2xl font-bold">3</div>
          </div>
          <div className="rounded-xl border bg-card text-card-foreground shadow-sm p-6">
             <div className="text-sm font-medium">Active Devices</div>
             <div className="text-2xl font-bold">12</div>
          </div>
       </div>
    </div>
  )
}
