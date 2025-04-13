/*
 * Homey Alarm Keypad
 */

#include <Adafruit_PCD8544.h>
#include <Arduino.h>
#include <Homey.h>
#include <Keypad.h>
#include <Preferences.h>

#include <map>

#include "HardwareSerial.h"
#include "LEDControl.hpp"
#include "WifiService.hpp"
#include "easteregg.hpp"
#include "tones.h"

WifiService *wifiService;

uint8_t state = 0;

uint8_t display_x = 0;
uint8_t display_y = 20;

bool shouldHideKeyboardEntry = true;

const byte ROWS = 4;
const byte COLS = 3;

char keys [ROWS][COLS] = {
    { '1', '2', '3' },
    { '4', '5', '6' },
    { '7', '8', '9' },
    { '*', '0', '#' }
};

byte rowPins [ROWS] = { 13, 12, 14, 27 };
byte colPins [COLS] = { 26, 25, 33 };

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
Preferences preferences;

String pinCode        = "0000";  // Default PIN code stored in flash memory
String currentCommand = "";

const int BUZZER_PIN = 15;

// Pin configuration for Nokia 5110 display
#define RST_PIN 16  // Reset
#define CE_PIN  17  // Chip Enable
#define DC_PIN  2   // Data/Command
#define DIN_PIN 23  // Data In (MOSI)
#define CLK_PIN 18  // Clock
#define BL_PIN  22  // Backlight

Adafruit_PCD8544 display = Adafruit_PCD8544(CLK_PIN, DIN_PIN, DC_PIN, CE_PIN, RST_PIN);

constexpr int RED_LED_PIN    = 32;
constexpr int ORANGE_LED_PIN = 4;
constexpr int GREEN_LED_PIN  = 5;

LEDControl redLED    = LEDControl(RED_LED_PIN);
LEDControl orangeLED = LEDControl(ORANGE_LED_PIN);
LEDControl greenLED  = LEDControl(GREEN_LED_PIN);

// Define constants for state values
constexpr int HOME     = 0;
constexpr int AWAY     = 1;
constexpr int SLEEP    = 2;
constexpr int ALERT    = 3;
constexpr int SCHEDULE = 8;
constexpr int DISARMED = 9;

struct KeyValue {
    int key;
    const char *value;
};

struct KeyValue stateTexts [] = {
    { HOME,     "THUIS"    },
    { AWAY,     "AFWEZIG"  },
    { SLEEP,    "SLAPEN"   },
    { ALERT,    "ALERT"    },
    { SCHEDULE, "SCHEMA"   },
    { DISARMED, "DISARMED" }
};

// Lookup table for string-to-numeric state mapping
const std::map<String, int> eufyStateMapping = {
    { "home",     HOME     },
    { "away",     AWAY     },
    { "custom_1", SLEEP    },
    { "custom_2", ALERT    },
    { "schedule", SCHEDULE },
    { "disarmed", DISARMED }
};

// Lookup table for state-to-trigger mapping
const std::map<int, String> stateTriggerMapping = {
    { HOME,     "setAlarmToHome"     },
    { AWAY,     "setAlarmToAway"     },
    { SLEEP,    "setAlarmToSleep"    },
    { ALERT,    "setAlarmToAlert"    },
    { SCHEDULE, "setAlarmToSchedule" }
};

// Function prototypes
void setState();
void getState();
void applyState();
void displayState();
void handleEufyStateChange();
void beep();
void displayKeyboardEntry(char c);
void clearKeyboardEntry();
void parseCommand(const String &command);
void savePinCodeToFlash(String &newPinCode);
void playSuccessNotes();
void playErrorNotes();
void triggerHomey();
void setLEDBrightness(int brightness);
void displayLEDState();

int mapEufyState(const String &state);

void changeAlarmState(int newState, const char *message);
void handlePinChange(const String &command);
void playMonkeyIslandTheme();
void invalidCommand();
void executeCommand(int commandValue, const String &command);

