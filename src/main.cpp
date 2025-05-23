/*
 * Homey Alarm Keypad
 */

#include <Adafruit_PCD8544.h>
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <Homey.h>
#include <Keypad.h>
#include <Preferences.h>

#include <map>

#include "Backlight.hpp"
#include "HardwareSerial.h"
#include "States.h"
#include "StatusLEDs.hpp"
#include "UptimeFormatter.hpp"
#include "WifiService.hpp"
#include "config.h"
#include "easteregg.hpp"
#include "tones.h"

AsyncWebServer server(80);

WifiService *wifiService;
StatusLEDs statusLEDs;

uint8_t state          = 0;
bool stateAcknowledged = false;

const uint8_t KEYBOARD_ENTRY_ROW    = 35;
const uint8_t KEYBOARD_ENTRY_COLUMN = 0;

uint8_t display_x = KEYBOARD_ENTRY_COLUMN;
uint8_t display_y = KEYBOARD_ENTRY_ROW;

bool shouldHideKeyboardEntry = true;

const byte ROWS = 4;
const byte COLS = 3;

char keys [ROWS][COLS] = {
    { '1', '2', '3' },
    { '4', '5', '6' },
    { '7', '8', '9' },
    { '*', '0', '#' }
};

#ifdef KEYPAD_1
byte rowPins [ROWS] = { 13, 12, 14, 27 };
byte colPins [COLS] = { 26, 25, 33 };
#elif defined(KEYPAD_2)
byte rowPins [ROWS] = { 12, 33, 25, 27 };
byte colPins [COLS] = { 14, 13, 26 };
#endif

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

LED backlightLed(BL_PIN);
Backlight backlight(backlightLed);

// Define constants for state values

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
void playAcknowledgeNotes();
void playErrorNotes();
void triggerHomey();
void requestAlarmStateFromHomey();
void displayLEDState();

int mapEufyState(const String &state);

void changeAlarmState(int newState, const char *message);
void handlePinChange(const String &command);
void playMonkeyIslandTheme();
void displayInfo();
void invalidCommand();
void executeCommand(int commandValue, const String &command);

constexpr int CMD_HOME       = 0;
constexpr int CMD_AWAY       = 1;
constexpr int CMD_SLEEP      = 2;
constexpr int CMD_ALERT      = 3;
constexpr int CMD_SCHEDULE   = 8;
constexpr int CMD_DISARM     = 9;
constexpr int CMD_INFO       = 77;
constexpr int CMD_REBOOT     = 88;
constexpr int CMD_CHANGE_PIN = 99;
constexpr int CMD_EASTER_EGG = 1990;

// Lookup table for command handlers
const std::map<int, std::function<void()>> commandHandlers = {
    { CMD_HOME,       []() { changeAlarmState(HOME, "Turning off the alarm"); }                   },
    { CMD_AWAY,       []() { changeAlarmState(AWAY, "Putting the alarm in away mode"); }          },
    { CMD_SLEEP,      []() { changeAlarmState(SLEEP, "Putting the alarm into sleep mode"); }      },
    { CMD_ALERT,      []() { changeAlarmState(ALERT, "Putting the alarm into alert mode"); }      },
    { CMD_SCHEDULE,   []() { changeAlarmState(SCHEDULE, "Putting the alarm on scheduled mode"); } },
    { CMD_REBOOT,     esp_restart                                                                 },
    { CMD_INFO,       displayInfo                                                                 },
    { CMD_EASTER_EGG, playMonkeyIslandTheme                                                       }
};

void setup() {
    String deviceName = "keypad-" + String(ESP.getEfuseMac());

    Serial.begin(115200);
    display.begin();
    display.setContrast(60);

    backlightLed.setBrightness(8);
    backlight.begin();

    wifiService = new WifiService(deviceName, display);
    wifiService->connect();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String responseMessage = "Keypad is online!\n\n";
        responseMessage += "Build Date: " + String(__DATE__) + "\n";
        responseMessage += "Build Time: " + String(__TIME__) + "\n\n";

        responseMessage += "Uptime: " + UptimeFormatter::getUptime();

        request->send(200, "text/plain", responseMessage);
    });

    ElegantOTA.begin(&server);
    server.begin();
    Serial.println("HTTP server started");

    pinMode(BUZZER_PIN, OUTPUT);

    statusLEDs.setBrightness(7);

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
    backlight.update();
    ElegantOTA.loop();
    statusLEDs.loop();

    char key = keypad.getKey();

    if (key) {
        backlight.registerActivity();

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
        displayState();
    }
}

void savePinCodeToFlash(String &newPinCode) {
    // Write the new PIN code to flash memory
    preferences.putString("pinCode", newPinCode);
    preferences.end();
}

void beep() { tone(BUZZER_PIN, NOTE_B3, 50); }

void clearKeyboardEntry() {
    display.fillRect(KEYBOARD_ENTRY_COLUMN, KEYBOARD_ENTRY_ROW, 84, 48, WHITE);
    display.display();
    display_y = KEYBOARD_ENTRY_ROW;
    display_x = KEYBOARD_ENTRY_COLUMN;
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

    int16_t x = 42;
    int16_t y = 15;
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
    display.setTextColor(BLACK);
    display.print(statusText);

    // Display the text on the screen
    display.display();

    statusLEDs.setState(state, stateAcknowledged);
}

void displayInfo() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(BLACK);
    display.setCursor(0, 0);

    IPAddress ip = WiFi.localIP();
    String ipStr = ip.toString();
    display.println(ipStr);

    display.println("\nBuild Date:");
    display.println(__DATE__);
    display.println(__TIME__);

    display.display();

    delay(5000);
    requestAlarmStateFromHomey();
}

void playSuccessNotes() {
    tone(BUZZER_PIN, NOTE_C5, 100);
    tone(BUZZER_PIN, NOTE_D5, 100);
    tone(BUZZER_PIN, NOTE_F5, 100);
    tone(BUZZER_PIN, NOTE_G5, 100);
}

void playAcknowledgeNotes() {
    tone(BUZZER_PIN, NOTE_G5, 150);
    tone(BUZZER_PIN, NOTE_F5, 200);
}

void playErrorNotes() { tone(BUZZER_PIN, NOTE_A2, 500); }

void setState() {
    state = Homey.value.toInt();
    applyState();

    displayState();
}

void handleEufyStateChange() {
    Serial.println("Eufy changed Alarm State to " + Homey.value);

    int newState = mapEufyState(Homey.value);
    if (newState < 0) return;

    state             = newState;
    stateAcknowledged = true;

    displayState();
    playAcknowledgeNotes();
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
    displayState();
    triggerHomey();
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

void requestAlarmStateFromHomey() {
    Serial.println("Requesting alarm state from Homey");
    Homey.trigger("getAlarmState");
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
    state             = newState;
    stateAcknowledged = false;
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
    displayState();
}
