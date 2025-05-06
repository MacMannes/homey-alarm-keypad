#pragma once

#include "AlarmState.hpp"
#include "LED.hpp"

class StatusLEDs {
public:
    StatusLEDs() : redLED(RED_LED_PIN), orangeLED(ORANGE_LED_PIN), greenLED(GREEN_LED_PIN) {}

    void init() {}

    void setState(int state, bool acknowledged = false) {
        setState(static_cast<AlarmState>(state), acknowledged);
    }

    void setState(AlarmState state, bool acknowledged) {
        redLED.off();
        orangeLED.off();
        greenLED.off();

        if (state == AlarmState::AWAY || state == AlarmState::SLEEP) {
            Serial.println("RED on");
            redLED.on(!acknowledged);
            return;
        }

        if (state == AlarmState::ALERT || state == AlarmState::SCHEDULE) {
            Serial.println("ORANGE on");
            orangeLED.on(!acknowledged);
            return;
        }

        Serial.println("GREEN on");
        greenLED.on(!acknowledged);
    }

    void setBrightness(int brightness) {
        redLED.setBrightness(brightness);
        orangeLED.setBrightness(brightness);
        greenLED.setBrightness(brightness);
    }

    void loop() {
        redLED.loop();
        orangeLED.loop();
        greenLED.loop();
    }

private:
    static constexpr int RED_LED_PIN    = 32;
    static constexpr int ORANGE_LED_PIN = 4;
    static constexpr int GREEN_LED_PIN  = 5;

    LED redLED;
    LED orangeLED;
    LED greenLED;
};
