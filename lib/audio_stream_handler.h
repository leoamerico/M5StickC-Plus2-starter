#ifndef AUDIO_STREAM_HANDLER_H
#define AUDIO_STREAM_HANDLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include <esp_https_server.h>
#include "wifi_helper.h"
// TLS credentials are gitignored — run: node secrets/gen_header.js
// See secrets/README.md for setup instructions.
#include "../secrets/tls_credentials.h"
#if __has_include("cam_page_gz.h")
#include "cam_page_gz.h"
#endif

// SPM1423 PDM microphone on M5StickC Plus2
// CLK: GPIO0, DATA: GPIO34
#define MIC_CLK_PIN   0
#define MIC_DATA_PIN  34
#define I2S_PORT      I2S_NUM_0

#define SAMPLE_RATE   16000
#define SAMPLE_BITS   16
#define CHANNELS      1
#define DMA_BUF_COUNT 4
#define DMA_BUF_LEN   512
#define MAX_STREAM_MS 600000UL  // 10-minute session cap

// WAV file header (44 bytes, chunkSize filled at stream time = 0xFFFFFFFF for live)
static void writeWavHeader(uint8_t* buf, uint32_t dataSize = 0xFFFFFFFF) {
    uint32_t byteRate   = SAMPLE_RATE * CHANNELS * (SAMPLE_BITS / 8);
    uint16_t blockAlign = CHANNELS * (SAMPLE_BITS / 8);

    memcpy(buf,      "RIFF",     4);
    uint32_t chunkSize = (dataSize == 0xFFFFFFFF) ? 0xFFFFFFFF : dataSize + 36;
    memcpy(buf + 4,  &chunkSize, 4);
    memcpy(buf + 8,  "WAVE",     4);
    memcpy(buf + 12, "fmt ",     4);
    uint32_t subchunk1 = 16;  memcpy(buf + 16, &subchunk1, 4);
    uint16_t audioFmt  = 1;   memcpy(buf + 20, &audioFmt,  2); // PCM
    uint16_t ch        = CHANNELS;     memcpy(buf + 22, &ch,      2);
    uint32_t sr        = SAMPLE_RATE;  memcpy(buf + 24, &sr,      4);
    memcpy(buf + 28, &byteRate,   4);
    memcpy(buf + 32, &blockAlign, 2);
    uint16_t bps = SAMPLE_BITS;        memcpy(buf + 34, &bps,     2);
    memcpy(buf + 36, "data",     4);
    memcpy(buf + 40, &dataSize,  4);
}

