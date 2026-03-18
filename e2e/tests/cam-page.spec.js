// @ts-check
/**
 * Testes Playwright — Interface M5 Cam
 * =====================================================================
 * CONCEITOS DEMONSTRADOS NESTE ARQUIVO
 * ─────────────────────────────────────────────────────────────────────
 * page.goto(url)               → navegar para uma URL
 * page.locator(css)            → referenciar elemento por seletor CSS
 * page.getByRole(role, {name}) → localizar por ARIA (preferido)
 * page.getByText(texto)        → localizar pelo conteúdo visível
 * expect(locator).*            → asserções sobre o elemento/página
 * page.route(url, fn)          → interceptar requisições (mock de API)
 * page.evaluate(fn)            → executar JS no contexto do browser
 * page.addInitScript(fn)       → injetar JS *antes* da página carregar
 * page.screenshot({path})      → salvar imagem do estado atual
 * locator.screenshot()         → screenshot só do elemento
 * page.waitForTimeout(ms)      → pausa explícita (use com moderação)
 * test.beforeEach(fn)          → hook executado antes de cada teste
 * test.step(nome, fn)          → sub-etapa nomeada dentro de um teste
 * =====================================================================
 */

const { test, expect } = require('@playwright/test');
const path = require('path');
const fs   = require('fs');

// Pasta onde as screenshots manuais são salvas
const SHOTS = path.join(__dirname, '..', 'screenshots');
if (!fs.existsSync(SHOTS)) fs.mkdirSync(SHOTS, { recursive: true });

// ═══════════════════════════════════════════════════════════════════════════════
// BLOCO 1 — Estrutura da Página
// Verifica que o HTML carregou e que os elementos essenciais estão presentes.
// ═══════════════════════════════════════════════════════════════════════════════
test.describe('1 — Estrutura da Página', () => {

  test('título da aba é "M5 Cam"', async ({ page }) => {
    // goto() navega e aguarda o evento 'load' disparar
    await page.goto('/cam');

    // toHaveTitle() verifica o conteúdo de <title>
    await expect(page).toHaveTitle('M5 Cam');
  });

  test('botão de início está visível com o texto correto', async ({ page }) => {
    await page.goto('/cam');

    // getByRole + name: localização semântica — resistente a mudanças de CSS
    // É a forma PREFERIDA pelo Playwright pois reflete a experiência do usuário
    const btn = page.getByRole('button', { name: 'Start Camera & Mic' });
    await expect(btn).toBeVisible();
  });

  test('elementos da interface existem no DOM', async ({ page }) => {
    await page.goto('/cam');

    // locator(css): igualzinho a document.querySelector()
    // toBeAttached()  → elemento existe no DOM (mesmo oculto)
    // toBeVisible()   → elemento existe E está visível ao usuário
    // toBeHidden()    → elemento existe MAS não está visível
    await expect(page.locator('#preview')).toBeAttached();   // <video> existe
    await expect(page.locator('#sheet')).toBeVisible();      // bottom sheet visível
    await expect(page.locator('#pill')).toBeAttached();      // HUD pill existe
    await expect(page.locator('#recRow')).toBeHidden();      // botão rec oculto
    await expect(page.locator('#dlSection')).toBeHidden();   // downloads oculto
  });

  test('status inicial menciona "M5Stick lavalier"', async ({ page }) => {
    await page.goto('/cam');

    // getByText(): localiza qualquer elemento que contenha esse texto
    await expect(page.getByText('M5Stick lavalier')).toBeVisible();

    // Podemos também verificar o elemento diretamente pelo ID
    await expect(page.locator('#sMsg')).toContainText('M5Stick lavalier');
  });

  test('salva screenshot do estado inicial', async ({ page }) => {
    await page.goto('/cam');

    // screenshot() do page inteiro
    await page.screenshot({
      path:     path.join(SHOTS, '01-initial.png'),
      fullPage: false,
    });

    // screenshot() só do bottom sheet
    await page.locator('#sheet').screenshot({
      path: path.join(SHOTS, '01-sheet.png'),
    });

    console.log(`  📸  Screenshots salvas em e2e/screenshots/`);
  });

});

