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

  test('espia /manifest.json sem modificá-la', async ({ page }) => {
    let capturedUrl   = '';
    let capturedStatus = 0;

    // page.route(padrão, handler) — padrão pode ser string, glob ou RegExp
    // route.continue() → deixa a requisição original seguir normalmente
    // route.request()  → objeto com info da requisição original
    await page.route('/manifest.json', async route => {
      capturedUrl = route.request().url();
      console.log(`  📡  Interceptei: ${capturedUrl}`);

      const response = await route.fetch();          // executa a req. original
      capturedStatus = response.status();
      await route.fulfill({ response });             // repassa a resposta
    });

    await page.goto('/cam');
    await page.evaluate(() => fetch('/manifest.json'));
    await page.waitForTimeout(500);

    expect(capturedUrl).toContain('/manifest.json');
    expect(capturedStatus).toBe(200);
    console.log(`  ✅  Status capturado: ${capturedStatus}`);
  });

  test('substitui /manifest.json com dados de teste (mock total)', async ({ page }) => {
    // route.fulfill() → responde com dados NOSSOS — servidor real nunca é chamado!
    await page.route('/manifest.json', route => {
      route.fulfill({
        status:      200,
        contentType: 'application/manifest+json',
        body:        JSON.stringify({ name: '🧪 Playwright Mock', display: 'standalone' }),
      });
    });

    await page.goto('/cam');

    // page.evaluate(fn) → executa a função no contexto DO BROWSER e retorna o resultado
    const manifest = await page.evaluate(async () => {
      const res = await fetch('/manifest.json');
      return res.json();
    });

    // O servidor devolveu NOSSO mock
    expect(manifest.name).toBe('🧪 Playwright Mock');
    console.log(`  ✅  Mock funcionou: name = "${manifest.name}"`);
  });

  test('simula erro 503 no /audio → página mostra estado de erro', async ({ page }) => {
    // Abortar a requisição faz o fetch() do browser rejeitar
    await page.route('/audio', route => route.abort('connectionreset'));

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

    // A página deve buscar ao menos o manifest e o ícone
    // (browser respeita <link rel="manifest"> e <link rel="apple-touch-icon">)
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
    await page.route('/audio', route => route.abort());

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

      await page.screenshot({ path: path.join(SHOTS, '06-download-ready.png') });
    });
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
    await page.route('/audio', route => route.abort());

    await page.goto('/cam');
    await page.locator('#startBtn').click();
    await page.waitForTimeout(1500);

    // Lê os dados que o mock guardou no window
    const calls       = await page.evaluate(() => window.__getUserMediaCalls || 0);
    const constraints = await page.evaluate(() => window.__getUserMediaConstraints);

    expect(calls).toBe(1);
    expect(constraints?.video?.facingMode).toBe('environment'); // câmera traseira
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