// -------------------------------------------------------------------
// Camera + teleprompter + lavalier-mic recording page served at /cam
// iPhone front/rear camera + teleprompter overlay + ESP32 Web Audio -> MediaRecorder -> download
// M5Stick controls: BtnA = rec/stop  |  BtnB = teleprompter pause/resume  |  BtnPWR = scroll speed
// -------------------------------------------------------------------
static const char CAM_PAGE_HTML[] PROGMEM = R"CAMPAGE(
<!DOCTYPE html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover"><meta name="apple-mobile-web-app-capable" content="yes"><meta name="apple-mobile-web-app-status-bar-style" content="black-transparent"><meta name="apple-mobile-web-app-title" content="M5 Cam"><meta name="theme-color" content="#000000"><link rel="manifest" href="data:application/json,{%22name%22:%22M5 Cam%22,%22short_name%22:%22M5 Cam%22,%22start_url%22:%22/cam%22,%22display%22:%22standalone%22,%22background_color%22:%22%23000%22,%22theme_color%22:%22%23000%22,%22orientation%22:%22portrait%22}"><link rel="apple-touch-icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 512 512'%3E%3Crect width='512' height='512' rx='112' fill='%231C1C1E'/%3E%3Crect x='72' y='152' width='368' height='252' rx='44' fill='none' stroke='white' stroke-width='30'/%3E%3Ccircle cx='256' cy='268' r='76' fill='none' stroke='white' stroke-width='30'/%3E%3Ccircle cx='256' cy='268' r='36' fill='white'/%3E%3Crect x='178' y='118' width='84' height='46' rx='14' fill='white'/%3E%3Ccircle cx='398' cy='200' r='20' fill='white'/%3E%3C/svg%3E"><title>M5 Cam</title><style>*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent}:root{--st:env(safe-area-inset-top,0px);--sb:env(safe-area-inset-bottom,0px);--blue:#007AFF;--red:#FF3B30;--green:#34C759;--orange:#FF9500;--glass:rgba(26,26,28,.78);--cell:rgba(255,255,255,.07);--sep:rgba(255,255,255,.11);--t2:rgba(255,255,255,.55)}html,body{height:100%;overflow:hidden}body{background:#000;color:#fff;font-family:-apple-system,BlinkMacSystemFont,"Helvetica Neue",Arial,sans-serif;-webkit-font-smoothing:antialiased}#preview{position:fixed;inset:0;width:100%;height:100%;object-fit:cover}#flash{position:fixed;inset:0;background:#fff;opacity:0;pointer-events:none;z-index:99;transition:opacity .14s ease-out}#hud{position:fixed;top:0;left:0;right:0;z-index:10;padding-top:calc(var(--st) + 12px);padding-left:16px;padding-right:16px;padding-bottom:0;display:flex;align-items:flex-start;justify-content:space-between;pointer-events:none}#pill{display:inline-flex;align-items:center;gap:6px;background:rgba(0,0,0,.8);border-radius:20px;padding:6px 12px 6px 9px;backdrop-filter:blur(24px);-webkit-backdrop-filter:blur(24px);opacity:0;transform:scale(.88);transition:opacity .35s cubic-bezier(.34,1.56,.64,1),transform .35s cubic-bezier(.34,1.56,.64,1)}#pill.show{opacity:1;transform:scale(1)}#pillDot{width:8px;height:8px;border-radius:50%;background:#444;transition:background .3s;flex-shrink:0}#pill.live #pillDot{background:var(--red);animation:throb .9s ease infinite}@keyframes throb{0%,100%{opacity:1;transform:scale(1)}50%{opacity:.5;transform:scale(.8)}}#pillTime{font-size:13px;font-weight:600;letter-spacing:.4px;min-width:40px}#micWave{display:flex;align-items:center;gap:2px;height:14px;margin-left:3px}.wb{width:3px;border-radius:2px;background:rgba(255,255,255,.35);transition:height .07s ease,background .2s}#hudTimer{font-size:16px;font-weight:600;letter-spacing:.4px;background:rgba(0,0,0,.6);border-radius:10px;padding:5px 10px;backdrop-filter:blur(12px);-webkit-backdrop-filter:blur(12px);opacity:0;transition:opacity .3s;margin-top:4px}#hudTimer.show{opacity:1}#sheet{position:fixed;bottom:0;left:0;right:0;z-index:10;background:var(--glass);backdrop-filter:blur(48px) saturate(200%);-webkit-backdrop-filter:blur(48px) saturate(200%);border-radius:22px 22px 0 0;border-top:.5px solid var(--sep);padding:0 16px;padding-bottom:calc(var(--sb) + 16px)}#handle{width:36px;height:5px;background:rgba(255,255,255,.22);border-radius:3px;margin:10px auto 14px}#statusRow{display:flex;align-items:center;gap:8px;margin-bottom:14px;padding:0 2px}#sIcon{width:8px;height:8px;border-radius:50%;background:#444;flex-shrink:0;transition:background .4s}#sIcon.ok{background:var(--green)}#sIcon.warn{background:var(--orange)}#sIcon.err{background:var(--red)}#sMsg{font-size:13px;color:var(--t2);font-weight:400;letter-spacing:.05px}#startBtn{display:block;width:100%;padding:16px;background:var(--blue);color:#fff;border:none;border-radius:14px;font-size:17px;font-weight:600;letter-spacing:-.2px;cursor:pointer;transition:filter .12s,transform .1s;margin-bottom:6px}#startBtn:active{filter:brightness(.82);transform:scale(.985)}#recRow{display:none;justify-content:center;align-items:center;padding:6px 0 12px}#recBtn{width:72px;height:72px;border-radius:50%;border:3px solid rgba(255,255,255,.82);background:transparent;cursor:pointer;display:flex;align-items:center;justify-content:center;transition:transform .12s cubic-bezier(.34,1.56,.64,1)}#recBtn:active{transform:scale(.9)}#recDot{width:56px;height:56px;border-radius:50%;background:var(--red);transition:all .22s cubic-bezier(.34,1.56,.64,1)}#recBtn.on #recDot{border-radius:9px;width:26px;height:26px}#dlSection{display:none;flex-direction:column;gap:10px;padding-bottom:4px}#dlTitle{font-size:12px;font-weight:600;color:var(--t2);text-transform:uppercase;letter-spacing:.7px;padding:0 4px 2px}#dlList{display:flex;flex-direction:column;gap:1px;border-radius:14px;overflow:hidden;background:var(--sep)}.dlRow{display:flex;align-items:center;gap:12px;background:var(--cell);padding:11px 14px;text-decoration:none;color:#fff;transition:background .12s;backdrop-filter:blur(0)}.dlRow:active{background:rgba(255,255,255,.14)}.dlRow:first-child{border-radius:14px 14px 0 0}.dlRow:last-child{border-radius:0 0 14px 14px}.dlRow:only-child{border-radius:14px}.dlIcon{width:38px;height:38px;border-radius:10px;background:linear-gradient(135deg,#1a6fff,#0040dd);display:flex;align-items:center;justify-content:center;font-size:20px;flex-shrink:0}.dlMeta{flex:1;min-width:0}.dlName{font-size:14px;font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}.dlSub{font-size:12px;color:var(--t2);margin-top:2px}.dlChevron{color:var(--t2);font-size:15px;font-weight:600;padding-left:4px}.dlActions{display:flex;gap:6px;flex-shrink:0}.dlBtn{width:36px;height:36px;border-radius:10px;border:none;display:flex;align-items:center;justify-content:center;font-size:17px;cursor:pointer;transition:filter .12s,transform .1s}.dlBtn:active{filter:brightness(.75);transform:scale(.91)}.dlBtnSave{background:var(--blue)}.dlBtnShare{background:rgba(255,255,255,.13)}#newBtn{width:100%;padding:14px;background:rgba(255,255,255,.1);color:#fff;border:none;border-radius:14px;font-size:15px;font-weight:600;cursor:pointer;transition:background .12s}#newBtn:active{background:rgba(255,255,255,.18)}#errMsg{font-size:12px;color:var(--red);text-align:center;display:none;padding:2px 0 6px;line-height:1.45}#tpOverlay{position:fixed;inset:0;z-index:20;pointer-events:none;display:none;-webkit-mask-image:linear-gradient(transparent 0%,#000 18%,#000 82%,transparent 100%);mask-image:linear-gradient(transparent 0%,#000 18%,#000 82%,transparent 100%)}#tpOverlay.active{display:block}#tpTrack{position:absolute;left:0;right:0;top:0;padding:52vh 20px 52vh}.tpLine{font-size:clamp(22px,5.5vw,34px);font-weight:700;line-height:1.45;color:#fff;text-align:left;text-shadow:0 2px 12px rgba(0,0,0,.9),0 0 4px rgba(0,0,0,.8);letter-spacing:-.2px;padding:0 4px;transition:opacity .2s;white-space:pre-wrap;word-break:break-word}.tpLine.active{color:#fff;font-size:clamp(24px,6vw,38px)}#tpBar{position:fixed;top:calc(var(--st)+8px);left:50%;transform:translateX(-50%);z-index:30;display:none;gap:8px;align-items:center}#tpBar.show{display:flex}.tpCtrl{height:32px;border-radius:16px;border:none;padding:0 14px;font-size:13px;font-weight:600;color:#fff;cursor:pointer;backdrop-filter:blur(20px);-webkit-backdrop-filter:blur(20px);transition:filter .1s,transform .09s}.tpCtrl:active{filter:brightness(.75);transform:scale(.93)}#tpPauseBtn{background:rgba(0,0,0,.72);min-width:36px}#tpSpeedBtn{background:rgba(0,0,0,.55)}#tpStopBtn{background:rgba(200,0,0,.72)}#camToggle{position:fixed;top:calc(var(--st)+8px);right:16px;z-index:30;height:32px;border-radius:16px;border:.5px solid rgba(255,255,255,.25);padding:0 12px;font-size:13px;font-weight:600;color:#fff;cursor:pointer;background:rgba(0,0,0,.6);backdrop-filter:blur(20px);-webkit-backdrop-filter:blur(20px);display:none}#camToggle.show{display:block}#editorOverlay{position:fixed;inset:0;z-index:100;background:rgba(0,0,0,.96);display:flex;flex-direction:column;padding:calc(var(--st)+16px) 0 calc(var(--sb)+16px)}#editorOverlay.hidden{display:none}#editorHeader{display:flex;align-items:center;justify-content:space-between;padding:0 16px 12px;border-bottom:.5px solid var(--sep)}#editorHeader h2{font-size:17px;font-weight:700;letter-spacing:-.3px}#editorDone{height:34px;border-radius:10px;border:none;padding:0 18px;font-size:15px;font-weight:600;background:var(--blue);color:#fff;cursor:pointer}#scriptArea{flex:1;width:100%;background:transparent;border:none;outline:none;color:#fff;font-size:17px;line-height:1.7;padding:16px;resize:none;font-family:-apple-system,BlinkMacSystemFont,"Helvetica Neue",Arial,sans-serif}#scriptArea::placeholder{color:rgba(255,255,255,.25)}#editorFooter{padding:10px 16px 0;display:flex;gap:8px;align-items:center;flex-wrap:wrap}#editorFooter span{font-size:12px;color:var(--t2);flex:1}.edPreset{height:28px;border-radius:8px;border:.5px solid var(--sep);padding:0 10px;font-size:12px;font-weight:500;background:var(--cell);color:#fff;cursor:pointer}#speedBadge{position:fixed;bottom:calc(var(--sb)+140px);left:50%;transform:translateX(-50%);z-index:35;background:rgba(0,0,0,.8);color:#fff;font-size:13px;font-weight:600;padding:5px 14px;border-radius:14px;pointer-events:none;opacity:0;transition:opacity .4s}#speedBadge.show{opacity:1}</style></head><body><video id="preview" autoplay playsinline muted></video><div id="flash"></div> <!-- Teleprompter overlay --><div id="tpOverlay"><div id="tpTrack"></div></div> <!-- Teleprompter controls bar (visible during recording) --><div id="tpBar"> <button class="tpCtrl" id="tpPauseBtn" onclick="tpTogglePause()">&#9646;&#9646;</button> <button class="tpCtrl" id="tpSpeedBtn" onclick="tpCycleSpeed()">1.0&#215;</button> <button class="tpCtrl" id="tpStopBtn" onclick="tpHide()">&#9646; Hide</button></div> <!-- Speed badge (momentary feedback) --><div id="speedBadge"></div> <!-- Camera toggle (front/rear) --><button id="camToggle" onclick="toggleCamera()">&#128247; Rear</button> <div id="hud"> <div id="pill"> <div id="pillDot"></div> <span id="pillTime"></span> <div id="micWave"> <div class="wb" id="wb0" style="height:4px"></div> <div class="wb" id="wb1" style="height:9px"></div> <div class="wb" id="wb2" style="height:6px"></div> <div class="wb" id="wb3" style="height:11px"></div> <div class="wb" id="wb4" style="height:5px"></div> </div> </div> <span id="hudTimer"></span></div> <div id="sheet"> <div id="handle"></div> <div id="statusRow"> <div id="sIcon"></div> <span id="sMsg">M5Stick lavalier &bull; iPhone camera</span> </div> <button id="startBtn" onclick="startAll()">Start Camera &amp; Mic</button> <div id="recRow"> <button id="recBtn" onclick="toggleRec()"><div id="recDot"></div></button> </div> <div id="dlSection"> <div id="dlTitle">Recordings</div> <div id="dlList"></div> <button id="newBtn" onclick="newRec()">&#9679;&nbsp; New Recording</button> </div> <div id="errMsg"></div> <!-- Teleprompter launch button (shown after camera starts) --> <button id="tpLaunchBtn" onclick="openEditor()" style="display:none;width:100%;padding:12px;background:rgba(255,255,255,.08);color:#fff;border:none;border-radius:14px;font-size:15px;font-weight:600;cursor:pointer;margin-top:6px;transition:background .12s">&#128218;&nbsp; Teleprompter</button></div> <!-- iOS Add to Home Screen overlay --><div id="a2hsOverlay" style="display:none;position:fixed;inset:0;z-index:200;background:rgba(0,0,0,.72);backdrop-filter:blur(20px);-webkit-backdrop-filter:blur(20px);align-items:flex-end;justify-content:center"> <div style="background:#1C1C1E;border-radius:22px 22px 0 0;width:100%;padding:28px 24px;padding-bottom:max(env(safe-area-inset-bottom,0px),28px);border-top:.5px solid rgba(255,255,255,.15)"> <div style="width:36px;height:5px;background:rgba(255,255,255,.2);border-radius:3px;margin:0 auto 22px"></div> <div style="display:flex;align-items:center;gap:16px;margin-bottom:20px"> <img src="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 512 512'%3E%3Crect width='512' height='512' rx='112' fill='%231C1C1E'/%3E%3Crect x='72' y='152' width='368' height='252' rx='44' fill='none' stroke='white' stroke-width='30'/%3E%3Ccircle cx='256' cy='268' r='76' fill='none' stroke='white' stroke-width='30'/%3E%3Ccircle cx='256' cy='268' r='36' fill='white'/%3E%3Crect x='178' y='118' width='84' height='46' rx='14' fill='white'/%3E%3Ccircle cx='398' cy='200' r='20' fill='white'/%3E%3C/svg%3E" style="width:56px;height:56px;border-radius:14px;flex-shrink:0" alt=""> <div> <div style="font-size:17px;font-weight:700;letter-spacing:-.3px">M5 Cam</div> <div style="font-size:13px;color:rgba(255,255,255,.55);margin-top:3px">Add to Home Screen</div> </div> </div> <div style="background:rgba(255,255,255,.07);border-radius:14px;padding:16px;margin-bottom:16px"> <div style="display:flex;align-items:flex-start;gap:14px;margin-bottom:14px"> <span style="font-size:22px;flex-shrink:0">&#11014;&#65039;</span> <div> <div style="font-size:14px;font-weight:600">Tap the Share button</div> <div style="font-size:12px;color:rgba(255,255,255,.5);margin-top:2px">at the bottom of Safari</div> </div> </div> <div style="display:flex;align-items:flex-start;gap:14px"> <span style="font-size:22px;flex-shrink:0">&#10133;</span> <div> <div style="font-size:14px;font-weight:600">Tap &ldquo;Add to Home Screen&rdquo;</div> <div style="font-size:12px;color:rgba(255,255,255,.5);margin-top:2px">Abre em tela cheia, sem barra do browser</div> </div> </div> </div> <button onclick="dismissA2HS()" style="width:100%;padding:15px;background:#007AFF;color:#fff;border:none;border-radius:14px;font-size:17px;font-weight:600;cursor:pointer">Entendido</button> </div></div> <!-- Script editor overlay --><div id="editorOverlay"> <div id="editorHeader"> <h2>&#128218; Script</h2> <button id="editorDone" onclick="closeEditor()">Done</button> </div> <textarea id="scriptArea" placeholder="Paste or type your script here.&#10;&#10;The teleprompter will scroll it automatically during recording.&#10;&#10;Tip: shorter paragraphs read better on camera." spellcheck="true"></textarea> <div id="editorFooter"> <span id="charCount">0 words</span> <button class="edPreset" onclick="tpSetSpeed(0.4)">Slow</button> <button class="edPreset" onclick="tpSetSpeed(0.8)">Normal</button> <button class="edPreset" onclick="tpSetSpeed(1.4)">Fast</button> </div></div> <script>var recorder,stream,audioCtx,analyser,dest;
var chunks=[],recordings=[];
var timerIv,recSec=0;
var nextWhen=0,hdrBuf=new Uint8Array(0),hdrDone=false;
var audioFlowing=false;
var facingMode="user";
var bars=["wb0","wb1","wb2","wb3","wb4"].map(function(id){return document.getElementById(id);});
var barH=[4,9,6,11,5];

