#ifndef ALARM_HANDLER_H
#define ALARM_HANDLER_H

#include <M5Unified.h>
#include <Preferences.h>

class AlarmHandler {
private:
    Preferences prefs;
    uint8_t alarmHour;
    uint8_t alarmMinute;
    bool enabled;
    bool ringing;
    // Guard to avoid re-firing during the same minute
    uint8_t firedH;
    uint8_t firedM;

public:
    AlarmHandler() : ringing(false), firedH(99), firedM(99) {
        load();
    }

    void load() {
        prefs.begin("alarm", true);
        alarmHour   = prefs.getUChar("hour", 7);
        alarmMinute = prefs.getUChar("min", 0);
        enabled     = prefs.getBool("on", false);
        prefs.end();
    }

    void save() {
        prefs.begin("alarm", false);
        prefs.putUChar("hour", alarmHour);
        prefs.putUChar("min", alarmMinute);
        prefs.putBool("on", enabled);
        prefs.end();
    }

    void setTime(uint8_t h, uint8_t m) {
        alarmHour = h;
        alarmMinute = m;
        enabled = true;
        firedH = 99; // reset guard so it can fire at the new time
        save();
    }

    void setEnabled(bool v) {
        enabled = v;
        if (v) { firedH = 99; firedM = 99; }
        save();
    }

    bool     isEnabled()  const { return enabled; }
    bool     isRinging()  const { return ringing; }
    uint8_t  getHour()    const { return alarmHour; }
    uint8_t  getMinute()  const { return alarmMinute; }

    void getTimeStr(char* buf, size_t len) const {
        snprintf(buf, len, "%02d:%02d", alarmHour, alarmMinute);
    }

    // Call every second from the page loop.
    // Returns true the instant the alarm should start ringing.
    bool check(uint8_t curH, uint8_t curM) {
        if (!enabled || ringing) return false;
        if (curH == alarmHour && curM == alarmMinute) {
            if (firedH == curH && firedM == curM) return false;
            firedH = curH;
            firedM = curM;
            ringing = true;
            return true;
        }
        return false;
    }

    void dismiss() { ringing = false; }
};

#endif
