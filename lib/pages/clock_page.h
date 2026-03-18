#ifndef CLOCK_PAGE_H
#define CLOCK_PAGE_H

#include "page_base.h"
#include "../clock_handler.h"
#include "../battery_handler.h"
#include "../settings_manager.h"
#include "../time_selector.h"
#include "../rtc_utils.h"
#include "../device_info_handler.h"
#include "../alarm_handler.h"
#include "../page_manager.h"
#include <stdint.h>

class ClockPage : public PageBase {
private:
    ClockHandler* clockHandler;
    BatteryHandler* batteryHandler;
    PageManager* pageManager;
    
    unsigned long lastClockUpdate;
    uint32_t clockRefreshInterval;
    
    MenuHandler* settingsMenu;
    TimeSelector* timeSelector;
    DeviceInfoViewer* deviceInfo;
    AlarmHandler* alarmHandler;
    char alarmLabel[32];
    char timerLabel[32];
    bool timerRinging;
    
    char soundLabel[32];
    char timeFormatLabel[32];
    char autoSleepLabel[32];
    char sleepDelayLabel[48];
    char raiseWakeLabel[32];

    void updateMenuLabels() {
        sprintf(soundLabel, "UI Sound: %s", 
                settings->getUiSound() ? "ON" : "OFF");
        
        sprintf(timeFormatLabel, "Time: %s", 
                settings->getTime24h() ? "24h" : "12h");
        
        sprintf(autoSleepLabel, "Auto Sleep: %s",
                settings->getAutoSleep() ? "ON" : "OFF");
        
        sprintf(sleepDelayLabel, "Sleep Delay: %ds",
                settings->getAutoSleepDelay());
        
        sprintf(raiseWakeLabel, "Raise Wake: %s",
                settings->getRaiseToWake() ? "ON" : "OFF");
    }

    void updateAlarmLabel() {
        if (alarmHandler->isEnabled()) {
            char t[8];
            alarmHandler->getTimeStr(t, sizeof(t));
            sprintf(alarmLabel, "Alarm: %s", t);
        } else {
            strcpy(alarmLabel, "Alarm: OFF");
        }
    }

    void updateTimerLabel() {
        uint32_t target = clockHandler->readTarget();
        if (target) {
            uint32_t nowE = rtcEpochNow();
            if (nowE < target) {
                uint32_t remain = target - nowE;
                sprintf(timerLabel, "Timer: %02u:%02u", remain / 60, remain % 60);
            } else {
                strcpy(timerLabel, "Set Timer");
            }
        } else {
            strcpy(timerLabel, "Set Timer");
        }
    }

    void rebuildMainMenu() {
        mainMenu->clear();
        updateAlarmLabel();
        updateTimerLabel();
        
        mainMenu->addItem(timerLabel,        [this]() { onSetTimer(); });
        mainMenu->addItem("Set Alarm",      [this]() { onSetAlarm(); });
        mainMenu->addItem(alarmLabel,        [this]() { onToggleAlarm(); });
        mainMenu->addItem("Audio Stream",   [this]() { onAudioStream(); });
        mainMenu->addItem("Device Info",    [this]() { onDeviceInfo(); });
        mainMenu->addItem("Settings",       [this]() { onSettings(); });
        mainMenu->addItem("Restart",        [this]() { onRestart(); });
    }
    
    void rebuildSettingsMenu() {
        if (!settingsMenu) {
            settingsMenu = new MenuHandler(display, "Settings");
        } else {
            settingsMenu->clear();
        }
        
        updateMenuLabels();
        
        settingsMenu->addItem(soundLabel, [this]() { onToggleSound(); });
        settingsMenu->addItem(timeFormatLabel, [this]() { onToggleTimeFormat(); });
        settingsMenu->addItem(autoSleepLabel, [this]() { onToggleAutoSleep(); });
        settingsMenu->addItem(sleepDelayLabel, [this]() { onConfigureSleepDelay(); });
        settingsMenu->addItem(raiseWakeLabel, [this]() { onToggleRaiseWake(); });
        settingsMenu->addItem("Set Time", [this]() { onSetTime(); });
        settingsMenu->addItem("Set Date", [this]() { onSetDate(); });
        settingsMenu->addItem("< Back", [this]() { closeMenu(); });
    }
    
