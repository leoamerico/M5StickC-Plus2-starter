const fs = require('fs');
const src = fs.readFileSync('../lib/audio_stream_handler.h', 'utf8');
const m = src.match(/R"CAMPAGE\(([\s\S]*?)\)CAMPAGE"/);
if (!m) { console.log('NOT FOUND via CAMPAGE'); process.exit(1); }
const html = m[1];
const renderIdx = html.indexOf('function renderDL()');
console.log('renderDL found at:', renderIdx);
if (renderIdx >= 0) console.log('renderDL source:', html.slice(renderIdx, renderIdx + 800));
// Find all dlBtnSave occurrences
let pos = 0;
while (true) {
  const idx = html.indexOf('dlBtnSave', pos);
  if (idx < 0) break;
  console.log('dlBtnSave at', idx, ':', html.slice(idx - 20, idx + 60));
  pos = idx + 1;
}
