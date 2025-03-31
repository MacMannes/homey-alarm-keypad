#ifndef LEDCONTROL_H
#define LEDCONTROL_H

#include <Arduino.h>

class LEDControl {
   public:
    explicit LEDControl(uint8_t pin);
    void on();
    void off();
    void setBrightness(uint8_t level);  // Level from 1 to 10

   private:
    uint8_t _pin;
    uint8_t _brightness;
};

#endif  // LEDCONTROL_H