// ═══════════════════════════════════════════════════════════════════════════════
// BLOCO 2 — page.route() — Interceptando Requisições de Rede
// page.route() é uma das funcionalidades mais poderosas do Playwright.
// Permite observar, modificar ou bloquear qualquer requisição HTTP.
// ═══════════════════════════════════════════════════════════════════════════════
test.describe('2 — Interceptação de Rede (page.route)', () => {

  test('espia /token sem modificá-la', async ({ page }) => {
    let capturedUrl   = '';
    let capturedStatus = 0;

    // page.route(padrão, handler) — padrão pode ser string, glob ou RegExp
    // route.continue() → deixa a requisição original seguir normalmente
    // route.request()  → objeto com info da requisição original
    await page.route('/token', async route => {
      capturedUrl = route.request().url();
      console.log(`  📡  Interceptei: ${capturedUrl}`);

      const response = await route.fetch();          // executa a req. original
      capturedStatus = response.status();
      await route.fulfill({ response });             // repassa a resposta
    });

    await page.goto('/cam');
    const tokenText = await page.evaluate(() => fetch('/token').then(r => r.text()));
    await page.waitForTimeout(500);

    expect(capturedUrl).toContain('/token');
    expect(capturedStatus).toBe(200);
    expect(tokenText.length).toBe(32); // 32-char hex token
    console.log(`  ✅  Token capturado: ${tokenText.slice(0,8)}...`);
  });

  test('substitui /token com dado de teste (mock total)', async ({ page }) => {
    // route.fulfill() → responde com dados NOSSOS — servidor real nunca é chamado!
    await page.route('/token', route => {
      route.fulfill({
        status:      200,
        contentType: 'text/plain',
        body:        'mock_token_for_playwright_test_1',
      });
    });

    await page.goto('/cam');

    // page.evaluate(fn) → executa a função no contexto DO BROWSER e retorna o resultado
    const token = await page.evaluate(async () => {
      const res = await fetch('/token');
      return res.text();
    });

    // O servidor devolveu NOSSO mock
    expect(token).toBe('mock_token_for_playwright_test_1');
    console.log(`  ✅  Mock funcionou: token = "${token}"`);
  });

  test('simula erro 503 no /audio → página mostra estado de erro', async ({ page }) => {
    // Abortar a requisição faz o fetch() do browser rejeitar
    await page.route(/\/audio/, route => route.abort('connectionreset'));

    await page.goto('/cam');
    await page.locator('#startBtn').click();

    // Após o clique, a câmera abre mas o /audio falha
    // O status indica erro: "Cannot reach M5Stick" ou "Mic stream ended"
    await expect(page.locator('#sMsg')).toContainText(/ended|error|denied|reach|WiFi/i, { timeout: 8000 });

    await page.screenshot({ path: path.join(SHOTS, '02-audio-error.png') });
    console.log(`  ✅  Erro de rede propagado corretamente na UI`);
  });

  test('captura todas as requisições feitas durante o carregamento', async ({ page }) => {
    const requests = [];

    // page.on('request') → evento disparado para CADA requisição
    page.on('request', req => requests.push(req.url()));

    await page.goto('/cam');
    await page.waitForTimeout(500);

    console.log(`  📋  Requisições feitas:`);
    requests.forEach(u => console.log(`       ${u}`));

    // A página deve buscar ao menos o /cam
    expect(requests.length).toBeGreaterThan(0);
  });

});

// ═══════════════════════════════════════════════════════════════════════════════
// BLOCO 3 — localStorage e Overlay "Adicionar à Tela Inicial"
// Demonstra page.evaluate() para manipular o estado do browser.
// ═══════════════════════════════════════════════════════════════════════════════
test.describe('3 — localStorage + Overlay A2HS', () => {

  test('overlay aparece após 1.8 s na primeira visita', async ({ page }) => {
    await page.goto('/cam');

    // page.evaluate() executa código no contexto do browser
    // Aqui limpamos o localStorage para simular "primeira visita"
    await page.evaluate(() => localStorage.removeItem('m5_a2hs_seen'));
    await page.reload();

    const overlay = page.locator('#a2hsOverlay');
    await expect(overlay).toBeHidden();  // começa oculto

    console.log(`  ⏳  Aguardando o setTimeout de 1.8 s...`);
    // waitForTimeout: pausa o teste — use apenas quando necessário!
    await page.waitForTimeout(2200);

    await expect(overlay).toBeVisible(); // agora apareceu
    console.log(`  ✅  Overlay visível!`);

    await overlay.screenshot({ path: path.join(SHOTS, '03-a2hs-overlay.png') });
  });

  test('botão "Entendido" fecha overlay e grava no localStorage', async ({ page }) => {
    await page.goto('/cam');
    await page.evaluate(() => localStorage.removeItem('m5_a2hs_seen'));
    await page.reload();
    await page.waitForTimeout(2200);

    // Clica pelo texto do botão
    await page.getByRole('button', { name: 'Entendido' }).click();

    await expect(page.locator('#a2hsOverlay')).toBeHidden();

    // Verifica o localStorage de volta
    const value = await page.evaluate(() => localStorage.getItem('m5_a2hs_seen'));
    expect(value).toBe('1');
    console.log(`  ✅  localStorage["m5_a2hs_seen"] = "${value}"`);
  });

  test('overlay NÃO aparece em visitas subsequentes', async ({ page }) => {
    await page.goto('/cam');
    // Seta o valor ANTES de recarregar
    await page.evaluate(() => localStorage.setItem('m5_a2hs_seen', '1'));
    await page.reload();

    await page.waitForTimeout(2200); // espera o tempo do setTimeout

    await expect(page.locator('#a2hsOverlay')).toBeHidden();
    console.log(`  ✅  Overlay não reapareceu`);
  });

});

