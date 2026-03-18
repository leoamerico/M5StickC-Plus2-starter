#ifndef AUDIO_STREAM_PAGE_H
#define AUDIO_STREAM_PAGE_H

#include "page_base.h"
#include "../wifi_helper.h"
#include "../audio_stream_handler.h"
#include "../settings_manager.h"
#include "../page_manager.h"
#include <Preferences.h>

// WiFi credentials stored in NVS under "wifi" namespace
#define WIFI_PREFS_NS   "wifi"
#define WIFI_KEY_SSID   "ssid"
#define WIFI_KEY_PASS   "pass"

class AudioStreamPage : public PageBase {
private:
    AudioStreamHandler* audioStream;
    PageManager* pageManager;

    enum State {
        ST_IDLE,          // Waiting for user to start
        ST_CONNECTING,    // Connecting to WiFi
        ST_STREAMING,     // Server running, showing URL
        ST_ERROR          // Connection failed
    };
    State state;

    char ssid[64];
    char pass[64];

    static const int SCREEN_W = 240;
    static const int SCREEN_H = 135;

    // ---------------------------------------------------------------
    void loadCredentials() {
        Preferences prefs;
        prefs.begin(WIFI_PREFS_NS, true);
        String s = prefs.getString(WIFI_KEY_SSID, "");
        String p = prefs.getString(WIFI_KEY_PASS, "");
        prefs.end();
        strncpy(ssid, s.c_str(), sizeof(ssid) - 1);
        strncpy(pass, p.c_str(), sizeof(pass) - 1);
    }

    bool hasCredentials() { return strlen(ssid) > 0; }

    void drawScreen() {
        M5.Display.fillScreen(BLACK);

        // Header
        M5.Display.fillRect(0, 0, SCREEN_W, 14, 0x1082);
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(TFT_CYAN);
        M5.Display.setCursor(4, 3);
        M5.Display.print("Audio Stream");

        switch (state) {
            case ST_IDLE: {
                M5.Display.setTextSize(2);
                M5.Display.setTextColor(TFT_WHITE);
                M5.Display.setCursor(6, 24);
                M5.Display.print("WiFi Audio");

                M5.Display.setTextSize(1);
                M5.Display.setTextColor(TFT_DARKGREY);
                M5.Display.setCursor(6, 50);
                if (hasCredentials()) {
                    M5.Display.printf("SSID: %.22s", ssid);
                    M5.Display.setCursor(6, 66);
                    M5.Display.print("A: Start stream");
                    M5.Display.setCursor(4, SCREEN_H - 10);
                    M5.Display.setTextColor(TFT_DARKGREY);
                    M5.Display.print("A:start  B:back");
                } else {
                    M5.Display.setTextColor(TFT_ORANGE);
                    M5.Display.print("No WiFi credentials");
                    M5.Display.setCursor(6, 66);
                    M5.Display.setTextColor(TFT_DARKGREY);
                    M5.Display.print("A: Open WiFi Setup");
                    M5.Display.setCursor(4, SCREEN_H - 10);
                    M5.Display.print("A:setup  B:back");
                }
                break;
            }
            case ST_CONNECTING: {
                M5.Display.setTextSize(2);
                M5.Display.setTextColor(TFT_YELLOW);
                M5.Display.setCursor(6, 30);
                M5.Display.print("Connecting");
                M5.Display.setCursor(6, 55);
                M5.Display.setTextSize(1);
                M5.Display.setTextColor(TFT_DARKGREY);
                M5.Display.printf("%.30s", ssid);
                break;
            }
            case ST_STREAMING: {
                String url = audioStream->getURL();

                // LIVE indicator + URL
                M5.Display.setTextSize(1);
                M5.Display.setTextColor(TFT_GREEN);
                M5.Display.setCursor(6, 18);
                M5.Display.print("LIVE");

                M5.Display.setTextColor(TFT_CYAN);
                M5.Display.setCursor(36, 18);
                M5.Display.print(url.c_str());

                // ── Real-time HUD ──
                int rssi = WiFiHelper::getSignalStrength();
                unsigned long elapsed = audioStream->getStreamElapsed();
                unsigned long remaining = audioStream->getStreamRemaining();
                uint32_t heap = ESP.getFreeHeap() / 1024;
                float cpuTemp = temperatureRead();
                bool wsClient = audioStream->hasClient();
                bool streaming = audioStream->isStreaming();

                // Stream time: elapsed / remaining
                M5.Display.setTextColor(TFT_WHITE);
                M5.Display.setCursor(6, 34);
                M5.Display.printf("Time  %lu:%02lu / %lu:%02lu left",
                    elapsed / 60, elapsed % 60,
                    remaining / 60, remaining % 60);

                // WiFi signal bar
                M5.Display.setCursor(6, 48);
                uint16_t rssiColor = (rssi > -60) ? TFT_GREEN : (rssi > -75) ? TFT_YELLOW : TFT_RED;
                M5.Display.setTextColor(rssiColor);
                M5.Display.printf("WiFi  %d dBm", rssi);

                // Heap
                M5.Display.setCursor(140, 48);
                uint16_t heapColor = (heap > 80) ? TFT_GREEN : (heap > 40) ? TFT_YELLOW : TFT_RED;
                M5.Display.setTextColor(heapColor);
                M5.Display.printf("Heap %luK", heap);

                // CPU temp
                M5.Display.setCursor(6, 62);
                uint16_t tempColor = (cpuTemp < 60) ? TFT_GREEN : (cpuTemp < 75) ? TFT_YELLOW : TFT_RED;
                M5.Display.setTextColor(tempColor);
                M5.Display.printf("CPU   %.0f C", cpuTemp);

                // Client + stream status
                M5.Display.setCursor(140, 62);
                M5.Display.setTextColor(wsClient ? TFT_GREEN : TFT_DARKGREY);
                M5.Display.print(wsClient ? "WS " : "WS -");
                M5.Display.setCursor(185, 62);
                M5.Display.setTextColor(streaming ? TFT_GREEN : TFT_DARKGREY);
                M5.Display.print(streaming ? "MIC " : "MIC -");

                // Separator line
                M5.Display.drawFastHLine(6, 77, SCREEN_W - 12, 0x2945);

                // Mic audio indicator (visual bar)
                M5.Display.setCursor(6, 82);
                M5.Display.setTextColor(TFT_DARKGREY);
                M5.Display.print("Open Safari on iPhone");

                // Bottom bar: button hints
                M5.Display.fillRect(0, SCREEN_H - 14, SCREEN_W, 14, 0x1082);
                M5.Display.setCursor(4, SCREEN_H - 11);
                M5.Display.setTextColor(TFT_DARKGREY);
                M5.Display.print("B:tp  PWR:spd  hold B:stop");
                break;
            }
            case ST_ERROR: {
                M5.Display.setTextSize(2);
                M5.Display.setTextColor(TFT_RED);
                M5.Display.setCursor(6, 28);
                M5.Display.print("Failed!");
                M5.Display.setTextSize(1);
                M5.Display.setTextColor(TFT_DARKGREY);
                M5.Display.setCursor(6, 56);
                M5.Display.print("Check WiFi credentials");
                M5.Display.setCursor(4, SCREEN_H - 10);
                M5.Display.print("A:WiFi Setup  B:back");
                break;
            }
        }
    }

