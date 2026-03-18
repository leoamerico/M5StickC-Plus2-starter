# M5StickC Plus2 — Starter Kit Completo

Um ponto de partida completo para o **M5StickC Plus2** com sistema de menus, relógio RTC, gerenciamento de bateria, deep sleep, streaming de áudio via WiFi e gravação de vídeo com microfone lavalier via iPhone.

```
Dispositivo : M5StickC Plus2 (ESP32-PICO-V3-02)
Framework   : Arduino + PlatformIO
Linguagem   : C++17
Testes E2E  : Playwright (Node.js)
```

---

## ✨ Funcionalidades

| Recurso | Descrição |
|---------|-----------|
| ��� **Relógio** | Hora real via RTC BM8563, formato 12h/24h, data DD/MM/YYYY |
| ��� **Bateria** | Nível em % na tela, deep sleep automático configurável |
| ��� **Menu** | Sistema de menus empilháveis, navegação por botões |
| ⚙️ **Settings** | Persistência em NVS, som de UI, delay de sleep |
| ��� **Set Date/Time** | Seletor visual de hora, data e ano |
| ��� **Audio Stream** | Microfone SPM1423 → iPhone via WiFi (WAV chunked) |
| ��� **M5 Cam** | Página iOS-style: câmera iPhone + mic ESP32 → gravação MP4 |
| ��� **HTTPS** | Servidor TLS nativo (mbedTLS) — obrigatório para `getUserMedia` no Safari |
| ��� **PWA** | Manifest + ícone SVG → "Add to Home Screen" no iPhone |
| ��� **Deep Sleep** | Pomodoro configura timer de despertar via RTC |
| ℹ️ **Device Info** | Viewer de 12 cards: CPU, memória, WiFi, bateria, firmware |
| ��� **Restart** | Reinício via menu com confirmação na tela |

---

## ��� Como usar

### Pré-requisitos

- **M5StickC Plus2**
- **VSCode** + extensão **PlatformIO**
- Node.js 18+ (apenas para testes Playwright, opcional)

### Upload do firmware

```bash
git clone https://github.com/leoamerico/M5StickC-Plus2-starter.git
cd M5StickC-Plus2-starter

# Abre no VSCode
code .

# No PlatformIO: clique em "Upload" ou pressione Ctrl+Alt+U
```

### Configurar WiFi (via Serial)

Abra o Monitor Serial (115200 baud) e envie:

```
WIFI:SuaRede:SuaSenha
```

As credenciais são salvas em NVS e persistem após reset.

---

## ��� M5 Cam — Gravar vídeo com microfone lavalier

O M5StickC Plus2 funciona como **microfone lavalier sem fio** para o iPhone:

1. Conecte o M5Stick ao WiFi
2. Pressione **Botão A** → **Audio Stream** no menu
3. O display mostra `https://IP/cam`
4. Abra o link no **Safari do iPhone**

> **Primeira vez**: Safari avisa sobre certificado auto-assinado → toque em *"Visitar Site"* →  
> vá em **Ajustes → Geral → Sobre → Definições de Certificado → ative "m5cam.local"**.  
> Na próxima visita o `getUserMedia` funciona normalmente.

5. Toque em **"Start Camera & Mic"**
6. Câmera traseira do iPhone + microfone ESP32 sincronizados
7. Toque no botão de gravação vermelho
8. Ao parar: arquivo MP4 disponível para download

### Como funciona internamente

```
ESP32 SPM1423 PDM mic
        ↓  I2S @ 16kHz 16-bit
   ESP-IDF HTTPS server (mbedTLS, porta 443)
        ↓  WAV chunked Transfer-Encoding
   Safari Web Audio API (AudioContext 16kHz)
        ↓  BufferSource → AnalyserNode
   MediaStreamDestination ← mistura com câmera iPhone
        ↓
   MediaRecorder → Blob → download .mp4
```

---

## ���️ Estrutura do projeto

```
src/
  main.cpp                  ← loop principal, SerialCommands

lib/
  audio_stream_handler.h    ← I2S + HTTPS server + página web completa
  battery_handler.h         ← leitura % bateria
  button_handler.h          ← debounce nos 3 botões
  clock_handler.h           ← relógio, Pomodoro, sleep timer
  device_info_handler.h     ← viewer de 12 cards de hardware
  display_handler.h         ← wrapper M5GFX com mensagens padronizadas
  menu_handler.h            ← itens de menu com scroll
  menu_manager.h            ← pilha de menus (push/pop)
  mqtt_helper.h             ← cliente MQTT (opcional)
  page_manager.h            ← navegação entre páginas
  rtc_utils.h               ← leitura/escrita RTC BM8563
  settings_manager.h        ← persistência NVS + deep sleep safeguard
  time_selector.h           ← seletor visual HH:MM / DD/MM/YYYY
  wifi_helper.h             ← connect com timeout
  pages/
    clock_page.h            ← tela principal com menu completo
    audio_stream_page.h     ← tela de status do streaming
    page_base.h             ← classe base com navegação de menus

e2e/                        ← testes Playwright (browser visível)
  server.js                 ← extrai HTML do firmware e serve localmente
  playwright.config.js      ← headed + slowMo 800ms + iPhone viewport
  tests/
    cam-page.spec.js        ← 17 testes em 5 blocos didáticos
```

---

## ��� Testes E2E com Playwright

Os testes rodam com o **browser visível** e extraem o HTML diretamente do firmware — sem precisar do dispositivo físico.

```bash
cd e2e
npm install
npx playwright install chromium

npm test               # headed + slowMo (modo aprendizado — browser visível)
npm run test:ui        # modo interativo — melhor para explorar
npm run test:headless  # rápido, sem browser (CI)
npm run show-report    # relatório HTML com steps e screenshots
```

### Conceitos cobertos nos testes

| Bloco | Conceitos Playwright |
|-------|---------------------|
| 1 — Estrutura | `page.goto`, `getByRole`, `locator`, `toBeVisible/Hidden` |
| 2 — Rede | `page.route` (spy, mock, abort), `page.on('request')` |
| 3 — localStorage | `page.evaluate`, estado do browser, `waitForTimeout` |
| 4 — Fluxo câmera | `test.step`, `beforeEach`, fake media device |
| 5 — Init Script | `addInitScript`, mockar `getUserMedia` |

---

## ��� Navegação nos botões

| Botão | Fora do menu | No menu |
|-------|-------------|---------|
| **PWR** (lateral) | — | Sobe na lista |
| **A** (frontal grande) | Abre menu | Seleciona item |
| **B** (frontal pequeno) | Volta à tela anterior | Desce na lista |

---

## ��� Dependências PlatformIO

```ini
m5stack/M5Unified @ ^0.2.10
m5stack/M5GFX @ ^0.2.16
marvinroger/AsyncMqttClient @ ^0.8.1
bblanchon/ArduinoJson @ ^7.4.2
mathieucarbou/ESPAsyncWebServer @ ^3.6.0
mathieucarbou/AsyncTCP @ ^3.3.2
```

O servidor HTTPS usa **`esp_https_server.h`** nativo do ESP-IDF (mbedTLS integrado) — sem libs extras para TLS.

---

## ��� Licença

MIT — use, modifique e distribua livremente.