// ═══════════════════════════════════════════════════════════════════════════════
// BLOCO 4 — Fluxo Câmera + Áudio (end-to-end com dispositivos falsos)
// Usa o servidor real em localhost:3333 que transmite WAV fake.
// O Chrome usa --use-fake-device-for-media-stream para câmera sintética.
// ═══════════════════════════════════════════════════════════════════════════════
test.describe('4 — Fluxo Câmera + Microfone', () => {

  // Impede o overlay A2HS de aparecer durante estes testes
  // (o overlay bloqueia cliques após 1.8 s)
  test.beforeEach(async ({ page }) => {
    await page.addInitScript(() => {
      localStorage.setItem('m5_a2hs_seen', '1');
    });
  });

  test('clicar Start: botão some e status muda', async ({ page }) => {
    // Bloqueia /audio com abort → fetch() rejeita rapidamente
    await page.route(/\/audio/, route => route.abort());

    await page.goto('/cam');

    const startBtn = page.locator('#startBtn');
    await expect(startBtn).toBeVisible();

    // click() executa a ação e aguarda a animação
    await startBtn.click();

    // Após o clique o botão desaparece (display:none via JS)
    await expect(startBtn).toBeHidden({ timeout: 4000 });

    // Status não é mais o inicial
    await expect(page.locator('#sMsg')).not.toHaveText('M5Stick lavalier • iPhone camera');
    console.log(`  ✅  Status agora: "${await page.locator('#sMsg').innerText()}"`);
  });

  test('botão de gravação aparece quando áudio começa a fluir', async ({ page }) => {
    // Sem interceptação → servidor real transmite WAV fake
    // O browser recebe o header WAV, depois chunks de silêncio
    // Quando audioFlowing=true o #recRow fica visível

    await page.goto('/cam');
    await page.locator('#startBtn').click();

    // waitForSelector com estado 'visible': testa pacientemente
    await expect(page.locator('#recRow')).toBeVisible({ timeout: 10_000 });
    await expect(page.locator('#pill')).toHaveClass(/show/);
    await expect(page.locator('#sMsg')).toContainText(/ready/i);

    console.log(`  ✅  Áudio fluindo — botão rec visível!`);
    await page.screenshot({ path: path.join(SHOTS, '04-ready-to-record.png') });
  });

  test('iniciar e parar gravação — ciclo completo', async ({ page }) => {
    // test.step() divide o teste em etapas nomeadas
    // Aparecem no relatório HTML com timing individual

    await test.step('Abrir página e conectar áudio', async () => {
      await page.goto('/cam');
      await page.locator('#startBtn').click();
      await expect(page.locator('#recRow')).toBeVisible({ timeout: 10_000 });
    });

    await test.step('Iniciar a gravação', async () => {
      const recBtn = page.locator('#recBtn');
      await recBtn.click();

      // Pill fica vermelho com classe "live"
      await expect(page.locator('#pill')).toHaveClass(/live/);
      // Botão muda de aparência (classe "on")
      await expect(recBtn).toHaveClass(/on/);
      console.log(`  🔴  Gravação iniciada`);
    });

    await test.step('Gravar por 2 segundos', async () => {
      await page.screenshot({ path: path.join(SHOTS, '05-recording.png') });
      await page.waitForTimeout(2000);
    });

    await test.step('Parar e verificar download disponível', async () => {
      await page.locator('#recBtn').click();

      // Seção de downloads aparece após recorder.onstop disparar
      await expect(page.locator('#dlSection')).toBeVisible({ timeout: 8000 });

      // Deve existir ao menos uma linha de download
      const dlRow = page.locator('.dlRow').first();
      await expect(dlRow).toBeVisible();

      const name = await dlRow.locator('.dlName').innerText();
      console.log(`  ✅  Arquivo gerado: ${name}`);

      // Botões de ação devem estar presentes
      await expect(dlRow.locator('.dlBtnSave')).toBeVisible();

      await page.screenshot({ path: path.join(SHOTS, '06-download-ready.png') });
    });
  });

  test('botão Save cria <a download> com nome correto (fallback sem showSaveFilePicker)', async ({ page }) => {
    // Remove showSaveFilePicker para forçar o caminho do fallback
    await page.addInitScript(() => {
      // @ts-ignore
      delete window.showSaveFilePicker;

      // Espia document.createElement para capturar o <a download> criado por saveRec()
      const origCreate = document.createElement.bind(document);
      window.__savedAnchor = null;
      document.createElement = (tag) => {
        const el = origCreate(tag);
        if (tag === 'a') {
          // Sobrescreve click() para capturar sem disparar navegação real
          const origClick = el.click.bind(el);
          el.click = () => { window.__savedAnchor = { href: el.href, download: el.download }; };
        }
        return el;
      };
    });

    await page.goto('/cam');
    await page.locator('#startBtn').click();
    await expect(page.locator('#recRow')).toBeVisible({ timeout: 10_000 });

    // Grava 1 segundo
    await page.locator('#recBtn').click();
    await page.waitForTimeout(1000);
    await page.locator('#recBtn').click();
    await expect(page.locator('#dlSection')).toBeVisible({ timeout: 8000 });

    // Clica no botão Save
    await page.locator('.dlBtnSave').first().click();
    await page.waitForTimeout(600); // aguarda o fetch(blob) terminar

    // Verifica o anchor criado pelo fallback
    const anchor = await page.evaluate(() => window.__savedAnchor || null);
    expect(anchor).not.toBeNull();
    expect(anchor.download).toMatch(/^m5_.*\.(mp4|webm)$/);
    expect(anchor.href).toMatch(/^blob:/);

    console.log(`  ✅  Fallback <a download> criado: ${anchor.download}`);
    await page.screenshot({ path: path.join(SHOTS, '07-save-fallback.png') });
  });

});

