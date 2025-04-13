#pragma once

#include <Arduino.h>

#include <map>

enum class AlarmState {
    UNKNOWN  = -1,
    HOME     = 0,
    AWAY     = 1,
    SLEEP    = 2,
    ALERT    = 3,
    SCHEDULE = 8,
    DISARMED = 9
};

namespace AlarmStateUtils {
    static const std::map<String, AlarmState> eufyStateMapping = {
        { "home",     AlarmState::HOME     },
        { "away",     AlarmState::AWAY     },
        { "custom_1", AlarmState::SLEEP    },
        { "custom_2", AlarmState::ALERT    },
        { "schedule", AlarmState::SCHEDULE },
        { "disarmed", AlarmState::DISARMED }
    };

    inline String normalizeString(const String &input) {
        String result = input;
        result.trim();
        result.toLowerCase();
        return result;
    }

    inline AlarmState fromEufyState(const String &state) {
        String normalized = normalizeString(state);
        auto it           = eufyStateMapping.find(normalized);

        if (it == eufyStateMapping.end()) {
            Serial.print("Invalid eufy state: ");
            Serial.println(state);
            return AlarmState::UNKNOWN;
        }

        return it->second;
    }
}  // namespace AlarmStateUtils