var tpRunning=false,tpPaused=false;
var tpPos=0,tpRaf=null;
var tpSpeed=0.8;
var tpSpeedLevels=[0.3,0.5,0.8,1.2,1.8,2.5];
var tpSpeedIdx=2;
var tpLines=[];
var tpBadgeTimer=null;
var _ws=null;
function connectWS(){
if(_ws)return;
var ws=new WebSocket("wss://"+location.host+"/ws");
_ws=ws;
ws.onmessage=function(e){var d=e.data;
if(d==="tp_pause")tpTogglePause();
else if(d==="tp_speed")tpCycleSpeed();
else if(d==="rec"&&audioFlowing)toggleRec();
};
ws.onclose=function(){_ws=null;setTimeout(connectWS,3000);};
ws.onerror=function(){};
}

function setStatus(msg,type){
document.getElementById("sMsg").textContent=msg;
var ic=document.getElementById("sIcon");
ic.className=type||"";
}
function showErr(t){
var e=document.getElementById("errMsg");
e.textContent=t;e.style.display="block";
setStatus(t,"err");
}
function syncMark(){
var t=audioCtx.currentTime+0.05;
[0,0.05,0.10].forEach(function(dt){
var osc=audioCtx.createOscillator(),g=audioCtx.createGain();
osc.frequency.value=880;
g.gain.setValueAtTime(0,t+dt);
g.gain.linearRampToValueAtTime(0.85,t+dt+0.005);
g.gain.exponentialRampToValueAtTime(0.001,t+dt+0.04);
osc.connect(g);g.connect(analyser);
osc.start(t+dt);osc.stop(t+dt+0.05);
});
var fl=document.getElementById("flash");
fl.style.opacity="0.92";
setTimeout(function(){fl.style.opacity="0";},150);
}