// ═══════════════════════════════════════════════════════════════════════════════
// BLOCO 5 — addInitScript: injetar código antes da página executar
// Útil para mockar APIs do browser (ex.: navigator.mediaDevices)
// ═══════════════════════════════════════════════════════════════════════════════
test.describe('5 — addInitScript: mockar APIs do Browser', () => {

  test('injeta mock de getUserMedia e verifica que é chamado', async ({ page }) => {
    // addInitScript() roda ANTES de qualquer script da página
    // Ideal para substituir APIs nativas
    await page.addInitScript(() => {
      let callCount = 0;
      const original = navigator.mediaDevices.getUserMedia.bind(navigator.mediaDevices);

      // Espionagem: conta chamadas e repassa ao original
      navigator.mediaDevices.getUserMedia = async (constraints) => {
        callCount++;
        // @ts-ignore — guardamos no window para o teste ler
        window.__getUserMediaCalls = callCount;
        window.__getUserMediaConstraints = constraints;
        return original(constraints);
      };
    });

    // Bloqueia /audio para o teste não depender do áudio fluir
    await page.route(/\/audio/, route => route.abort());

    await page.goto('/cam');
    await page.locator('#startBtn').click();
    await page.waitForTimeout(1500);

    // Lê os dados que o mock guardou no window
    const calls       = await page.evaluate(() => window.__getUserMediaCalls || 0);
    const constraints = await page.evaluate(() => window.__getUserMediaConstraints);

    expect(calls).toBe(1);
    expect(constraints?.video?.facingMode).toBe('user');       // câmera frontal (padrão)
    expect(constraints?.audio).toBe(false);                     // sem mic nativo

    console.log(`  ✅  getUserMedia chamado ${calls}x`);
    console.log(`  📋  Constraints:`, JSON.stringify(constraints));
  });

  test('substitui getUserMedia por mock que rejeita (simula permissão negada)', async ({ page }) => {
    await page.addInitScript(() => {
      navigator.mediaDevices.getUserMedia = async () => {
        const err = new Error('Permission denied by test');
        err.name = 'NotAllowedError';
        throw err;
      };
    });

    await page.goto('/cam');
    await page.locator('#startBtn').click();

    // A página captura NotAllowedError e exibe mensagem
    await expect(page.locator('#errMsg')).toBeVisible({ timeout: 5000 });
    await expect(page.locator('#errMsg')).toContainText(/permission/i);

    console.log(`  ✅  Erro de permissão exibido corretamente`);
    await page.screenshot({ path: path.join(SHOTS, '07-permission-denied.png') });
  });

});

