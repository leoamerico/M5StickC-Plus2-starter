const fs = require('fs');
const zlib = require('zlib');
const src = fs.readFileSync('../lib/audio_stream_handler.h', 'utf8');
const m = src.match(/R"CAMPAGE\(([\s\S]*?)\)CAMPAGE"/);
if (!m) { console.log('nao achou'); process.exit(1); }
const html = m[1];
const gz = zlib.gzipSync(Buffer.from(html));
console.log('HTML original: ' + (html.length / 1024).toFixed(1) + ' KB');
console.log('HTML gzip:     ' + (gz.length / 1024).toFixed(1) + ' KB');
console.log('Reducao gzip:  ' + ((1 - gz.length / html.length) * 100).toFixed(0) + '%');
const lines = html.split('\n');
const comments = lines.filter(l => l.trim() === '' || l.trim().startsWith('//')).length;
console.log('Linhas vazias/comentario: ' + comments + ' / ' + lines.length);
// Minify estimate: remove comments, extra whitespace
const minified = html
  .replace(/\/\*[\s\S]*?\*\//g, '')
  .replace(/\/\/[^\n]*/g, '')
  .replace(/\s{2,}/g, ' ')
  .replace(/\n\s*\n/g, '\n')
  .trim();
const mingz = zlib.gzipSync(Buffer.from(minified));
console.log('Minified: ' + (minified.length / 1024).toFixed(1) + ' KB');
console.log('Minified+gzip: ' + (mingz.length / 1024).toFixed(1) + ' KB');
console.log('Economia Flash (sem gzip, so minify): ' + ((html.length - minified.length) / 1024).toFixed(1) + ' KB');