constexpr int CMD_HOME       = 0;
constexpr int CMD_AWAY       = 1;
constexpr int CMD_SLEEP      = 2;
constexpr int CMD_ALERT      = 3;
constexpr int CMD_SCHEDULE   = 8;
constexpr int CMD_DISARM     = 9;
constexpr int CMD_CHANGE_PIN = 99;
constexpr int CMD_EASTER_EGG = 1990;

// Lookup table for command handlers
const std::map<int, std::function<void()>> commandHandlers = {
    { CMD_HOME,       []() { changeAlarmState(HOME, "Turning off the alarm"); }                   },
    { CMD_AWAY,       []() { changeAlarmState(AWAY, "Putting the alarm in away mode"); }          },
    { CMD_SLEEP,      []() { changeAlarmState(SLEEP, "Putting the alarm into sleep mode"); }      },
    { CMD_ALERT,      []() { changeAlarmState(ALERT, "Putting the alarm into alert mode"); }      },
    { CMD_SCHEDULE,   []() { changeAlarmState(SCHEDULE, "Putting the alarm on scheduled mode"); } },
    { CMD_EASTER_EGG, playMonkeyIslandTheme                                                       }
};

void setup() {
    String deviceName = "keypad-" + String(ESP.getEfuseMac());

    Serial.begin(115200);
    display.begin();

    wifiService = new WifiService(deviceName);
    wifiService->connect();

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BL_PIN, OUTPUT);
    digitalWrite(BL_PIN, HIGH);

    setLEDBrightness(7);

    display.setContrast(60);
    displayState();

    // Open the preferences storage with read/write access
    preferences.begin("keypad", false);

    // Read the PIN code from flash memory
    pinCode = preferences.getString("pinCode", "0000");

    Homey.begin("Alarm Keypad");
    Homey.setClass("remote");

    Homey.addAction("Set Alarm State", setState);
    Homey.addAction("handleEufyStateChange", handleEufyStateChange);
    Homey.addCondition("Get Alarm State", getState);
}

void loop() {
    Homey.loop();

    char key = keypad.getKey();

    if (key) {
        beep();  // Produce a beep sound
        displayKeyboardEntry((shouldHideKeyboardEntry) ? '*' : key);
        Serial.println(key);

        if (key == '*') {
            shouldHideKeyboardEntry = false;
        }

        if (key == '#') {
            clearKeyboardEntry();
            // Process the command when # is pressed
            parseCommand(currentCommand);
            currentCommand          = "";  // Clear the current command
            shouldHideKeyboardEntry = true;
        } else {
            currentCommand += key;  // Append key to the current command
        }
    }
}