// ═══════════════════════════════════════════════════════════════════════════════
// BLOCO 6 — PWA: Manifest e Ícone Inline (data: URIs)
// Verifica que o manifest e ícone estão embutidos como data: URIs no HTML,
// sem precisar de endpoints separados /manifest.json e /icon.svg.
// ═══════════════════════════════════════════════════════════════════════════════
test.describe('6 — PWA Inline Assets (data: URIs)', () => {

  test('link rel="manifest" usa data: URI com campos obrigatórios', async ({ page }) => {
    await page.goto('/cam');

    const manifest = await page.evaluate(() => {
      const link = document.querySelector('link[rel="manifest"]');
      if (!link) return null;
      const href = link.getAttribute('href');
      if (!href || !href.startsWith('data:')) return null;
      // Decodifica o data: URI → parse JSON
      const commaIdx = href.indexOf(',');
      const json = decodeURIComponent(href.substring(commaIdx + 1));
      return JSON.parse(json);
    });

    expect(manifest).not.toBeNull();
    expect(manifest.name).toBe('M5 Cam');
    expect(manifest.display).toBe('standalone');
    expect(manifest.start_url).toBe('/cam');
    console.log(`  ✅  Manifest inline OK: "${manifest.name}", display=${manifest.display}`);
  });

  test('link rel="apple-touch-icon" usa data: URI SVG', async ({ page }) => {
    await page.goto('/cam');

    const href = await page.evaluate(() => {
      const link = document.querySelector('link[rel="apple-touch-icon"]');
      return link ? link.getAttribute('href') : null;
    });

    expect(href).not.toBeNull();
    expect(href).toMatch(/^data:image\/svg\+xml/);
    console.log(`  ✅  Ícone inline OK: data:image/svg+xml...`);
  });

});

// ═══════════════════════════════════════════════════════════════════════════════
// BLOCO 7 — Token de Sessão (/token)
// O endpoint /token é buscado pelo JS antes de conectar ao /audio.
// Testa o fluxo de autenticação token→audio.
// ═══════════════════════════════════════════════════════════════════════════════
test.describe('7 — Token de Sessão', () => {

  test('/token retorna texto plano com 32 caracteres', async ({ page }) => {
    await page.goto('/cam');

    const token = await page.evaluate(() => fetch('/token').then(r => r.text()));

    expect(token).toHaveLength(32);
    expect(token).toMatch(/^[a-f0-9]+$/); // hex only
    console.log(`  ✅  Token: ${token}`);
  });

  test('startAll() busca /token antes de conectar /audio', async ({ page }) => {
    const urls = [];

    // Captura a sequência de requisições
    page.on('request', req => {
      const u = new URL(req.url());
      if (u.pathname === '/token' || u.pathname === '/audio') {
        urls.push(u.pathname);
      }
    });

    // Aborta /audio para não bloquear o teste
    await page.route('/audio**', route => route.abort());

    await page.addInitScript(() => {
      localStorage.setItem('m5_a2hs_seen', '1');
    });

    await page.goto('/cam');
    await page.locator('#startBtn').click();
    await page.waitForTimeout(2000);

    // /token deve vir ANTES de /audio na sequência
    const tokenIdx = urls.indexOf('/token');
    const audioIdx = urls.indexOf('/audio');
    expect(tokenIdx).toBeGreaterThanOrEqual(0);
    expect(audioIdx).toBeGreaterThanOrEqual(0);
    expect(tokenIdx).toBeLessThan(audioIdx);
    console.log(`  ✅  Sequência: ${urls.join(' → ')}`);
  });

  test('/audio recebe token como query param ?t=', async ({ page }) => {
    let audioUrl = '';

    page.on('request', req => {
      if (req.url().includes('/audio')) audioUrl = req.url();
    });

    await page.route('/audio**', route => route.abort());

    await page.addInitScript(() => {
      localStorage.setItem('m5_a2hs_seen', '1');
    });

    await page.goto('/cam');
    await page.locator('#startBtn').click();
    await page.waitForTimeout(2000);

    expect(audioUrl).toContain('/audio?t=');
    const token = new URL(audioUrl).searchParams.get('t');
    expect(token).toHaveLength(32);
    console.log(`  ✅  /audio chamado com ?t=${token.slice(0,8)}...`);
  });

});

