#include "LEDControl.h"

LEDControl::LEDControl(uint8_t pin) : _pin(pin) { pinMode(_pin, OUTPUT); }

void LEDControl::on() { analogWrite(_pin, _brightness); }

void LEDControl::off() { analogWrite(_pin, 0); }

void LEDControl::setBrightness(uint8_t level) {
    if (level < 1) level = 1;
    if (level > 10) level = 10;

    _brightness = map(level, 1, 10, 25, 255);  // Scale 1-10 to 25-255
}
