import { useState } from 'react';

/**
 * ADF4350 Controller — ESP32-C3
 * PlatformIO Project
 * 
 * Основные файлы проекта:
 *   platformio.ini          — конфигурация
 *   include/adf4350.h       — драйвер (пины, параметры)
 *   src/adf4350.cpp         — реализация SPI
 *   src/main.cpp            — основной код
 */

function App() {
  const [copied, setCopied] = useState('');

  const copyFile = (name: string, content: string) => {
    navigator.clipboard.writeText(content);
    setCopied(name);
    setTimeout(() => setCopied(''), 2000);
  };

  return (
    <div className="min-h-screen bg-gray-900 text-gray-100 p-6">
      <div className="max-w-4xl mx-auto">
        <header className="mb-8">
          <h1 className="text-3xl font-bold text-blue-400">📡 ADF4350 Controller — ESP32-C3</h1>
          <p className="text-gray-400 mt-2">PlatformIO / Arduino проект</p>
        </header>

        {/* Распиновка */}
        <section className="bg-gray-800 rounded-xl p-6 mb-6 border border-gray-700">
          <h2 className="text-xl font-semibold text-orange-400 mb-4">🔌 Распиновка</h2>
          
          <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
            <div>
              <h3 className="text-sm font-semibold text-blue-300 mb-2">ADF4350 (SPI)</h3>
              <table className="w-full text-sm">
                <tbody>
                  <tr className="border-b border-gray-700"><td className="py-1 text-gray-400">GPIO7</td><td className="py-1">→ SDATA (MOSI)</td></tr>
                  <tr className="border-b border-gray-700"><td className="py-1 text-gray-400">GPIO6</td><td className="py-1">→ SCLK</td></tr>
                  <tr className="border-b border-gray-700"><td className="py-1 text-gray-400">GPIO3</td><td className="py-1">→ LE</td></tr>
                  <tr className="border-b border-gray-700"><td className="py-1 text-gray-400">GPIO1</td><td className="py-1">→ CE</td></tr>
                  <tr><td className="py-1 text-gray-400">GPIO2</td><td className="py-1">← MUXOUT (Lock)</td></tr>
                </tbody>
              </table>
            </div>
            <div>
              <h3 className="text-sm font-semibold text-green-300 mb-2">Периферия</h3>
              <table className="w-full text-sm">
                <tbody>
                  <tr className="border-b border-gray-700"><td className="py-1 text-gray-400">GPIO8</td><td className="py-1">I2C SCL (дисплей)</td></tr>
                  <tr className="border-b border-gray-700"><td className="py-1 text-gray-400">GPIO9</td><td className="py-1">I2C SDA (дисплей)</td></tr>
                  <tr className="border-b border-gray-700"><td className="py-1 text-gray-400">GPIO0</td><td className="py-1">ADC батарея</td></tr>
                  <tr className="border-b border-gray-700"><td className="py-1 text-gray-400">GPIO4</td><td className="py-1">Кнопка 1 (пред.)</td></tr>
                  <tr className="border-b border-gray-700"><td className="py-1 text-gray-400">GPIO5</td><td className="py-1">Кнопка 3 (след.)</td></tr>
                  <tr><td className="py-1 text-gray-400">GPIO10</td><td className="py-1">LED</td></tr>
                </tbody>
              </table>
            </div>
          </div>
        </section>

        {/* Файлы проекта */}
        <section className="bg-gray-800 rounded-xl p-6 mb-6 border border-gray-700">
          <h2 className="text-xl font-semibold text-green-400 mb-4">📁 Файлы проекта</h2>
          <p className="text-sm text-gray-400 mb-4">
            Скопируй эти файлы в свой PlatformIO проект в VS Code.
          </p>
          
          <div className="space-y-3">
            {[
              { name: 'platformio.ini', desc: 'Конфигурация PlatformIO' },
              { name: 'include/adf4350.h', desc: 'Драйвер — пины и параметры' },
              { name: 'src/adf4350.cpp', desc: 'Реализация SPI-драйвера' },
              { name: 'src/main.cpp', desc: 'Основной код (кнопки, дисплей, LED)' },
            ].map(file => (
              <div key={file.name} className="flex items-center justify-between bg-gray-900 rounded-lg p-3 border border-gray-700">
                <div>
                  <span className="text-blue-300 font-mono text-sm">{file.name}</span>
                  <span className="text-gray-500 text-sm ml-3">{file.desc}</span>
                </div>
                <span className="text-xs text-green-400">✓ создан</span>
              </div>
            ))}
          </div>
        </section>

        {/* Инструкция */}
        <section className="bg-gray-800 rounded-xl p-6 border border-gray-700">
          <h2 className="text-xl font-semibold text-purple-400 mb-4">🚀 Как запустить</h2>
          <ol className="list-decimal list-inside space-y-2 text-sm text-gray-300">
            <li>Открой VS Code с расширением <strong>PlatformIO IDE</strong></li>
            <li>Создай новый проект: Board = <code className="bg-gray-700 px-1 rounded">ESP32-C3-DevKitM-1</code>, Framework = <code className="bg-gray-700 px-1 rounded">Arduino</code></li>
            <li>Замени файлы в проекте на файлы из этого репозитория</li>
            <li>В <code className="bg-gray-700 px-1 rounded">platformio.ini</code> укажи свой COM-порт</li>
            <li>Build (Ctrl+Alt+B) → Upload (Ctrl+Alt+U)</li>
            <li>Serial Monitor (Ctrl+Alt+M) на скорости 115200</li>
          </ol>
        </section>

        <footer className="mt-8 text-center text-sm text-gray-500">
          ADF4350 + ESP32-C3 • PlatformIO Project
        </footer>
      </div>
    </div>
  );
}

export default App;
