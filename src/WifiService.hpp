#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "Adafruit_PCD8544.h"
#include "wifi_config.h"

class WifiService {
public:
    WifiService(const String &deviceName, Adafruit_PCD8544 &displayRef)
        : deviceName(deviceName), display(displayRef) {}

    void connect() {
        WiFiClass::setHostname(deviceName.c_str());
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(BLACK);
        display.setCursor(0, 0);

        Serial.print("Connecting WiFi");
        display.print("Connecting");
        display.display();

        while (WiFiClass::status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
            display.print(".");
            display.display();
        }

        IPAddress ip = WiFi.localIP();
        String ipStr = ip.toString();

        Serial.print("Connected. IP address: ");
        Serial.println(ipStr);
    }

private:
    String deviceName;
    Adafruit_PCD8544 &display;
};