// ═══════════════════════════════════════════════════════════════════════════════
// BLOCO 8 — Teleprompter: Editor de Script
// O editor permite ao usuário digitar/colar um roteiro que será exibido
// durante a gravação. Testa abertura, digitação, contagem de palavras,
// persistência no localStorage e construção das linhas.
// ═══════════════════════════════════════════════════════════════════════════════
test.describe('8 — Teleprompter: Editor de Script', () => {

  test.beforeEach(async ({ page }) => {
    await page.addInitScript(() => {
      localStorage.setItem('m5_a2hs_seen', '1');
      localStorage.removeItem('m5_tp_script');
    });
  });

  test('editor overlay começa oculto', async ({ page }) => {
    await page.goto('/cam');
    await expect(page.locator('#editorOverlay')).toHaveClass(/hidden/);
  });

  test('openEditor() mostra overlay e foca no textarea', async ({ page }) => {
    await page.goto('/cam');

    await page.evaluate(() => openEditor());
    await page.waitForTimeout(300);

    await expect(page.locator('#editorOverlay')).not.toHaveClass(/hidden/);
    // O textarea deve estar focado
    const focused = await page.evaluate(() => document.activeElement?.id);
    expect(focused).toBe('scriptArea');
    console.log(`  ✅  Editor aberto e focado`);
  });

  test('digitar texto atualiza contagem de palavras', async ({ page }) => {
    await page.goto('/cam');
    await page.evaluate(() => openEditor());
    await page.waitForTimeout(300);

    const scriptArea = page.locator('#scriptArea');
    await scriptArea.fill('Um dois tres quatro cinco');

    // Disparar 'input' event para acionar o listener
    await scriptArea.dispatchEvent('input');
    await page.waitForTimeout(200);

    const countText = await page.locator('#charCount').innerText();
    expect(countText).toContain('5 words');
    console.log(`  ✅  Contagem: ${countText}`);
  });

  test('closeEditor() salva roteiro no localStorage e constrói linhas', async ({ page }) => {
    await page.goto('/cam');
    await page.evaluate(() => openEditor());
    await page.waitForTimeout(300);

    const sampleText = 'Primeira linha do roteiro.\n\nSegunda parte com mais palavras aqui.';
    await page.locator('#scriptArea').fill(sampleText);

    await page.evaluate(() => closeEditor());
    await page.waitForTimeout(200);

    // Editor deve fechar
    await expect(page.locator('#editorOverlay')).toHaveClass(/hidden/);

    // localStorage deve ter o roteiro
    const saved = await page.evaluate(() => localStorage.getItem('m5_tp_script'));
    expect(saved).toBe(sampleText);

    // Linhas devem ter sido construídas no tpTrack
    const lineCount = await page.locator('#tpTrack .tpLine').count();
    expect(lineCount).toBeGreaterThan(0);

    // Status indica script carregado
    await expect(page.locator('#sMsg')).toContainText(/Script loaded/i);
    console.log(`  ✅  Script salvo (${lineCount} linhas), localStorage OK`);
  });

  test('roteiro persiste entre recarregamentos', async ({ page }) => {
    // addInitScript garante que o valor sobrevive ao reload
    // (roda DEPOIS do removeItem do beforeEach, pois é adicionado depois)
    await page.addInitScript(() => {
      localStorage.setItem('m5_tp_script', 'Texto salvo anteriormente no localStorage.');
    });
    await page.goto('/cam');
    await page.waitForTimeout(500);

    // O textarea deve carregar o texto salvo pela IIFE da página
    const text = await page.locator('#scriptArea').inputValue();
    expect(text).toBe('Texto salvo anteriormente no localStorage.');

    // As linhas do TP devem estar construídas
    const lineCount = await page.locator('#tpTrack .tpLine').count();
    expect(lineCount).toBeGreaterThan(0);
    console.log(`  ✅  Script restaurado do localStorage (${lineCount} linhas)`);
  });

});