async function getCameraStream(){
if(stream){
stream.getVideoTracks().forEach(function(t){t.stop();});
}
var cam=await navigator.mediaDevices.getUserMedia({
video:{facingMode:facingMode,width:{ideal:1920},height:{ideal:1080}},
audio:false
});
document.getElementById("preview").srcObject=cam;

document.getElementById("preview").style.transform=
facingMode==="user"?"scaleX(-1)":"none";
return cam;
}

async function toggleCamera(){
facingMode=facingMode==="user"?"environment":"user";
var btn=document.getElementById("camToggle");
btn.textContent=facingMode==="user"?"\uD83D\uDCF7 Rear":"\uD83E\uDD33 Front";
try{
var cam=await getCameraStream();
if(dest&&audioFlowing){
stream=new MediaStream(cam.getVideoTracks().concat(dest.stream.getAudioTracks()));
}
}catch(e){showErr(e.message);}
}

async function startAll(){
document.getElementById("startBtn").style.display="none";
setStatus("Opening camera...","warn");
try{
var cam=await getCameraStream();
setStatus("Connecting M5Stick mic...","warn");
audioCtx=new(window.AudioContext||window.webkitAudioContext)({sampleRate:16000});
dest=audioCtx.createMediaStreamDestination();
analyser=audioCtx.createAnalyser();analyser.fftSize=128;
analyser.connect(dest);
analyser.connect(audioCtx.destination);
nextWhen=audioCtx.currentTime+0.3;
var tokenP=window._audioToken?Promise.resolve(window._audioToken):fetch("/token").then(function(r){return r.text();});
tokenP.then(function(tok){window._audioToken=tok;
fetch("/audio?t="+tok).then(function(r){
var rd=r.body.getReader();
(function pump(){
rd.read().then(function(v){
if(v.done){setStatus("Mic ended \u2014 reload","err");return;}
var pcm=v.value;
if(!hdrDone){
var tmp=new Uint8Array(hdrBuf.length+v.value.length);
tmp.set(hdrBuf);tmp.set(v.value,hdrBuf.length);hdrBuf=tmp;
if(hdrBuf.length<44){pump();return;}
pcm=hdrBuf.slice(44);hdrDone=true;hdrBuf=null;
}
if(pcm.length>=2){
var n=Math.floor(pcm.length/2),ab=audioCtx.createBuffer(1,n,16000);
var ch=ab.getChannelData(0),dv=new DataView(pcm.buffer,pcm.byteOffset);
for(var i=0;i<n;i++)ch[i]=dv.getInt16(i*2,true)/32768;
var src=audioCtx.createBufferSource();
src.buffer=ab;src.connect(analyser);
var w=Math.max(nextWhen,audioCtx.currentTime+0.05);
src.start(w);nextWhen=w+ab.duration;
if(!audioFlowing){
audioFlowing=true;
stream=new MediaStream(cam.getVideoTracks().concat(dest.stream.getAudioTracks()));
document.getElementById("recRow").style.display="flex";
document.getElementById("tpLaunchBtn").style.display="block";
document.getElementById("pill").classList.add("show");
document.getElementById("camToggle").classList.add("show");
setStatus("Ready \u2014 tap \u25CF to record","ok");
connectWS();
}
}
pump();
}).catch(function(){setStatus("Mic stream ended","err");});
})();
}).catch(function(){showErr("Cannot reach M5Stick. Same WiFi?");});
});
var md=new Uint8Array(analyser.frequencyBinCount);
(function meter(){
analyser.getByteTimeDomainData(md);
var rms=0;
for(var i=0;i<md.length;i++){var x=(md[i]-128)/128;rms+=x*x;}
var lvl=Math.min(1,Math.sqrt(rms/md.length)*8);
bars.forEach(function(b,i){
var h=Math.round(barH[i]+(14-barH[i])*lvl*(0.5+0.5*Math.random()));
b.style.height=h+"px";
b.style.background=lvl>.55?"var(--red)":"rgba(255,255,255,.6)";
});
requestAnimationFrame(meter);
})();
}catch(e){
showErr(e.name==="NotAllowedError"?"Camera permission denied.":e.message);
document.getElementById("startBtn").style.display="block";
}
}

