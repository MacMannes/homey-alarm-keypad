#pragma once

#include <Arduino.h>

class UptimeFormatter {
public:
    static String getUptime() {
        unsigned long ms = millis();

        unsigned long seconds = ms / 1000;
        unsigned long minutes = seconds / 60;
        unsigned long hours   = minutes / 60;
        unsigned long days    = hours / 24;

        seconds %= 60;
        minutes %= 60;
        hours %= 24;

        String result = "";

        if (days > 0) {
            result += String(days) + " day";
            if (days > 1) result += "s";
            result += ", ";
        }
        if (hours > 0 || days > 0) {
            result += String(hours) + " hour";
            if (hours != 1) result += "s";
            result += ", ";
        }
        if (minutes > 0 || hours > 0 || days > 0) {
            result += String(minutes) + " minute";
            if (minutes != 1) result += "s";
            result += ", ";
        }

        result += String(seconds) + " second";
        if (seconds != 1) result += "s";

        return result;
    }
};
