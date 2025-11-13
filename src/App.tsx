import { Bolt } from 'lucide-react';

function App() {
  return (
    <div className="min-h-screen bg-gray-900 text-white flex flex-col items-center justify-center p-4">
      <div className="text-center">
        <div className="inline-block p-4 bg-gray-800 rounded-full mb-4">
          <Bolt size={48} className="text-yellow-400" />
        </div>
        <h1 className="text-5xl font-bold mb-2">Bolt</h1>
        <p className="text-xl text-gray-400 mb-8">A beautiful and fast web application.</p>
        <a
          href="#"
          className="bg-yellow-400 text-gray-900 font-bold py-3 px-6 rounded-full hover:bg-yellow-500 transition-colors"
        >
          Get Started
        </a>
      </div>
    </div>
  )
}

export default App