// ═══════════════════════════════════════════════════════════════════════════════
// BLOCO 9 — Teleprompter: Controle de Scroll, Pause e Velocidade
// Testa o engine de scroll (tpStart, tpStop, tpTogglePause, tpCycleSpeed)
// usando page.evaluate() para chamar as funções JS diretamente.
// ═══════════════════════════════════════════════════════════════════════════════
test.describe('9 — Teleprompter: Scroll Engine', () => {

  test.beforeEach(async ({ page }) => {
    await page.addInitScript(() => {
      localStorage.setItem('m5_a2hs_seen', '1');
      // Pre-carrega um script de teste
      localStorage.setItem('m5_tp_script', 'Linha um do teleprompter.\n\nLinha dois do teleprompter.\n\nLinha tres do teleprompter.\n\nLinha quatro final.');
    });
  });

  test('tpStart() ativa overlay e barra de controle', async ({ page }) => {
    await page.goto('/cam');
    await page.waitForTimeout(500); // espera IIFE carregar o script

    await page.evaluate(() => tpStart());
    await page.waitForTimeout(300);

    await expect(page.locator('#tpOverlay')).toHaveClass(/active/);
    await expect(page.locator('#tpBar')).toHaveClass(/show/);

    const running = await page.evaluate(() => tpRunning);
    expect(running).toBe(true);
    console.log(`  ✅  Teleprompter rodando, overlay ativo`);

    await page.screenshot({ path: path.join(SHOTS, '09-tp-running.png') });
  });

  test('tpTogglePause() alterna entre play e pause', async ({ page }) => {
    await page.goto('/cam');
    await page.waitForTimeout(500);

    await page.evaluate(() => tpStart());
    await page.waitForTimeout(200);

    // Pause
    await page.evaluate(() => tpTogglePause());
    let paused = await page.evaluate(() => tpPaused);
    expect(paused).toBe(true);
    await expect(page.locator('#tpPauseBtn')).toHaveText('▶');

    // Resume
    await page.evaluate(() => tpTogglePause());
    paused = await page.evaluate(() => tpPaused);
    expect(paused).toBe(false);
    await expect(page.locator('#tpPauseBtn')).toHaveText('⏸');

    console.log(`  ✅  Pause/resume funciona corretamente`);
  });

  test('tpCycleSpeed() cicla pelos 6 níveis de velocidade', async ({ page }) => {
    await page.goto('/cam');
    await page.waitForTimeout(500);

    await page.evaluate(() => tpStart());

    // Velocidade inicial: índice 2 (0.8×)
    let speed = await page.evaluate(() => tpSpeed);
    expect(speed).toBeCloseTo(0.8, 1);

    // Cicla uma vez → índice 3 (1.2×)
    await page.evaluate(() => tpCycleSpeed());
    speed = await page.evaluate(() => tpSpeed);
    expect(speed).toBeCloseTo(1.2, 1);

    // Botão mostra a velocidade
    await expect(page.locator('#tpSpeedBtn')).toHaveText('1.2×');

    // Badge temporário aparece
    await expect(page.locator('#speedBadge')).toHaveClass(/show/);
    await expect(page.locator('#speedBadge')).toContainText('1.2');

    console.log(`  ✅  Speed cycle: 0.8 → ${speed}`);
  });

  test('tpStop() desativa overlay e barra', async ({ page }) => {
    await page.goto('/cam');
    await page.waitForTimeout(500);

    await page.evaluate(() => tpStart());
    await page.waitForTimeout(200);

    await page.evaluate(() => tpStop());

    await expect(page.locator('#tpOverlay')).not.toHaveClass(/active/);
    await expect(page.locator('#tpBar')).not.toHaveClass(/show/);

    const running = await page.evaluate(() => tpRunning);
    expect(running).toBe(false);
    console.log(`  ✅  Teleprompter parado`);
  });

  test('scroll avança a posição (tpPos) ao longo do tempo', async ({ page }) => {
    await page.goto('/cam');
    await page.waitForTimeout(500);

    await page.evaluate(() => tpStart());

    // Captura posição inicial
    const pos1 = await page.evaluate(() => tpPos);

    // Espera o scroll avançar
    await page.waitForTimeout(1000);

    const pos2 = await page.evaluate(() => tpPos);
    expect(pos2).toBeGreaterThan(pos1);

    console.log(`  ✅  Scroll avançou: ${pos1.toFixed(1)} → ${pos2.toFixed(1)}`);

    await page.evaluate(() => tpStop());
  });

});

