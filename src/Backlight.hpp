#pragma once

#include <Arduino.h>

#include "LED.hpp"

class Backlight {
public:
    Backlight(LED& led, unsigned long timeoutMs = 60000)
        : _led(led), _timeout(timeoutMs), _lastActivity(0), _isOn(false) {}

    void begin() {
        _led.off();
        _isOn         = false;
        _lastActivity = millis();
    }

    void registerActivity() {
        _lastActivity = millis();
        if (!_isOn) {
            _led.on();
            _isOn = true;
        }
    }

    void update() {
        if (_isOn && (millis() - _lastActivity >= _timeout)) {
            _led.off();
            _isOn = false;
        }
    }

private:
    LED& _led;
    unsigned long _timeout;
    unsigned long _lastActivity;
    bool _isOn;
};
