/*
 * Homey Alarm Keypad
 */

#include <Arduino.h>
#include <Homey.h>
#include "wifi_config.h"
#include <Keypad.h>
#include <Preferences.h>
#include "tones.h"
#include "easteregg.h"

uint8_t state = 0;

const byte ROWS = 4; // Number of rows in the keypad
const byte COLS = 3; // Number of columns in the keypad

char keys[ROWS][COLS] = {
        {'1','2','3'},
        {'4','5','6'},
        {'7','8','9'},
        {'*','0','#'}
};

byte rowPins[ROWS] = { 13, 12, 14, 27}; // Connect to the row pinouts of the keypad
byte colPins[COLS] = {26, 25, 33};    // Connect to the column pinouts of the keypad

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
Preferences preferences;

String pinCode = "0000"; // Default PIN code stored in flash memory
String currentCommand = "";

const int BUZZER_PIN = 15;

void setState();
void getState();
void applyState();
void beep();
void parseCommand(const String& command);
void savePinCodeToFlash(String& newPinCode);
void playSuccessNotes();
void playErrorNotes();

void setup() {
    Serial.begin(115200);

    pinMode(BUZZER_PIN, OUTPUT);

    // Open the preferences storage with read/write access
    preferences.begin("keypad", false);

    // Read the PIN code from flash memory
    pinCode = preferences.getString("pinCode", "0000");

    String deviceName = "keypad-" + String(ESP.getEfuseMac()); // Generate device name based on ID

    Serial.println("\n\n\nDevice: " + deviceName);

    //Connect to network
    WiFiClass::setHostname(deviceName.c_str());
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.println("Connecting WiFi");
    while (WiFiClass::status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    //Print IP address
    Serial.print("Connected. IP address: ");
    Serial.println(WiFi.localIP());

    Serial.println("Connected!");

//    Homey.begin(deviceName, "remote"); //Start Homeyduino
    Homey.begin("Alarm Keypad");
    Homey.setClass("remote");

    Homey.addAction("Set Alarm State", setState);
    Homey.addCondition("Get Alarm State", getState);


}

void loop() {
    Homey.loop();

    char key = keypad.getKey();

    if (key) {
        beep(); // Produce a beep sound

        if (key == '#') {
            Serial.println(key);
            // Process the command when # is pressed
            parseCommand(currentCommand);
            currentCommand = ""; // Clear the current command
        } else {
            Serial.print(key);
            currentCommand += key; // Append key to the current command
        }
    }
}

void parseCommand(const String& command) {
    int separatorIndex = command.indexOf('*');
    String enteredPin = "";

    if (separatorIndex != -1) {
        // Extract PIN code
        enteredPin = command.substring(0, separatorIndex);
    } else {
        enteredPin = command;
    }

    if (enteredPin.equals(pinCode)) {
        // Check if entered PIN is correct

        // Set the default command value to 0
        int commandValue = 0;

        // Check if there is a numeric command after the * character
        if (separatorIndex < command.length() - 1) {
            // Extract numeric command after the * character
            String numericCommand = command.substring(separatorIndex + 1);
            commandValue = numericCommand.toInt();
        }

        switch (commandValue) {
            case 0: {
                Serial.println("Turning off the alarm");
                state = 0;
                applyState();
                playSuccessNotes();
                break;
            }
            case 1: {
                Serial.println("Putting the alarm into sleep mode");
                state = 1;
                applyState();
                playSuccessNotes();
                break;
            }
            case 2: {
                Serial.println("Putting the alarm in away mode");
                state = 1;
                applyState();
                playSuccessNotes();
                break;
            }
            case 8: {
                Serial.println("Putting the alarm on scheduled mode");
                state = 8;
                applyState();
                playSuccessNotes();
                break;
            }
            case 99: {
                // Change the PIN code
                int newPinIndex = command.indexOf('*', separatorIndex + 1);
                if (newPinIndex != -1) {
                    String newPin = command.substring(newPinIndex + 1);
                    pinCode = newPin;
                    Serial.print("PIN code changed to: ");
                    Serial.println(pinCode);

                    // Save the new PIN code to flash memory
                    savePinCodeToFlash(pinCode);
                } else {
                    Serial.println("Invalid command format");
                }

                playSuccessNotes();
                playSuccessNotes();

                break;
            }
            case 1990: {
                Serial.println("Playing Monkey Island Theme :-D");
                playMonkeyIslandTune(BUZZER_PIN);
                break;
            }
            default:
                Serial.println("Invalid command");
                playErrorNotes();
        }
    } else {
        Serial.println("Incorrect PIN code");
        playErrorNotes();
    }
}


void savePinCodeToFlash(String &newPinCode) {
    // Write the new PIN code to flash memory
    preferences.putString("pinCode", newPinCode);
    preferences.end();
}


void beep() {
    tone(BUZZER_PIN, NOTE_B3, 50);
}

void playSuccessNotes() {
    tone(BUZZER_PIN, NOTE_C5, 100);
    tone(BUZZER_PIN, NOTE_D5, 100);
    tone(BUZZER_PIN, NOTE_F5, 100);
    tone(BUZZER_PIN, NOTE_G5, 100);
}

void playErrorNotes() {
    tone(BUZZER_PIN, NOTE_A2, 500);
}

void setState() {
    state = Homey.value.toInt();
    applyState();
}

void getState() {
    Serial.println("getState(): state is "+String(state));
    return Homey.returnResult(state);
}


void applyState() {
    Serial.println("applyState(): new state is " + String(state));
    Homey.setCapabilityValue("state", state);
}
