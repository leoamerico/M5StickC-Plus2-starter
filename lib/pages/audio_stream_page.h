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

                M5.Display.setTextSize(1);
                M5.Display.setTextColor(TFT_GREEN);
                M5.Display.setCursor(6, 20);
                M5.Display.print("Streaming  LIVE");

                M5.Display.setTextColor(TFT_WHITE);
                M5.Display.setCursor(6, 38);
                M5.Display.print("Open in iPhone Safari:");

                // URL, split into two lines if long
                M5.Display.setTextColor(TFT_CYAN);
                M5.Display.setCursor(6, 54);
                if (url.length() <= 22) {
                    M5.Display.print(url.c_str());
                } else {
                    // show IP + path on two rows
                    M5.Display.print(url.substring(0, 22).c_str());
                    M5.Display.setCursor(6, 70);
                    M5.Display.print(url.substring(22).c_str());
                }

                M5.Display.setTextSize(1);
                M5.Display.setTextColor(TFT_DARKGREY);
                M5.Display.setCursor(6, 90);
                int rssi = WiFiHelper::getSignalStrength();
                M5.Display.printf("WiFi RSSI: %d dBm", rssi);

                M5.Display.setCursor(4, SCREEN_H - 10);
                M5.Display.print("B:pause TP  PWR:speed  hold B:stop");
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
                M5.Display.print("A:retry  B:back");
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
        // Refresh RSSI indicator while streaming (every 5s)
        static unsigned long lastRefresh = 0;
        if (state == ST_STREAMING && millis() - lastRefresh > 5000) {
            drawScreen();
            lastRefresh = millis();
        }
    }

    void handleInput() override {
        handleBasicInputInteractions();
    }

    const char* getName() override { return "Audio Stream"; }

    // A button: start / retry
    void onButtonAPressed() override {
        settings->resetInactivityTimer();
        if (state == ST_IDLE || state == ST_ERROR) {
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