    void onSetTimer() {
        // If timer is already running, cancel it
        if (clockHandler->readTarget()) {
            clockHandler->clearTarget();
            display->showFullScreenMessage("Timer", "Cancelled", MSG_ERROR, 800);
            rebuildMainMenu();
            mainMenu->draw();
            return;
        }
        
        menuManager->closeAll();
        
        // 5-minute steps: 5, 10, 15, ..., 120
        timeSelector->configureMinutesStep(5, 5, 120, 5);
        
        timeSelector->setOnComplete([this](TimeValue result) {
            uint32_t minutes = result.minutes;
            uint32_t nowE = rtcEpochNow();
            uint32_t target = nowE + minutes * 60;
            clockHandler->writeTarget(target);
            
            char msg[32];
            sprintf(msg, "%d min", minutes);
            display->showFullScreenMessage("Timer Set", msg, MSG_SUCCESS, 1000);
            
            rebuildMainMenu();
            menuManager->pushMenu(mainMenu);
        });
        
        timeSelector->start();
    }
    
    void onSetAlarm() {
        menuManager->closeAll();
        
        uint8_t curH = alarmHandler->getHour();
        uint8_t curM = alarmHandler->getMinute();
        timeSelector->configureHoursMinutes(curH, curM);
        
        timeSelector->setOnComplete([this](TimeValue result) {
            alarmHandler->setTime(result.hours, result.minutes);
            
            char msg[16];
            sprintf(msg, "%02d:%02d", result.hours, result.minutes);
            display->showFullScreenMessage("Alarm Set", msg, MSG_SUCCESS, 1000);
            
            rebuildMainMenu();
            menuManager->pushMenu(mainMenu);
        });
        
        timeSelector->start();
    }
    
    void onToggleAlarm() {
        bool current = alarmHandler->isEnabled();
        alarmHandler->setEnabled(!current);
        
        if (!current) {
            char msg[16];
            alarmHandler->getTimeStr(msg, sizeof(msg));
            display->showFullScreenMessage("Alarm", msg, MSG_SUCCESS, 800);
        } else {
            display->showFullScreenMessage("Alarm", "OFF", MSG_ERROR, 800);
        }
        
        rebuildMainMenu();
        mainMenu->draw();
    }
    
    void onSettings() {
        rebuildSettingsMenu();
        menuManager->pushMenu(settingsMenu);
    }

    void onAudioStream() {
        menuManager->closeAll();
        pageManager->goToPage(1); // AudioStreamPage is always index 1
    }

    void onRestart() {
        menuManager->closeAll();
        display->showFullScreenMessage("Restart", "Restarting...", MSG_INFO, 1200);
        ESP.restart();
    }

    void onDeviceInfo() {
        menuManager->closeAll();
        deviceInfo->setOnExit([this]() {
            display->clearScreen();
            clockHandler->drawClock(0);
        });
        deviceInfo->start();
    }
    
    void onToggleSound() {
        bool current = settings->getUiSound();
        settings->setUiSound(!current);
        
        display->showFullScreenMessage(
            "UI Sound", 
            current ? "OFF" : "ON", 
            current ? MSG_ERROR : MSG_SUCCESS, 
            800
        );
        
        rebuildSettingsMenu();
        settingsMenu->draw();
    }
    
    void onToggleTimeFormat() {
        bool current = settings->getTime24h();
        settings->setTime24h(!current);
        
        display->showFullScreenMessage(
            "Time Format", 
            current ? "12h" : "24h", 
            MSG_INFO, 
            800
        );
        
        rebuildSettingsMenu();
        settingsMenu->draw();
    }
    
    void onToggleAutoSleep() {
        bool current = settings->getAutoSleep();
        settings->setAutoSleep(!current);
        
        display->showFullScreenMessage(
            "Auto Sleep", 
            current ? "OFF" : "ON", 
            current ? MSG_ERROR : MSG_SUCCESS, 
            800
        );
        
        rebuildSettingsMenu();
        settingsMenu->draw();
    }
    
