#pragma once

#include <Arduino.h>

class LEDControl {
public:
    LEDControl(uint8_t pin) : _pin(pin) { pinMode(_pin, OUTPUT); }

    void on() { analogWrite(_pin, _brightness); }

    void off() { analogWrite(_pin, 0); }

    void setBrightness(uint8_t level) {
        if (level < 1) level = 1;
        if (level > 10) level = 10;

        _brightness = map(level, 1, 10, 25, 255);  // Scale 1–10 to 25–255
    }

private:
    uint8_t _pin;
    uint8_t _brightness = 255;  // default to max brightness
};
