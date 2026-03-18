#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#ifndef UNIT_TEST
#include <Preferences.h>
#include <Arduino.h>
#endif

// Singleton with cache for settings since prefs reading is slow and power consuming
class SettingsManager {
private:
    static SettingsManager* instance;
#ifndef UNIT_TEST
    unsigned long lastActionTime;
    unsigned long timeForAutoDeepSleep;
    Preferences prefs;
#endif
    
    // Settings cache read only ONCE on startup
    struct {
        bool uiSound;
        uint8_t brightness;
        bool time24h;
        bool autoSleep;
        uint16_t autoSleepDelay;
    } cache;
    
    SettingsManager() {
        // Private constructor (singleton)
    }
    
public:
    // Get singleton instance
    static SettingsManager* getInstance() {
        if (instance == nullptr) {
            instance = new SettingsManager();
        }
        return instance;
    }
    
    // Load settings from NVS into cache (called ONCE at setup setup)
    void begin() {
#ifndef UNIT_TEST
        prefs.begin("settings", true);
        
        // Load UI settings
        cache.uiSound = prefs.getBool("ui_sound", true);
        
        // Load time settings (not used yet)
        cache.time24h = prefs.getBool("time_24h", true);
        
        cache.autoSleep = prefs.getBool("auto_sleep", true);
        cache.autoSleepDelay = prefs.getUShort("sleep_delay", 15);
        if (cache.autoSleepDelay < 5) cache.autoSleepDelay = 5; // Never below 5s (guards NVS corruption)
        
        resetInactivityTimer();
        prefs.end();
#else
        // Unit test: use defaults, no NVS access
        cache.uiSound = true;
        cache.time24h = true;
        cache.autoSleep = true;
        cache.autoSleepDelay = 15;
#endif
    }
    
    // GETTERS (just read the cache)
    
    bool getUiSound() { return cache.uiSound; }
    bool getTime24h() { return cache.time24h; }
    bool getAutoSleep() { return cache.autoSleep; }
    uint16_t getAutoSleepDelay() { return cache.autoSleepDelay; }
    
    // SETTERS (update cache + save to NVS)
    void setUiSound(bool value) {
        cache.uiSound = value;
#ifndef UNIT_TEST
        prefs.begin("settings", false);
        prefs.putBool("ui_sound", value);
        prefs.end();
#endif
    }
    
    void setTime24h(bool value) {
        cache.time24h = value;
#ifndef UNIT_TEST
        prefs.begin("settings", false);
        prefs.putBool("time_24h", value);
        prefs.end();
#endif
    }
    
    void setAutoSleep(bool value) {
        cache.autoSleep = value;
#ifndef UNIT_TEST
        prefs.begin("settings", false);
        prefs.putBool("auto_sleep", value);
        prefs.end();
#endif
    }
    
    void setAutoSleepDelay(uint16_t seconds) {
        cache.autoSleepDelay = seconds;
#ifndef UNIT_TEST
        prefs.begin("settings", false);
        prefs.putUShort("sleep_delay", seconds);
        prefs.end();
#endif
    }
    
    // Reset to defaults
    void resetToDefaults() {
#ifndef UNIT_TEST
        prefs.begin("settings", false);
        prefs.clear();
        prefs.end();
#endif
        // Reload defaults
        begin();
    }

    void resetInactivityTimer() {
#ifndef UNIT_TEST
        lastActionTime = millis();
        timeForAutoDeepSleep = millis() + (cache.autoSleepDelay * 1000UL);
#endif
    }

    bool shouldGoToSleep() {
#ifndef UNIT_TEST
        if (!cache.autoSleep) {
            return false;
        }

        // Always stay awake for at least 45 seconds after boot
        // This prevents an immediate-sleep loop if NVS has a corrupt delay value
        if (millis() < 45000UL) {
            return false;
        }
        
        unsigned long now = millis();
        
        if (now < lastActionTime) {
            resetInactivityTimer();
            return false;
        }
        
        return now >= timeForAutoDeepSleep;
#else
        return false;
#endif
    }
    
    // Debug: print all settings
    void printAll() {
#ifndef UNIT_TEST
        Serial.println("=== Current Settings ===");
        Serial.printf("UI Sound: %s\n", cache.uiSound ? "ON" : "OFF");
        Serial.printf("Time Format: %s\n", cache.time24h ? "24h" : "12h");
        Serial.printf("Auto Sleep: %s\n", cache.autoSleep ? "ON" : "OFF");
        Serial.printf("Sleep Delay: %d s\n", cache.autoSleepDelay);
        Serial.println("========================");
#endif
    }
};

// Initialize static instance
SettingsManager* SettingsManager::instance = nullptr;

#endif