    void onToggleRaiseWake() {
        bool current = settings->getRaiseToWake();
        settings->setRaiseToWake(!current);
        
        display->showFullScreenMessage(
            "Raise Wake", 
            current ? "OFF" : "ON", 
            current ? MSG_ERROR : MSG_SUCCESS, 
            800
        );
        
        rebuildSettingsMenu();
        settingsMenu->draw();
    }
    
    void onConfigureSleepDelay() {
        menuManager->closeAll();
        
        uint16_t currentDelay = settings->getAutoSleepDelay();
        timeSelector->configureSeconds(currentDelay, 5, 120);
        
        timeSelector->setOnComplete([this](TimeValue result) {
            settings->setAutoSleepDelay(result.seconds);
            
            char msg[32];
            sprintf(msg, "%d seconds", result.seconds);
            display->showFullScreenMessage("Sleep Delay", msg, MSG_SUCCESS, 1000);
            
            rebuildSettingsMenu();
            menuManager->pushMenu(mainMenu);
            menuManager->pushMenu(settingsMenu);
        });
        
        timeSelector->start();
    }
    
    void onSetTime() {
        menuManager->closeAll();
        
        uint8_t currentHour = rtcGetHours();
        uint8_t currentMinute = rtcGetMinutes();
        
        timeSelector->configureHoursMinutes(currentHour, currentMinute);
        
        timeSelector->setOnComplete([this](TimeValue result) {
            // Update RTC with new time, keep seconds to 0 just to simplify
            rtcSetTime(result.hours, result.minutes, 0);
            
            char msg[32];
            sprintf(msg, "%02d:%02d", result.hours, result.minutes);
            display->showFullScreenMessage("Time Set", msg, MSG_SUCCESS, 1000);
            
            rebuildSettingsMenu();
            menuManager->pushMenu(mainMenu);
            menuManager->pushMenu(settingsMenu);
        });
        
        timeSelector->start();
    }

    void onSetDate() {
        menuManager->closeAll();

        uint8_t currentDay   = rtcGetDay();
        uint8_t currentMonth = rtcGetMonth();
        uint16_t currentYear = rtcGetYear();
        uint8_t yOff = (currentYear >= 2000 && currentYear <= 2099)
                       ? (uint8_t)(currentYear - 2000) : 26;

        timeSelector->configureDayMonthYear(currentDay, currentMonth, yOff);

        timeSelector->setOnComplete([this](TimeValue result) {
            uint16_t year = 2000 + result.yearOffset;
            rtcSetDate(result.day, result.month, year);

            char msg[32];
            sprintf(msg, "%02d/%02d/%04d", result.day, result.month, year);
            display->showFullScreenMessage("Date Set", msg, MSG_SUCCESS, 1000);

            rebuildSettingsMenu();
            menuManager->pushMenu(mainMenu);
            menuManager->pushMenu(settingsMenu);
        });

        timeSelector->start();
    }
    
