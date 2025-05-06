#pragma once

#include <Arduino.h>

class LED {
public:
    LED(uint8_t pin) : _pin(pin) { pinMode(_pin, OUTPUT); }

    void on(bool shouldFlash = false) {
        if (shouldFlash) {
            flash();
            return;
        }

        _flashing = false;
        analogWrite(_pin, _brightness);
    }

    void off() {
        _flashing = false;
        analogWrite(_pin, 0);
    }

    void setBrightness(uint8_t level) {
        if (level < 1) level = 1;
        if (level > 10) level = 10;

        _brightness = map(level, 1, 10, 25, 255);  // Scale 1–10 to 25–255
    }

    void flash() {
        _flashing       = true;
        _lastToggleTime = millis();
        _isOn           = false;
        analogWrite(_pin, 0);  // Start off
    }

    void loop() {
        if (_flashing && millis() - _lastToggleTime >= _interval) {
            _isOn = !_isOn;
            analogWrite(_pin, _isOn ? _brightness : 0);
            _lastToggleTime = millis();
        }
    }

private:
    uint8_t _pin;
    uint8_t _brightness           = 255;
    bool _flashing                = false;
    bool _isOn                    = false;
    unsigned long _interval       = 500;
    unsigned long _lastToggleTime = 0;
};