    void doConnect() {
        state = ST_CONNECTING;
        drawScreen();

        bool ok = WiFiHelper::connect(ssid, pass);
        if (!ok) {
            state = ST_ERROR;
            drawScreen();
            return;
        }

        bool started = audioStream->start();
        if (!started) {
            state = ST_ERROR;
            drawScreen();
            return;
        }

        state = ST_STREAMING;
        drawScreen();
    }

    void doStop() {
        audioStream->stop();
        WiFiHelper::disconnect();
        state = ST_IDLE;
    }

public:
    AudioStreamPage(DisplayHandler* disp, PageManager* pm)
        : PageBase(disp, "Audio Stream"),
          pageManager(pm),
          state(ST_IDLE) {
        audioStream = new AudioStreamHandler();
        memset(ssid, 0, sizeof(ssid));
        memset(pass, 0, sizeof(pass));
        loadCredentials();
    }

    ~AudioStreamPage() {
        doStop();
        delete audioStream;
    }

    void setup() override {
        loadCredentials();
        state = ST_IDLE;
        drawScreen();
    }

    void loop() override {
        // Refresh HUD metrics while streaming (every 2s)
        static unsigned long lastRefresh = 0;
        if (state == ST_STREAMING && millis() - lastRefresh > 2000) {
            drawScreen();
            lastRefresh = millis();
        }
    }

    void handleInput() override {
        handleBasicInputInteractions();
    }

    const char* getName() override { return "Audio Stream"; }

    // A button: start / fix credentials
    void onButtonAPressed() override {
        settings->resetInactivityTimer();
        if (state == ST_ERROR) {
            doStop();
            pageManager->goToPage(2); // WiFi Setup to fix credentials
        } else if (state == ST_IDLE) {
            if (hasCredentials()) {
                doConnect();
            } else {
                pageManager->goToPage(2); // WiFi Setup
            }
        }
    }

    // B short: pause/resume teleprompter while streaming; back otherwise
    void onButtonBShortPress() override {
        settings->resetInactivityTimer();
        if (state == ST_STREAMING) {
            audioStream->setTpCmd(1); // tp_pause event → browser toggles TP
        } else {
            doStop();
            pageManager->goToPage(0);
        }
    }

    // B long: stop stream and go back to ClockPage (index 0)
    void onButtonBLongPress() override {
        doStop();
        pageManager->goToPage(0);
    }

    // PWR short: cycle teleprompter scroll speed while streaming
    void onButtonPWRPressed() override {
        settings->resetInactivityTimer();
        if (state == ST_STREAMING) {
            audioStream->setTpCmd(2); // tp_speed event → browser cycles speed
        } else {
            // Default: navigate up (via base class behaviour)
            PageBase::onButtonPWRPressed();
        }
    }
};

#endif