    // Callback overrides to handle TimeSelector
    void ringAlarm() {
        M5.Speaker.begin();
        M5.Speaker.setVolume(200);
        bool ringing = true;
        
        while (ringing) {
            display->clearScreen();
            display->displayMainTitle("ALARM!", MSG_ERROR);
            
            // Show alarm time or "Timer" depending on source
            if (timerRinging) {
                display->displaySubtitle("Timer", MSG_WARNING);
            } else {
                char t[8];
                alarmHandler->getTimeStr(t, sizeof(t));
                display->displaySubtitle(t, MSG_WARNING);
            }
            display->displayStatus("Press to dismiss", MSG_INFO);
            
            for (int i = 0; i < 4 && ringing; i++) {
                M5.Speaker.tone(2500, 150);
                delay(200);
                M5.Speaker.tone(3200, 150);
                delay(200);
                M5.update();
                if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnPWR.wasPressed()) {
                    ringing = false;
                    break;
                }
            }
            if (!ringing) break;
            delay(600);
            M5.update();
            if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnPWR.wasPressed()) {
                ringing = false;
            }
        }
        
        if (timerRinging) timerRinging = false;
        if (alarmHandler->isRinging()) alarmHandler->dismiss();
        
        M5.Speaker.end();
        settings->resetInactivityTimer();
        display->clearScreen();
    }

    void onButtonPWRPressed() override {
        if (alarmHandler->isRinging() || timerRinging) { alarmHandler->dismiss(); timerRinging = false; return; }
        if (timeSelector->isActive()) {
            timeSelector->navigateUp();
        } else if (deviceInfo->isActive()) {
            deviceInfo->prev();
        } else if (hasActiveMenu()) {
            navigateMenuUp();
        }
    }
    
    void onButtonAPressed() override {
        if (alarmHandler->isRinging() || timerRinging) { alarmHandler->dismiss(); timerRinging = false; return; }
        if (timeSelector->isActive()) {
            timeSelector->select();
        } else if (deviceInfo->isActive()) {
            deviceInfo->exit();
        } else if (hasActiveMenu()) {
            selectMenuItem();
        } else {
            openMenu();
        }
    }
    
    void onButtonBShortPress() override {
        if (alarmHandler->isRinging() || timerRinging) { alarmHandler->dismiss(); timerRinging = false; return; }
        if (timeSelector->isActive()) {
            timeSelector->navigateDown();
        } else if (deviceInfo->isActive()) {
            deviceInfo->next();
        } else if (hasActiveMenu()) {
            navigateMenuDown();
        }
    }
    
public:
    ClockPage(DisplayHandler* disp, ClockHandler* clock, BatteryHandler* battery, PageManager* pm)
        : PageBase(disp, "Clock Menu"),
          clockHandler(clock),
          batteryHandler(battery),
          pageManager(pm),
          settingsMenu(nullptr) {
        
        settings = SettingsManager::getInstance();
        
        lastClockUpdate = 0;
        clockRefreshInterval = 1000;
        
        timeSelector = new TimeSelector(display, "Set Time");
        deviceInfo   = new DeviceInfoViewer(display);
        alarmHandler = new AlarmHandler();
        timerRinging = false;
        
        updateMenuLabels();
        rebuildMainMenu();
    }
    
    ~ClockPage() {
        if (settingsMenu) {
            delete settingsMenu;
        }
        if (timeSelector) {
            delete timeSelector;
        }
        if (deviceInfo) {
            delete deviceInfo;
        }
        if (alarmHandler) {
            delete alarmHandler;
        }
    }
    
    void setup() override {
        display->clearScreen();
        clockHandler->drawClock(0);
        lastClockUpdate = millis();
    }
    
    void loop() override {
        if (timeSelector->isActive()) {
            return;
        }

        if (deviceInfo->isActive()) {
            return;
        }
        
        if (hasActiveMenu()) {
            return;
        }
        
        unsigned long now = millis();
        if (now - lastClockUpdate >= clockRefreshInterval) {
            // Check alarm
            uint8_t curH = rtcGetHours();
            uint8_t curM = rtcGetMinutes();
            if (alarmHandler->check(curH, curM)) {
                ringAlarm();
            }
            
            uint32_t target = clockHandler->readTarget();
            uint32_t remain = 0;
            
            if (target) {
                uint32_t nowE = rtcEpochNow();
                if (nowE < target) {
                    remain = target - nowE;
                } else {
                    // Timer expired — ring!
                    clockHandler->clearTarget();
                    timerRinging = true;
                    ringAlarm();
                }
            }
            
            // Build alarm indicator string
            const char* alarmStr = nullptr;
            char alarmBuf[16];
            if (alarmHandler->isEnabled()) {
                char t[8];
                alarmHandler->getTimeStr(t, sizeof(t));
                snprintf(alarmBuf, sizeof(alarmBuf), "Alarm %s", t);
                alarmStr = alarmBuf;
            }
            
            clockHandler->drawClock(remain, alarmStr);
            batteryHandler->displayInfo();
            lastClockUpdate = now;
        }
    }
    
    void handleInput() override {
        handleBasicInputInteractions();
    }
    
    const char* getName() override {
        return "Clock";
    }
    
    ClockHandler* getClockHandler() { return clockHandler; }
};

#endif