function toggleRec(){
if(recorder&&recorder.state==="recording"){
recorder.stop();clearInterval(timerIv);
document.getElementById("recBtn").classList.remove("on");
document.getElementById("pill").classList.remove("live");
document.getElementById("hudTimer").classList.remove("show");
document.getElementById("hudTimer").textContent="";
setStatus("Processing...","warn");

if(tpRunning)tpStop();
return;
}
chunks=[];recSec=0;
var mimes=["video/mp4;codecs=avc1","video/mp4","video/webm;codecs=vp8","video/webm"];
var mime=mimes.find(function(m){try{return MediaRecorder.isTypeSupported(m);}catch(e){return false;}})||"";
try{recorder=new MediaRecorder(stream,mime?{mimeType:mime}:{});}
catch(e){recorder=new MediaRecorder(stream);}
recorder.ondataavailable=function(e){if(e.data.size>0)chunks.push(e.data);};
recorder.onstop=function(){
var blob=new Blob(chunks,{type:recorder.mimeType||"video/mp4"});
var url=URL.createObjectURL(blob);
var ext=(recorder.mimeType||"video/mp4").indexOf("mp4")>=0?"mp4":"webm";
var ts=new Date().toISOString().replace(/[:.]/g,"-").slice(0,19);
var dur=recSec;
var sizeMB=(blob.size/1048576).toFixed(1);
recordings.unshift({url:url,name:"m5_"+ts+"."+ext,dur:dur,sizeMB:sizeMB});
renderDL();
document.getElementById("recRow").style.display="none";
document.getElementById("tpLaunchBtn").style.display="none";
document.getElementById("dlSection").style.display="flex";
var mm=("0"+Math.floor(dur/60)).slice(-2),ss=("0"+dur%60).slice(-2);
setStatus("Saved \u2014 "+mm+":"+ss+" \u00B7 "+sizeMB+" MB","ok");
};
syncMark();
recorder.start(500);
document.getElementById("recBtn").classList.add("on");
document.getElementById("pill").classList.add("live");
document.getElementById("hudTimer").classList.add("show");
document.getElementById("dlSection").style.display="none";
document.getElementById("errMsg").style.display="none";
timerIv=setInterval(function(){
recSec++;
var mm=("0"+Math.floor(recSec/60)).slice(-2),ss=("0"+recSec%60).slice(-2);
document.getElementById("pillTime").textContent=mm+":"+ss;
document.getElementById("hudTimer").textContent=mm+":"+ss;
setStatus("\u25CF "+mm+":"+ss);
},1000);

if(tpLines.length>0)tpStart();
}