void parseCommand(const String &command) {
    int separatorIndex = command.indexOf('*');
    String enteredPin  = "";

    if (separatorIndex != -1) {
        // Extract PIN code
        enteredPin = command.substring(0, separatorIndex);
    } else {
        enteredPin = command;
    }

    Serial.print("Entered PIN code: ");
    Serial.println(pinCode);

    if (enteredPin.equals(pinCode)) {
        // Set the default command value to 0
        int commandValue = 0;

        // Check if there is a numeric command after the * character
        if (separatorIndex < command.length() - 1) {
            // Extract numeric command after the * character
            String numericCommand = command.substring(separatorIndex + 1);
            commandValue          = numericCommand.toInt();
        }

        executeCommand(commandValue, command);
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

void beep() { tone(BUZZER_PIN, NOTE_B3, 50); }

void clearKeyboardEntry() {
    display.fillRect(0, 20, 84, 48, WHITE);
    display.display();
    display_x = 0;
    display_y = 20;
}

void displayKeyboardEntry(char c) {
    display.setTextSize(1);
    display.setTextColor(BLACK);
    display.setCursor(display_x, display_y);
    display.print(c);
    display.display();
    display_x += 6;
}

void displayState() {
    display.clearDisplay();  // Clear the display

    display.fillRect(0, 0, 84, 11, BLACK);

    int16_t x = 42;
    int16_t y = 2;
    int16_t x1, y1;
    uint16_t w, h;

    // Determine the size of the array
    size_t arraySize = sizeof(stateTexts) / sizeof(stateTexts [0]);

    const char *statusText = "ERROR";

    // Search the array for the entry for the current state
    for (size_t i = 0; i < arraySize; ++i) {
        if (stateTexts [i].key == state) {
            statusText = stateTexts [i].value;
            break;
        }
    }

    // Calculate width of new string
    display.getTextBounds(statusText, x, y, &x1, &y1, &w, &h);
    display.setCursor(x - w / 2, y);
    display.setTextColor(WHITE);
    display.print(statusText);

    // Display the text on the screen
    display.display();

    displayLEDState();
}

void displayLEDState() {
    redLED.off();
    orangeLED.off();
    greenLED.off();

    if (state == AWAY || state == SLEEP) {
        Serial.println("RED on");
        redLED.on();
        return;
    }

    if (state == ALERT || state == SCHEDULE) {
        Serial.println("ORANGE on");
        orangeLED.on();
        return;
    }

    Serial.println("GREEN on");
    greenLED.on();
}

void playSuccessNotes() {
    tone(BUZZER_PIN, NOTE_C5, 100);
    tone(BUZZER_PIN, NOTE_D5, 100);
    tone(BUZZER_PIN, NOTE_F5, 100);
    tone(BUZZER_PIN, NOTE_G5, 100);
}

void playErrorNotes() { tone(BUZZER_PIN, NOTE_A2, 500); }

void setLEDBrightness(int brightness) {
    redLED.setBrightness(brightness);
    orangeLED.setBrightness(brightness);
    greenLED.setBrightness(brightness);
}

void setState() {
    state = Homey.value.toInt();
    applyState();

    displayState();
}

void handleEufyStateChange() {
    Serial.println("Eufy changed Alarm State to " + Homey.value);

    int newState = mapEufyState(Homey.value);
    if (newState < 0) return;

    state = newState;
    displayState();
    playSuccessNotes();
}

void getState() {
    Serial.println("getState(): state is " + String(state));
    return Homey.returnResult(state);
}

String normalizeString(const String &input) {
    String result = input;
    result.trim();         // Remove leading/trailing spaces
    result.toLowerCase();  // Convert to lowercase
    return result;
}

int mapEufyState(const String &state) {
    String normalizedState = normalizeString(state);

    auto eufyState = eufyStateMapping.find(normalizedState);
    if (eufyState == eufyStateMapping.end()) {
        Serial.print("Invalid eufy state: ");
        Serial.println(state);
        return -1;
    }
    return eufyState->second;
}

void applyState() {
    Serial.println("applyState(): new state is " + String(state));
    Homey.setCapabilityValue("state", state);
    triggerHomey();
    displayState();
}

void triggerHomey() {
    auto triggerMapping = stateTriggerMapping.find(state);
    if (triggerMapping == stateTriggerMapping.end()) {
        return;
    }

    String trigger = triggerMapping->second;
    Serial.println("Trigger Homey: " + trigger);
    Homey.trigger(trigger, true);
}

void executeCommand(int commandValue, const String &command) {
    if (commandValue == CMD_CHANGE_PIN) {
        handlePinChange(command);
        return;
    }

    auto commandHandler = commandHandlers.find(commandValue);
    if (commandHandler == commandHandlers.end()) {
        invalidCommand();
        return;
    }
    commandHandler->second();  // Call the function mapped to the command
}

void changeAlarmState(int newState, const char *message) {
    Serial.println(message);
    state = newState;
    applyState();
    playSuccessNotes();
}

void handlePinChange(const String &command) {
    int positionOfFirstStar = command.indexOf('*');
    int newPinIndex         = command.indexOf('*', positionOfFirstStar + 1);
    if (newPinIndex != -1) {
        String newPin = command.substring(newPinIndex + 1);
        pinCode       = newPin;
        Serial.print("PIN code changed to: ");
        Serial.println(pinCode);
        savePinCodeToFlash(pinCode);
        playSuccessNotes();
        playSuccessNotes();
        return;
    }
    Serial.println("Invalid command format");
}

void playMonkeyIslandTheme() {
    Serial.println("Playing Monkey Island Theme :-D");
    playMonkeyIslandTune(BUZZER_PIN);
}

void invalidCommand() {
    Serial.println("Invalid command");
    playErrorNotes();
}
