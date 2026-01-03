import { useState } from 'preact/hooks'

export function App() {
  const [count, setCount] = useState(0)

  return (
    <>
      <div class="p-4">
        <h1 class="text-2xl font-bold">Home Base Dashboard</h1>
        <div class="card">
          <button class="bg-blue-500 hover:bg-blue-700 text-white font-bold py-2 px-4 rounded" onClick={() => setCount((count) => count + 1)}>
            count is {count}
          </button>
          <p>
            Served from SD Card
          </p>
        </div>
      </div>
    </>
  )
}
