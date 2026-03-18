// @ts-check
const { defineConfig } = require('@playwright/test');

/**
 * Playwright Configuration
 * Docs: https://playwright.dev/docs/test-configuration
 *
 * Conceitos-chave:
 *   baseURL      → URL base; os testes usam paths relativos: page.goto('/cam')
 *   headless     → false = browser VISÍVEL (você acompanha em tempo real!)
 *   slowMo       → pausa (ms) entre cada ação — ótimo para aprendizado
 *   webServer    → Playwright sobe o servidor antes de rodar os testes
 */
module.exports = defineConfig({
  testDir: './tests',

  // Tempo máximo por teste
  timeout: 40_000,

  // Reporters: texto no terminal + HTML navegável depois
  reporter: [
    ['list'],
    ['html', { outputFolder: 'playwright-report', open: 'never' }],
  ],

  // Pasta de artefatos (screenshots e vídeos de falhas)
  outputDir: './test-results',

  use: {
    baseURL: 'http://localhost:3333',

    // ← BROWSER VISÍVEL — comente esta linha para modo headless (CI)
    headless: false,

    // Cada ação espera 800 ms extra → você consegue acompanhar o que acontece
    slowMo: 800,

    // Salva screenshot/vídeo automaticamente em caso de falha
    screenshot: 'only-on-failure',
    video:      'retain-on-failure',

    // Viewport de iPhone 14 Pro — a interface foi desenhada para mobile
    viewport: { width: 390, height: 844 },
  },

  projects: [
    {
      name: 'Chromium (headed)',
      use: {
        browserName: 'chromium',
        launchOptions: {
          args: [
            // Aproxima a câmera/mic sem hardware real:
            '--use-fake-ui-for-media-stream',      // aprova permissão automaticamente
            '--use-fake-device-for-media-stream',  // câmera e microfone sintéticos
            '--autoplay-policy=no-user-gesture-required', // AudioContext sem gesto
            '--no-sandbox',
          ],
        },
      },
    },
  ],

  // Playwright sobe este servidor antes de iniciar os testes
  // e o derruba quando todos terminam
  webServer: {
    command:              'node server.js',
    url:                  'http://localhost:3333',
    reuseExistingServer:  true,   // reutiliza se já estiver rodando
    timeout:              12_000,
  },
});
