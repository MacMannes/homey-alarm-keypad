#pragma once

enum class SystemState {
    UNKNOWN  = -1,
    HOME     = 0,
    AWAY     = 1,
    SLEEP    = 2,
    ALERT    = 3,
    SCHEDULE = 8,
    DISARMED = 9
};
