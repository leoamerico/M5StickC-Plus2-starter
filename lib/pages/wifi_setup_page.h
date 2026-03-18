#ifndef WIFI_SETUP_PAGE_H
#define WIFI_SETUP_PAGE_H

#include "page_base.h"
#include "../wifi_portal.h"
#include "../page_manager.h"
#include <Preferences.h>

class WifiSetupPage : public PageBase {
private:
    WifiPortal portal;
    PageManager* pageManager;
    bool portalRunning;
    bool justSaved;
    char savedSsid[33];

    static const int SCREEN_W = 240;
    static const int SCREEN_H = 135;

    void loadSavedSsid() {
        Preferences prefs;
        prefs.begin("wifi", true);
        String s = prefs.getString("ssid", "");
        prefs.end();
        strncpy(savedSsid, s.c_str(), sizeof(savedSsid) - 1);
        savedSsid[sizeof(savedSsid) - 1] = 0;
    }

    void drawScreen() {
        M5.Display.fillScreen(BLACK);

        // Header bar
        M5.Display.fillRect(0, 0, SCREEN_W, 14, 0x1082);
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(TFT_CYAN);
        M5.Display.setCursor(4, 3);
        M5.Display.print("WiFi Setup");

        if (justSaved) {
            M5.Display.setTextSize(2);
            M5.Display.setTextColor(TFT_GREEN);
            M5.Display.setCursor(6, 32);
            M5.Display.print("Saved!");
            M5.Display.setTextSize(1);
            M5.Display.setTextColor(TFT_DARKGREY);
            M5.Display.setCursor(6, 62);
            M5.Display.print("Credentials stored.");
            M5.Display.setCursor(4, SCREEN_H - 10);
            M5.Display.print("B:back to stream");
        } else if (portalRunning) {
            M5.Display.setTextSize(1);
            M5.Display.setTextColor(TFT_YELLOW);
            M5.Display.setCursor(6, 20);
            M5.Display.print("On your phone, join WiFi:");

            M5.Display.setTextSize(2);
            M5.Display.setTextColor(TFT_WHITE);
            M5.Display.setCursor(6, 36);
            M5.Display.print("M5Stick-Setup");

            M5.Display.setTextSize(1);
            M5.Display.setTextColor(TFT_YELLOW);
            M5.Display.setCursor(6, 62);
            M5.Display.print("Then enter your home WiFi");
            M5.Display.setCursor(6, 74);
            M5.Display.print("SSID and password.");

            M5.Display.setCursor(6, 94);
            int clients = portal.getClientCount();
            if (clients > 0) {
                M5.Display.setTextColor(TFT_GREEN);
                M5.Display.printf("Connected: %d device%s", clients, clients > 1 ? "s" : "");
            } else {
                M5.Display.setTextColor(TFT_DARKGREY);
                M5.Display.print("Waiting for connection...");
            }

            M5.Display.setCursor(4, SCREEN_H - 10);
            M5.Display.setTextColor(TFT_DARKGREY);
            M5.Display.print("B:cancel");
        } else {
            M5.Display.setTextSize(2);
            M5.Display.setTextColor(TFT_WHITE);
            M5.Display.setCursor(6, 24);
            M5.Display.print("WiFi Config");

            M5.Display.setTextSize(1);
            M5.Display.setCursor(6, 48);
            if (savedSsid[0]) {
                M5.Display.setTextColor(TFT_DARKGREY);
                M5.Display.printf("Current: %.22s", savedSsid);
            } else {
                M5.Display.setTextColor(TFT_ORANGE);
                M5.Display.print("No credentials saved");
            }

            M5.Display.setTextColor(TFT_DARKGREY);
            M5.Display.setCursor(6, 66);
            M5.Display.print("A: Start setup portal");
            M5.Display.setCursor(4, SCREEN_H - 10);
            M5.Display.print("A:setup  B:back");
        }
    }

    void startPortal() {
        portalRunning = portal.start();
        drawScreen();
    }

    void stopPortal() {
        if (portalRunning) {
            portal.stop();
            portalRunning = false;
        }
    }

public:
    WifiSetupPage(DisplayHandler* disp, PageManager* pm)
        : PageBase(disp, "WiFi Setup"),
          pageManager(pm),
          portalRunning(false),
          justSaved(false) {
        memset(savedSsid, 0, sizeof(savedSsid));
    }

    ~WifiSetupPage() { stopPortal(); }

    void setup() override {
        justSaved = false;
        portalRunning = false;
        loadSavedSsid();
        drawScreen();
    }

    void cleanup() override {
        stopPortal();
    }

    void loop() override {
        if (portalRunning) {
            portal.loop();

            // Keep device awake while portal is running
            settings->resetInactivityTimer();

            if (portal.credentialsSaved()) {
                stopPortal();
                justSaved = true;
                loadSavedSsid();
                drawScreen();
            }

            // Refresh client count every 2s
            static unsigned long lastRefresh = 0;
            if (millis() - lastRefresh > 2000) {
                drawScreen();
                lastRefresh = millis();
            }
        }
    }

    void handleInput() override {
        handleBasicInputInteractions();
    }

    const char* getName() override { return "WiFi Setup"; }

    void onButtonAPressed() override {
        settings->resetInactivityTimer();
        if (!portalRunning && !justSaved) {
            startPortal();
        }
    }

    void onButtonBShortPress() override {
        settings->resetInactivityTimer();
        stopPortal();
        // After saving, go to Audio Stream (page 1); otherwise go to Clock (page 0)
        pageManager->goToPage(justSaved ? 1 : 0);
    }

    void onButtonBLongPress() override {
        stopPortal();
        pageManager->goToPage(0);
    }

    void onButtonPWRPressed() override {
        settings->resetInactivityTimer();
        if (!portalRunning) {
            PageBase::onButtonPWRPressed();
        }
    }
};

#endif
