#ifndef CLOCK_HANDLER_H
#define CLOCK_HANDLER_H

#include <M5Unified.h>
#include <Arduino.h>
#include <Preferences.h>

#include "./display_handler.h"
#include "./rtc_utils.h" 

class ClockHandler {
private:
    m5::rtc_datetime_t dt;
    char timeBuffer[16];  // Buffer to store time, array of 16 caracters (string)
    char dateBuffer[24];  // Buffer to store date
    DisplayHandler* display;

    const char* weekDayAbbr(int wd) {
        static const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        return (wd >= 0 && wd <= 6) ? days[wd] : "???";
    }
public:
    Preferences prefs;
    static const int CLOCK_REFRESH_MS = 1000;

    // Constructor
    ClockHandler(DisplayHandler* displayHandler) : display(displayHandler) {
        memset(timeBuffer, 0, sizeof(timeBuffer));
        memset(dateBuffer, 0, sizeof(dateBuffer));
    }
    
    // Update internal datetime
    void updateDateTime() {
        M5.Rtc.getDateTime(&dt);
    }
    
    // Get current time as string (format: HH:MM)
    const char* getCurrentFullTime() {
        updateDateTime();
        sprintf(timeBuffer, "%02d:%02d", dt.time.hours, dt.time.minutes);
        return timeBuffer;
    }
    
    // Get current date in BR format with weekday (format: Wed 18/03)
    const char* getCurrentFullDateFR() {
        updateDateTime();
        sprintf(dateBuffer, "%s %02d/%02d", weekDayAbbr(dt.date.weekDay),
                dt.date.date, dt.date.month);
        return dateBuffer;
    }
    
    // Get current date in US format (format: MM-DD-YYYY)
    const char* getCurrentFullDateUS() {
        updateDateTime();
        sprintf(dateBuffer, "%02d-%02d-%04d", dt.date.month, dt.date.date, dt.date.year);
        return dateBuffer;
    }
    
    // Get current date in ISO format (format: YYYY-MM-DD)
    const char* getCurrentFullDateISO() {
        updateDateTime();
        sprintf(dateBuffer, "%04d-%02d-%02d", dt.date.year, dt.date.month, dt.date.date);
        return dateBuffer;
    }

    void drawClock(uint32_t remainSec = 0, const char* alarmStr = nullptr) {
        static uint32_t lastRemain = 9999;
        static uint8_t lastMin = 99;
        static bool lastHadAlarm = false;
        
        updateDateTime();
        
        bool hasAlarm = (alarmStr != nullptr);
        
        if (dt.time.minutes != lastMin || remainSec != lastRemain || hasAlarm != lastHadAlarm) {
            display->clearScreen();
            
            // Display time using DisplayHandler
            display->displayMainTitle(getCurrentFullTime());
            
            // Display date
            display->displaySubtitle(getCurrentFullDateFR());
            
            // Display countdown if active
            if (remainSec > 0) {
                char timerText[32];
                sprintf(timerText, "Timer: %02u:%02u", remainSec / 60, remainSec % 60);
                display->displayInfoMessage(timerText);
            }
            
            // Alarm indicator at bottom
            if (alarmStr) {
                display->displayStatus(alarmStr, MSG_WARNING);
            }
            
            lastMin = dt.time.minutes;
            lastRemain = remainSec;
            lastHadAlarm = hasAlarm;
        }
    }

    void armTimerAndSleep(uint32_t minutes) {
        uint32_t now = rtcEpochNow();
        uint32_t target = now + minutes * 60;
        writeTarget(target);

        uint64_t us = (uint64_t)minutes * 60ULL * 1000000ULL;
        BatteryHandler::M5deepSleep(us);
    }

    void writeTarget(uint32_t epoch) {
        prefs.begin("clock", false);
        prefs.putUInt("target", epoch);
        prefs.end();
    }

    uint32_t readTarget() {
        prefs.begin("clock", true);
        uint32_t v = prefs.getUInt("target", 0);
        prefs.end();
        return v;
    }

    void clearTarget() { 
        writeTarget(0); 
    }
    
    // Get individual time components
    int getHours() { 
        updateDateTime();
        return dt.time.hours; 
    }
    
    int getMinutes() { 
        updateDateTime();
        return dt.time.minutes; 
    }
    
    int getSeconds() { 
        updateDateTime();
        return dt.time.seconds; 
    }
    
    // Get individual date components
    int getYear() { 
        updateDateTime();
        return dt.date.year; 
    }
    
    int getMonth() { 
        updateDateTime();
        return dt.date.month; 
    }
    
    int getDay() { 
        updateDateTime();
        return dt.date.date; 
    }
    
    int getWeekDay() { 
        updateDateTime();
        return dt.date.weekDay; 
    }
};

#endif