function renderDL(){
var list=document.getElementById("dlList");
list.innerHTML="";
recordings.forEach(function(r,idx){
var mm=("0"+Math.floor(r.dur/60)).slice(-2),ss=("0"+r.dur%60).slice(-2);
var row=document.createElement("div");
row.className="dlRow";
var canShare=!!(navigator.canShare&&navigator.canShare({files:[new File([],"t.mp4",{type:"video/mp4"})]}));
row.innerHTML="<div class=\"dlIcon\">&#127916;</div>"
+"<div class=\"dlMeta\"><div class=\"dlName\">"+r.name+"</div>"
+"<div class=\"dlSub\">"+mm+":"+ss+" &bull; "+r.sizeMB+" MB</div></div>"
+"<div class=\"dlActions\">"
+(canShare?"<button class=\"dlBtn dlBtnShare\" title=\"Share\" onclick=\"shareRec("+idx+")\" aria-label=\"Share\">&#129310;</button>":"")
+"<button class=\"dlBtn dlBtnSave\" title=\"Save\" onclick=\"saveRec("+idx+")\" aria-label=\"Save to device\">&#8681;</button>"
+"</div>";
list.appendChild(row);
});
}
async function saveRec(idx){
var r=recordings[idx];
var ext=r.name.split(".").pop();
var mime=ext==="mp4"?"video/mp4":"video/webm";
if(window.showSaveFilePicker){
try{
var fh=await window.showSaveFilePicker({suggestedName:r.name,types:[{description:"Video",accept:{[mime]:["." +ext]}}]});
var blob=await fetch(r.url).then(function(res){return res.blob();});
var w=await fh.createWritable();await w.write(blob);await w.close();
setStatus("Saved \u2014 "+r.name,"ok");return;
}catch(e){if(e.name==="AbortError")return;}
}
var a=document.createElement("a");a.href=r.url;a.download=r.name;
document.body.appendChild(a);a.click();document.body.removeChild(a);
}
async function shareRec(idx){
var r=recordings[idx];
try{
var blob=await fetch(r.url).then(function(res){return res.blob();});
var file=new File([blob],r.name,{type:blob.type});
await navigator.share({files:[file],title:r.name});
}catch(e){if(e.name!=="AbortError")setStatus("Share failed","err");}
}
function newRec(){
document.getElementById("dlSection").style.display="none";
document.getElementById("recRow").style.display="flex";
document.getElementById("tpLaunchBtn").style.display="block";
setStatus("Ready \u2014 tap to record","ok");
}

function tpBuildLines(text){
tpLines=[];
var raw=text.replace(/\r\n/g,"\n").replace(/\r/g,"\n");
var paras=raw.split(/\n{2,}/);
paras.forEach(function(p){
var t=p.trim();
if(t.length===0)return;

var words=t.split(/\s+/);
var line="";
words.forEach(function(w){
if((line+" "+w).trim().length>58&&line.length>0){
tpLines.push(line.trim());line=w;
}else{
line=(line+" "+w).trim();
}
});
if(line.trim())tpLines.push(line.trim());
tpLines.push("");
});

var track=document.getElementById("tpTrack");
track.innerHTML="";
tpLines.forEach(function(l,i){
var d=document.createElement("div");
d.className="tpLine";
d.setAttribute("data-idx",i);
d.textContent=l;
track.appendChild(d);
});
}

function tpStart(){
if(!tpLines.length)return;
tpPos=0;
tpPaused=false;
tpRunning=true;
document.getElementById("tpTrack").style.transform="translateY(0px)";
document.getElementById("tpOverlay").classList.add("active");
document.getElementById("tpBar").classList.add("show");
document.getElementById("tpPauseBtn").textContent="\u23F8";
tpAnimate();
}

function tpStop(){
tpRunning=false;
tpPaused=false;
cancelAnimationFrame(tpRaf);
document.getElementById("tpOverlay").classList.remove("active");
document.getElementById("tpBar").classList.remove("show");
}

function tpAnimate(){
if(!tpRunning||tpPaused){tpRaf=null;return;}
tpPos+=tpSpeed;
document.getElementById("tpTrack").style.transform="translateY(-"+tpPos+"px)";

var track=document.getElementById("tpTrack");
var midY=window.innerHeight/2;
var trackTop=track.getBoundingClientRect().top;
var lines=track.querySelectorAll(".tpLine");
var best=null,bestDist=9999;
lines.forEach(function(ln){
var r=ln.getBoundingClientRect();
var cy=r.top+r.height/2;
var d=Math.abs(cy-midY);
if(d<bestDist){bestDist=d;best=ln;}
});
lines.forEach(function(ln){ln.classList.remove("active");});
if(best)best.classList.add("active");

var lastLine=lines[lines.length-1];
if(lastLine){
var r=lastLine.getBoundingClientRect();
if(r.bottom<midY){tpStop();return;}
}
tpRaf=requestAnimationFrame(tpAnimate);
}

function tpTogglePause(){
if(!tpRunning)return;
tpPaused=!tpPaused;
document.getElementById("tpPauseBtn").textContent=tpPaused?"\u25B6":"\u23F8";
if(!tpPaused)tpAnimate();
}

function tpHide(){
document.getElementById("tpOverlay").classList.remove("active");
document.getElementById("tpBar").classList.remove("show");
}

function tpShow(){
if(!tpRunning&&tpLines.length>0)tpStart();
else{
document.getElementById("tpOverlay").classList.add("active");
document.getElementById("tpBar").classList.add("show");
}
}

function tpSetSpeed(s){
tpSpeed=s;
tpSpeedIdx=tpSpeedLevels.reduce(function(best,v,i){
return Math.abs(v-s)<Math.abs(tpSpeedLevels[best]-s)?i:best;
},0);
tpSpeedIdx=tpSpeedLevels.indexOf(
tpSpeedLevels.reduce(function(prev,curr){return Math.abs(curr-s)<Math.abs(prev-s)?curr:prev;})
);
document.getElementById("tpSpeedBtn").textContent=tpSpeed.toFixed(1)+"\xD7";
tpShowSpeedBadge();
}

function tpCycleSpeed(){
tpSpeedIdx=(tpSpeedIdx+1)%tpSpeedLevels.length;
tpSpeed=tpSpeedLevels[tpSpeedIdx];
document.getElementById("tpSpeedBtn").textContent=tpSpeed.toFixed(1)+"\xD7";
tpShowSpeedBadge();
}

function tpShowSpeedBadge(){
var b=document.getElementById("speedBadge");
b.textContent="Speed: "+tpSpeed.toFixed(1)+"\xD7";
b.classList.add("show");
clearTimeout(tpBadgeTimer);
tpBadgeTimer=setTimeout(function(){b.classList.remove("show");},1200);
}

function openEditor(){
document.getElementById("editorOverlay").classList.remove("hidden");
setTimeout(function(){document.getElementById("scriptArea").focus();},200);
}

