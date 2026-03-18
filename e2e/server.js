/**
 * Servidor de testes para o Playwright
 *
 * Extrai automaticamente o HTML do firmware (audio_stream_handler.h)
 * e serve localmente com endpoints de suporte:
 *
 *   GET /         → redirect para /cam
 *   GET /cam      → página M5 Cam (HTML extraído do PROGMEM)
 *   GET /audio    → stream WAV fake (silêncio a 16 kHz, infinito)
 *   GET /manifest.json → PWA manifest
 *   GET /icon.svg      → ícone SVG
 */

'use strict';

const http = require('http');
const fs   = require('fs');
const path = require('path');

// ─── Extrai o HTML do arquivo C++ ─────────────────────────────────────────────

const handlerPath = path.join(__dirname, '..', 'lib', 'audio_stream_handler.h');
const src = fs.readFileSync(handlerPath, 'utf8');

const htmlMatch = src.match(/R"CAMPAGE\(([\s\S]*?)\)CAMPAGE"/);
if (!htmlMatch) {
  console.error('❌  Não encontrou CAM_PAGE_HTML em audio_stream_handler.h');
  process.exit(1);
}
const CAM_HTML = htmlMatch[1];
console.log(`✅  HTML extraído do firmware: ${(CAM_HTML.length / 1024).toFixed(1)} KB`);

// ─── Assets estáticos ─────────────────────────────────────────────────────────

const CAM_MANIFEST = JSON.stringify({
  name:             'M5 Cam',
  short_name:       'M5 Cam',
  start_url:        '/cam',
  display:          'standalone',
  background_color: '#000000',
  theme_color:      '#000000',
  orientation:      'portrait',
  icons: [{ src: '/icon.svg', sizes: 'any', type: 'image/svg+xml' }],
});

const CAM_ICON = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512">
  <rect width="512" height="512" rx="112" fill="#1C1C1E"/>
  <rect x="72" y="152" width="368" height="252" rx="44" fill="none"
        stroke="white" stroke-width="30"/>
  <circle cx="256" cy="268" r="76" fill="none" stroke="white" stroke-width="30"/>
  <circle cx="256" cy="268" r="36" fill="white"/>
  <rect x="178" y="118" width="84" height="46" rx="14" fill="white"/>
</svg>`;

// ─── WAV header helper (16-bit mono 16 kHz live stream) ───────────────────────

function wavHeader() {
  const buf = Buffer.alloc(44);
  buf.write('RIFF', 0);           buf.writeUInt32LE(0xFFFFFFFF, 4);
  buf.write('WAVE', 8);           buf.write('fmt ', 12);
  buf.writeUInt32LE(16, 16);      buf.writeUInt16LE(1,  20); // PCM
  buf.writeUInt16LE(1,  22);                                  // Mono
  buf.writeUInt32LE(16000, 24);                               // 16 kHz
  buf.writeUInt32LE(32000, 28);   buf.writeUInt16LE(2,  32);
  buf.writeUInt16LE(16, 34);      buf.write('data', 36);
  buf.writeUInt32LE(0xFFFFFFFF, 40);
  return buf;
}

// ─── Servidor HTTP ────────────────────────────────────────────────────────────

const server = http.createServer((req, res) => {
  const url = (req.url || '/').split('?')[0];

  if (url === '/') {
    res.writeHead(302, { Location: '/cam' });
    return res.end();
  }

  if (url === '/cam') {
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
    return res.end(CAM_HTML);
  }

  if (url === '/manifest.json') {
    res.writeHead(200, { 'Content-Type': 'application/manifest+json',
                         'Cache-Control': 'no-cache' });
    return res.end(CAM_MANIFEST);
  }

  if (url === '/icon.svg') {
    res.writeHead(200, { 'Content-Type': 'image/svg+xml',
                         'Cache-Control': 'public, max-age=86400' });
    return res.end(CAM_ICON);
  }

  if (url === '/audio') {
    // Stream WAV contínuo: header + chunks de silêncio a cada 50 ms
    // Simula exatamente o que o M5StickC transmite pela rede
    res.writeHead(200, {
      'Content-Type':                'audio/wav',
      'Cache-Control':               'no-cache, no-store',
      'Access-Control-Allow-Origin': '*',
      'Transfer-Encoding':           'chunked',
    });

    res.write(wavHeader()); // 44 bytes de header WAV

    // 1600 bytes = 50 ms @ 16 kHz 16-bit PCM mono
    // Tom de 440 Hz (Lá) — audível para confirmar que o pipeline funciona
    let phase = 0;
    const SAMPLES = 800; // 50 ms a 16 kHz
    const iv = setInterval(() => {
      if (res.destroyed) { clearInterval(iv); return; }
      const chunk = Buffer.alloc(SAMPLES * 2);
      for (let i = 0; i < SAMPLES; i++) {
        const sample = Math.round(Math.sin(phase) * 28000); // ~85% do máximo
        chunk.writeInt16LE(sample, i * 2);
        phase += (2 * Math.PI * 440) / 16000;
        if (phase > 2 * Math.PI) phase -= 2 * Math.PI;
      }
      try { res.write(chunk); }
      catch { clearInterval(iv); }
    }, 50);

    req.on('close',  () => clearInterval(iv));
    req.on('error',  () => clearInterval(iv));
    return; // mantém a conexão aberta
  }

  res.writeHead(404, { 'Content-Type': 'text/plain' });
  res.end('Not found\n');
});

const PORT = 3333;
server.listen(PORT, () => {
  console.log(`🚀  Servidor rodando em http://localhost:${PORT}`);
  console.log(`    Abra http://localhost:${PORT}/cam para ver a interface`);
});
