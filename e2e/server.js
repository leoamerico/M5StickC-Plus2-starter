/**
 * Servidor de testes para o Playwright
 *
 * Extrai automaticamente o HTML do firmware (audio_stream_handler.h)
 * e serve localmente com endpoints de suporte:
 *
 *   GET /         → redirect para /cam
 *   GET /cam      → página M5 Cam (HTML extraído do PROGMEM)
 *   GET /audio    → stream WAV fake (silêncio a 16 kHz, infinito)
 *   GET /token    → session token fake
 *   WS  /ws       → WebSocket para comandos do teleprompter
 */

'use strict';

const http = require('http');
const fs   = require('fs');
const path = require('path');
const { WebSocketServer } = require('ws');

// ─── Extrai o HTML do arquivo C++ ─────────────────────────────────────────────

const handlerPath = path.join(__dirname, '..', 'lib', 'audio_stream_handler.h');
const src = fs.readFileSync(handlerPath, 'utf8');

const htmlMatch = src.match(/R"CAMPAGE\(([\s\S]*?)\)CAMPAGE"/);
if (!htmlMatch) {
  console.error('❌  Não encontrou CAM_PAGE_HTML em audio_stream_handler.h');
  process.exit(1);
}
const CAM_HTML = htmlMatch[1];
console.log(`  HTML extraido do firmware: ${(CAM_HTML.length / 1024).toFixed(1)} KB`);

// Fake session token (matches firmware's 32-char hex)
const FAKE_TOKEN = 'aabbccdd11223344aabbccdd11223344';

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

  if (url === '/token') {
    res.writeHead(200, { 'Content-Type': 'text/plain',
                         'Cache-Control': 'no-cache, no-store' });
    return res.end(FAKE_TOKEN);
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

// ─── WebSocket server for teleprompter commands (/ws) ─────────────────────────

const wss = new WebSocketServer({ server, path: '/ws' });
wss.on('connection', (ws) => {
  ws.on('message', (msg) => {
    // Echo back to all clients (mimics firmware broadcast behavior)
    const text = msg.toString();
    wss.clients.forEach(c => { if (c.readyState === 1) c.send(text); });
  });
});

server.listen(PORT, () => {
  console.log(`  Servidor rodando em http://localhost:${PORT}`);
  console.log(`    Abra http://localhost:${PORT}/cam para ver a interface`);
});