function closeEditor(){
var text=document.getElementById("scriptArea").value.trim();
document.getElementById("editorOverlay").classList.add("hidden");
if(text.length>0){
tpBuildLines(text);
localStorage.setItem("m5_tp_script",text);
setStatus("Script loaded \u2014 "+tpLines.filter(function(l){return l.length>0;}).length+" lines","ok");
}
}

document.getElementById("scriptArea").addEventListener("input",function(){
var words=this.value.trim().split(/\s+/).filter(function(w){return w.length>0;});
document.getElementById("charCount").textContent=words.length+" words \u2248 "+Math.ceil(words.length/130)+" min";
});

(function(){
var saved=localStorage.getItem("m5_tp_script");
if(saved){
document.getElementById("scriptArea").value=saved;
tpBuildLines(saved);
var words=saved.trim().split(/\s+/).filter(function(w){return w.length>0;});
document.getElementById("charCount").textContent=words.length+" words \u2248 "+Math.ceil(words.length/130)+" min";
}

document.getElementById("editorOverlay").classList.add("hidden");
})();

(function(){
var standalone=window.navigator.standalone===true||
window.matchMedia("(display-mode:standalone)").matches;
if(standalone)return;
if(localStorage.getItem("m5_a2hs_seen"))return;
setTimeout(function(){
var ov=document.getElementById("a2hsOverlay");
ov.style.display="flex";
},1800);
})();
function dismissA2HS(){
localStorage.setItem("m5_a2hs_seen","1");
document.getElementById("a2hsOverlay").style.display="none";
}</script></body></html>
)CAMPAGE";

class AudioStreamHandler {
private:
    httpd_handle_t   httpsServer;    // ESP-IDF native HTTPS on port 443
    bool i2sRunning;
    bool serverRunning;
    volatile bool streamActive;
    unsigned long streamStart;
    char sessionToken[33]; // 32-char hex token, regenerated each start()
    volatile int wsFd;       // WebSocket client fd (-1 = none)

    // ── Static HTTPS handlers (ESP-IDF native C API) ─────────────────────────
    static esp_err_t sRoot(httpd_req_t* r) {
        httpd_resp_set_status(r, "302 Found");
        httpd_resp_set_hdr(r, "Location", "/cam");
        httpd_resp_send(r, NULL, 0);
        return ESP_OK;
    }

    static esp_err_t sCam(httpd_req_t* r) {
#if __has_include("cam_page_gz.h")
        // Serve gzip-compressed HTML (saves ~20 KB Flash)
        httpd_resp_set_type(r, "text/html; charset=utf-8");
        httpd_resp_set_hdr(r, "Content-Encoding", "gzip");
        httpd_resp_set_hdr(r, "Vary", "Accept-Encoding");
        httpd_resp_send(r, (const char*)CAM_PAGE_GZ, CAM_PAGE_GZ_LEN);
        return ESP_OK;
#else
        // Fallback: serve raw HTML with inline token injection
        auto* self = static_cast<AudioStreamHandler*>(r->user_ctx);
        httpd_resp_set_type(r, "text/html; charset=utf-8");
        const char* MARKER = "</head>";
        const char* data = CAM_PAGE_HTML;
        const char* pos = strstr(data, MARKER);
        if (!pos) pos = data + strlen(data);
        size_t pre = pos - data;
        for (size_t off = 0; off < pre; ) {
            size_t n = (pre - off < 1024) ? (pre - off) : 1024;
            if (httpd_resp_send_chunk(r, data + off, n) != ESP_OK) return ESP_FAIL;
            off += n;
        }
        char inject[72];
        snprintf(inject, sizeof(inject), "<script>window._audioToken='%s';</script>", self->sessionToken);
        if (httpd_resp_send_chunk(r, inject, strlen(inject)) != ESP_OK) return ESP_FAIL;
        const char* rest = pos;
        size_t rlen = strlen(rest);
        for (size_t off = 0; off < rlen; ) {
            size_t n = (rlen - off < 1024) ? (rlen - off) : 1024;
            if (httpd_resp_send_chunk(r, rest + off, n) != ESP_OK) return ESP_FAIL;
            off += n;
        }
        return httpd_resp_send_chunk(r, NULL, 0);
#endif
    }

