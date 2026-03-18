#ifndef DEVICE_INFO_HANDLER_H
#define DEVICE_INFO_HANDLER_H

#include <M5Unified.h>
#include <Arduino.h>
#include <functional>
#include "display_handler.h"

// One hardware component card
struct ComponentCard {
    const char* name;
    const char* lines[5]; // up to 5 spec lines (nullptr = unused)
};

class DeviceInfoViewer {
private:
    DisplayHandler* display;
    std::function<void()> onExit;
    bool active;
    int currentIndex;

    static const int SCREEN_W = 240;
    static const int SCREEN_H = 135;

    // --- Hardware component database for M5StickC Plus2 ---
    static const int CARD_COUNT = 12;
    const ComponentCard cards[CARD_COUNT] = {
        {
            "MCU",
            { "ESP32-PICO-V3-02", "Dual-core Xtensa LX6", "Clock: 240 MHz", "WiFi + BT 4.2 + BLE", nullptr }
        },
        {
            "Memory",
            { "SRAM:  520 KB", "PSRAM: 8 MB", "Flash: 8 MB", "NVS via Preferences", nullptr }
        },
        {
            "Display",
            { "1.14\" IPS LCD", "Resolution: 135x240 px", "Driver: ST7789V2", "65K colors (RGB565)", nullptr }
        },
        {
            "IMU",
            { "MPU6886  6-axis", "3-axis Gyroscope", "3-axis Accelerometer", "Interface: I2C", nullptr }
        },
        {
            "RTC",
            { "Chip: BM8563", "Interface: I2C", "Battery backup", "Alarm support", nullptr }
        },
        {
            "Speaker",
            { "Power: 1 W built-in", "Amplifier: NS4168", "I2S digital interface", nullptr, nullptr }
        },
        {
            "Microphone",
            { "SPM1423 PDM mic", "Resolution: 16-bit", "Sample rate: 16 KHz", "Interface: PDM / I2S", nullptr }
        },
        {
            "Power",
            { "Battery: 200 mAh LiPo", "Charging: USB-C", "Deep sleep support", "Power hold: GPIO4", nullptr }
        },
        {
            "Connectivity",
            { "WiFi: 802.11 b/g/n", "Band: 2.4 GHz", "BT: v4.2 BR/EDR", "BLE supported", nullptr }
        },
        {
            "IR",
            { "IR transmitter", "GPIO: 9", "Carrier: 38 KHz", "Remote ctrl capable", nullptr }
        },
        {
            "Buttons",
            { "BtnA  GPIO37 (front)", "BtnB  GPIO39 (side)", "BtnPWR power button", "Wakeup ext: BtnA", nullptr }
        },
        {
            "Board",
            { "M5StickC Plus2", "Size: 48x25.5x13 mm", "Weight: ~16 g", "Grove: I2C SDA32/SCL33", nullptr }
        }
    };

    void drawCard() {
        M5.Display.fillScreen(BLACK);

        const ComponentCard& card = cards[currentIndex];

        // Header bar
        M5.Display.fillRect(0, 0, SCREEN_W, 14, 0x1082); // dark grey
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(TFT_CYAN);
        M5.Display.setCursor(4, 3);
        M5.Display.print("Device Info");

        // Page indicator (top-right)
        char pg[12];
        snprintf(pg, sizeof(pg), "%d/%d", currentIndex + 1, CARD_COUNT);
        M5.Display.setTextColor(TFT_DARKGREY);
        int pgW = strlen(pg) * 6;
        M5.Display.setCursor(SCREEN_W - pgW - 4, 3);
        M5.Display.print(pg);

        // Component name
        M5.Display.setTextSize(2);
        M5.Display.setTextColor(TFT_YELLOW);
        M5.Display.setCursor(6, 18);
        M5.Display.print(card.name);

        // Spec lines
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(TFT_WHITE);
        int y = 38;
        for (int i = 0; i < 5; i++) {
            if (card.lines[i] == nullptr) break;
            M5.Display.setCursor(8, y);
            M5.Display.print(card.lines[i]);
            y += 16;
        }

        // Footer
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(TFT_DARKGREY);
        M5.Display.setCursor(4, SCREEN_H - 10);
        M5.Display.print("PWR:<  B:>  A:exit");
    }

public:
    DeviceInfoViewer(DisplayHandler* disp)
        : display(disp), active(false), currentIndex(0) {}

    void setOnExit(std::function<void()> cb) { onExit = cb; }

    bool isActive() { return active; }

    void start() {
        active = true;
        currentIndex = 0;
        drawCard();
    }

    void stop() {
        active = false;
    }

    void next() {
        if (!active) return;
        currentIndex = (currentIndex + 1) % CARD_COUNT;
        drawCard();
    }

    void prev() {
        if (!active) return;
        currentIndex = (currentIndex - 1 + CARD_COUNT) % CARD_COUNT;
        drawCard();
    }

    void exit() {
        active = false;
        if (onExit) onExit();
    }
};

#endif
