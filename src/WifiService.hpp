#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "wifi_config.h"

class WifiService {
public:
    WifiService(const String &deviceName) : deviceName(deviceName) {}

    void connect() {
        WiFiClass::setHostname(deviceName.c_str());
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        Serial.println("Connecting WiFi");

        while (WiFiClass::status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }

        Serial.print("Connected. IP address: ");
        Serial.println(WiFi.localIP());

        Serial.println("Connected!");
    }

private:
    String deviceName;
};