    // Session token endpoint — JS fetches this to authenticate /audio requests
    static esp_err_t sToken(httpd_req_t* r) {
        auto* self = static_cast<AudioStreamHandler*>(r->user_ctx);
        httpd_resp_set_type(r, "text/plain");
        httpd_resp_set_hdr(r, "Cache-Control", "no-cache, no-store");
        httpd_resp_send(r, self->sessionToken, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    // Blocking audio stream handler — runs in ESP-IDF httpd task.
    // Browser closes the connection to stop streaming (no /stop endpoint needed).
    static esp_err_t sAudio(httpd_req_t* r) {
        auto* self = static_cast<AudioStreamHandler*>(r->user_ctx);
        if (self->streamActive) {
            httpd_resp_set_status(r, "503 Service Unavailable");
            httpd_resp_set_type(r, "text/plain");
            httpd_resp_send(r, "Stream busy", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        self->streamActive = true;
        self->streamStart  = millis();
        // Check session token (injected into /cam page at serve time)
        char qbuf[64] = {};
        if (httpd_req_get_url_query_str(r, qbuf, sizeof(qbuf)) == ESP_OK) {
            char tval[48] = {};
            httpd_query_key_value(qbuf, "t", tval, sizeof(tval));
            if (strcmp(tval, self->sessionToken) != 0) {
                httpd_resp_set_status(r, "401 Unauthorized");
                httpd_resp_set_type(r, "text/plain");
                httpd_resp_send(r, "Unauthorized", HTTPD_RESP_USE_STRLEN);
                return ESP_OK;
            }
        } else {
            httpd_resp_set_status(r, "401 Unauthorized");
            httpd_resp_set_type(r, "text/plain");
            httpd_resp_send(r, "Unauthorized", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
        httpd_resp_set_type(r, "audio/wav");
        httpd_resp_set_hdr(r, "Cache-Control", "no-cache, no-store");
        uint8_t hdr[44];
        writeWavHeader(hdr);
        if (httpd_resp_send_chunk(r, (const char*)hdr, 44) != ESP_OK) {
            self->streamActive = false;
            return ESP_FAIL;
        }
        uint8_t buf[DMA_BUF_LEN * 2];
        size_t bytes = 0;
        while (self->streamActive && millis() - self->streamStart < MAX_STREAM_MS) {
            i2s_read(I2S_PORT, buf, sizeof(buf), &bytes, pdMS_TO_TICKS(50));
            if (bytes > 0) {
                if (httpd_resp_send_chunk(r, (const char*)buf, bytes) != ESP_OK) break;
            }
            taskYIELD();
        }
        httpd_resp_send_chunk(r, NULL, 0);
        self->streamActive = false;
        return ESP_OK;
    }

    // WebSocket endpoint for teleprompter commands (replaces SSE /cmd)
    static esp_err_t sWs(httpd_req_t* r) {
        auto* self = static_cast<AudioStreamHandler*>(r->user_ctx);
        if (r->method == HTTP_GET) {
            self->wsFd = httpd_req_to_sockfd(r);
            return ESP_OK;
        }
        httpd_ws_frame_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        uint8_t buf[16];
        pkt.payload = buf;
        httpd_ws_recv_frame(r, &pkt, sizeof(buf));
        return ESP_OK;
    }

    static esp_err_t sNotFound(httpd_req_t* r, httpd_err_code_t) {
        httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "Not found");
        return ESP_FAIL;
    }

    void regUri(const char* uri, esp_err_t (*handler)(httpd_req_t*)) {
        httpd_uri_t u;
        memset(&u, 0, sizeof(u));
        u.uri      = uri;
        u.method   = HTTP_GET;
        u.handler  = handler;
        u.user_ctx = this;
        httpd_register_uri_handler(httpsServer, &u);
    }

    void startI2S() {
        if (i2sRunning) return;
        M5.Speaker.end();  // Release I2S bus from speaker

        i2s_config_t cfg = {
            .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
            .sample_rate          = SAMPLE_RATE,
            .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format       = I2S_CHANNEL_FMT_ONLY_RIGHT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count        = DMA_BUF_COUNT,
            .dma_buf_len          = DMA_BUF_LEN,
            .use_apll             = false,
            .tx_desc_auto_clear   = false,
            .fixed_mclk           = 0
        };
        i2s_pin_config_t pins = {
            .mck_io_num   = I2S_PIN_NO_CHANGE,
            .bck_io_num   = I2S_PIN_NO_CHANGE,
            .ws_io_num    = MIC_CLK_PIN,
            .data_out_num = I2S_PIN_NO_CHANGE,
            .data_in_num  = MIC_DATA_PIN
        };

        i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);
        i2s_set_pin(I2S_PORT, &pins);
        i2s_zero_dma_buffer(I2S_PORT);
        i2sRunning = true;
    }

    void stopI2S() {
        if (!i2sRunning) return;
        i2s_driver_uninstall(I2S_PORT);
        i2sRunning = false;
    }

public:
    AudioStreamHandler()
        : httpsServer(nullptr),
          i2sRunning(false), serverRunning(false),
          streamActive(false), streamStart(0), wsFd(-1)
    {
        memset(sessionToken, 0, sizeof(sessionToken));
    }

    // Push teleprompter command to browser via WebSocket
    void setTpCmd(uint8_t cmd) {
        if (!httpsServer || wsFd < 0) return;
        const char* msg = (cmd == 1) ? "tp_pause" : "tp_speed";
        httpd_ws_frame_t pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.type = HTTPD_WS_TYPE_TEXT;
        pkt.payload = (uint8_t*)msg;
        pkt.len = strlen(msg);
        if (httpd_ws_send_frame_async(httpsServer, wsFd, &pkt) != ESP_OK) {
            wsFd = -1;
        }
    }

    ~AudioStreamHandler() {
        stop();
    }

    // Returns true if server started successfully
    bool start() {
        if (!WiFiHelper::isConnected()) return false;
        if (serverRunning) return true;

        // Generate a fresh random session token on each start
        for (int i = 0; i < 32; i++) {
            sessionToken[i] = "0123456789abcdef"[esp_random() & 0xF];
        }
        sessionToken[32] = '\0';

        startI2S();

        // HTTPS server on port 443 via ESP-IDF native TLS (mbedTLS)
        httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
        conf.cacert_pem          = (const uint8_t*)TLS_CERT;
        conf.cacert_len          = strlen(TLS_CERT) + 1;
        conf.prvtkey_pem         = (const uint8_t*)TLS_KEY;
        conf.prvtkey_len         = strlen(TLS_KEY) + 1;
        conf.httpd.stack_size        = 16384;
        conf.httpd.max_uri_handlers  = 7;
        conf.httpd.send_wait_timeout = 30;
        if (httpd_ssl_start(&httpsServer, &conf) != ESP_OK) return false;

        regUri("/",              sRoot);
        regUri("/cam",           sCam);
        regUri("/audio",         sAudio);
        regUri("/token",         sToken);
        { // WebSocket URI (needs is_websocket flag)
            httpd_uri_t ws = {};
            ws.uri = "/ws";
            ws.method = HTTP_GET;
            ws.handler = sWs;
            ws.user_ctx = this;
            ws.is_websocket = true;
            httpd_register_uri_handler(httpsServer, &ws);
        }
        httpd_register_err_handler(httpsServer, HTTPD_404_NOT_FOUND, sNotFound);

        serverRunning = true;
        return true;
    }

    void stop() {
        streamActive = false;
        wsFd = -1;
        if (httpsServer) {
            httpd_ssl_stop(httpsServer);
            httpsServer = nullptr;
        }
        stopI2S();
        serverRunning = false;
    }

    bool isRunning() { return serverRunning; }

    String getURL() {
        if (!serverRunning || !WiFiHelper::isConnected()) return "";
        return "https://" + WiFi.localIP().toString() + "/cam";
    }
};

#endif