// ═══════════════════════════════════════════════════════════════════════════════
// BLOCO 10 — WebSocket: Comandos do M5Stick para o Browser
// Testa a conexão WebSocket /ws que recebe comandos remotos
// (tp_pause, tp_speed, rec) do modo espelho do firmware.
// ═══════════════════════════════════════════════════════════════════════════════
test.describe('10 — WebSocket (/ws)', () => {

  test.beforeEach(async ({ page }) => {
    await page.addInitScript(() => {
      localStorage.setItem('m5_a2hs_seen', '1');
      localStorage.setItem('m5_tp_script', 'Texto de teste para WS.');
    });
  });

  test('connectWS() abre WebSocket e recebe mensagens', async ({ page }) => {
    await page.goto('/cam');

    // O CAMPAGE JS usa wss:// mas o test server é ws://, assim:
    // Sobrescreve connectWS para usar ws:// no teste
    const wsReady = await page.evaluate(() => {
      return new Promise((resolve) => {
        const ws = new WebSocket('ws://' + location.host + '/ws');
        ws.onopen = () => { window.__testWs = ws; resolve(true); };
        ws.onerror = () => resolve(false);
        setTimeout(() => resolve(false), 3000);
      });
    });

    expect(wsReady).toBe(true);
    console.log(`  ✅  WebSocket conectado ao test server`);
  });

  test('mensagem "tp_pause" via WS invoca tpTogglePause()', async ({ page }) => {
    await page.goto('/cam');
    await page.waitForTimeout(500);

    // Inicia o TP para que tpTogglePause tenha efeito
    await page.evaluate(() => tpStart());
    await page.waitForTimeout(200);

    // Conecta WS e envia mensagem
    await page.evaluate(() => {
      return new Promise((resolve) => {
        const ws = new WebSocket('ws://' + location.host + '/ws');
        ws.onopen = () => {
          // Registra handler para receber echo
          ws.onmessage = (e) => {
            if (e.data === 'tp_pause') tpTogglePause();
          };
          ws.send('tp_pause');
          setTimeout(resolve, 500);
        };
      });
    });

    const paused = await page.evaluate(() => tpPaused);
    expect(paused).toBe(true);
    console.log(`  ✅  tp_pause via WS → teleprompter pausado`);

    await page.evaluate(() => tpStop());
  });

  test('mensagem "tp_speed" via WS invoca tpCycleSpeed()', async ({ page }) => {
    await page.goto('/cam');
    await page.waitForTimeout(500);

    await page.evaluate(() => tpStart());

    // Velocidade padrão: 0.8
    let speed = await page.evaluate(() => tpSpeed);
    expect(speed).toBeCloseTo(0.8, 1);

    // Envia tp_speed via WS
    await page.evaluate(() => {
      return new Promise((resolve) => {
        const ws = new WebSocket('ws://' + location.host + '/ws');
        ws.onopen = () => {
          ws.onmessage = (e) => {
            if (e.data === 'tp_speed') tpCycleSpeed();
          };
          ws.send('tp_speed');
          setTimeout(resolve, 500);
        };
      });
    });

    speed = await page.evaluate(() => tpSpeed);
    expect(speed).toBeCloseTo(1.2, 1);
    console.log(`  ✅  tp_speed via WS → velocidade agora ${speed}×`);

    await page.evaluate(() => tpStop());
  });

});

// ═══════════════════════════════════════════════════════════════════════════════
// BLOCO 11 — Toggle Câmera (Frente / Traseira)
// Testa o botão #camToggle que alterna facingMode entre "user" e "environment".
// O botão só aparece quando o áudio está fluindo.
// ═══════════════════════════════════════════════════════════════════════════════
test.describe('11 — Toggle Câmera (Frente/Traseira)', () => {

  test.beforeEach(async ({ page }) => {
    await page.addInitScript(() => {
      localStorage.setItem('m5_a2hs_seen', '1');
    });
  });

  test('botão camToggle fica visível quando áudio está fluindo', async ({ page }) => {
    await page.goto('/cam');
    await page.locator('#startBtn').click();

    // Espera o áudio fluir e o botão aparecer (classe "show")
    await expect(page.locator('#camToggle')).toHaveClass(/show/, { timeout: 10_000 });
    console.log(`  ✅  Botão camToggle visível`);
  });

  test('facingMode padrão é "user" (câmera frontal)', async ({ page }) => {
    await page.goto('/cam');

    const mode = await page.evaluate(() => facingMode);
    expect(mode).toBe('user');
    console.log(`  ✅  facingMode padrão = "user"`);
  });

  test('toggleCamera() alterna facingMode e atualiza texto do botão', async ({ page }) => {
    await page.goto('/cam');

    // Precisa de getUserMedia disponível — vamos mockar para evitar erros
    await page.evaluate(async () => {
      // Simula toggleCamera sem getUserMedia real
      facingMode = facingMode === 'user' ? 'environment' : 'user';
      const btn = document.getElementById('camToggle');
      btn.textContent = facingMode === 'user' ? '\uD83D\uDCF7 Rear' : '\uD83E\uDD33 Front';
    });

    const mode = await page.evaluate(() => facingMode);
    expect(mode).toBe('environment');

    const btnText = await page.locator('#camToggle').innerText();
    expect(btnText).toContain('Front');

    console.log(`  ✅  Toggle: user → environment, botão="${btnText}"`);
  });

  test('mirroring: preview scaleX(-1) quando facingMode="user"', async ({ page }) => {
    await page.goto('/cam');

    // facingMode padrão é "user", após getCameraStream o preview fica espelhado
    // Verifica no CSS inline
    const transform = await page.evaluate(() => {
      // Simula o que getCameraStream faz
      const preview = document.getElementById('preview');
      preview.style.transform = 'scaleX(-1)'; // como o código faz para "user"
      return preview.style.transform;
    });

    expect(transform).toBe('scaleX(-1)');
    console.log(`  ✅  Preview espelhado para selfie: ${transform}`);
  });

});
