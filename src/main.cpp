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
#include <Adafruit_PCD8544.h>
#include <map>

uint8_t state = 0;

uint8_t display_x = 0;
uint8_t display_y = 20;

bool shouldHideKeyboardEntry = true;

const byte ROWS = 4; // Number of rows in the keypad
const byte COLS = 3; // Number of columns in the keypad

char keys[ROWS][COLS] = {
        {'1','2','3'},
        {'4','5','6'},
        {'7','8','9'},
        {'*','0','#'}
};

byte rowPins[ROWS] = { 13, 12, 14, 27 }; // Connect to the row pinouts of the keypad
byte colPins[COLS] = { 26, 25, 33 };    // Connect to the column pinouts of the keypad

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
Preferences preferences;


String pinCode = "0000"; // Default PIN code stored in flash memory
String currentCommand = "";

const int BUZZER_PIN = 15;

// Pin configuration for Nokia 5110 display
#define RST_PIN   16  // Reset
#define CE_PIN    17  // Chip Enable
#define DC_PIN    2   // Data/Command
#define DIN_PIN   23  // Data In (MOSI)
#define CLK_PIN   18  // Clock
#define BL_PIN    22  // Backlight

Adafruit_PCD8544 display = Adafruit_PCD8544(CLK_PIN, DIN_PIN, DC_PIN, CE_PIN,RST_PIN );

struct KeyValue {
    int key;
    const char *value;
};

struct KeyValue stateTexts[] = {
        {0, "THUIS"},
        {1, "AFWEZIG"},
        {2, "SLAPEN"},
        {3, "ALERT"},
        {8, "SCHEMA"}
};

// Define constants for state values
constexpr int HOME = 0;
constexpr int AWAY = 1;
constexpr int SLEEP = 2;
constexpr int ALERT = 3;
constexpr int SCHEDULE = 8;

// Lookup table for string-to-numeric state mapping
const std::map<String, int> eufyStateMapping = {
    {"home", HOME},
    {"away", AWAY},
    {"custom_1", SLEEP},
    {"custom_2", ALERT},
    {"schedule", SCHEDULE}
};

// Lookup table for state-to-trigger mapping
const std::map<int, String> stateTriggerMapping = {
    {HOME, "setAlarmToHome"},
    {AWAY, "setAlarmToAway"},
    {SLEEP, "setAlarmToSleep"},
    {ALERT, "setAlarmToAlert"},
    {SCHEDULE, "setAlarmToSchedule"}
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
void parseCommand(const String& command);
void savePinCodeToFlash(String& newPinCode);
void playSuccessNotes();
void playErrorNotes();
void triggerHomey();

int mapEufyState(const String& state);

void changeAlarmState(int newState, const char* message);
void handlePinChange(const String& command, int separatorIndex);
void playMonkeyIslandTheme();
void invalidCommand();
void executeCommand(int commandValue);


// Lookup table for command handlers
const std::map<int, std::function<void()>> commandHandlers = {
    {HOME,      []() { changeAlarmState(HOME, "Turning off the alarm"); }},
    {AWAY,      []() { changeAlarmState(AWAY, "Putting the alarm in away mode"); }},
    {SLEEP,     []() { changeAlarmState(SLEEP, "Putting the alarm into sleep mode"); }},
    {ALERT,     []() { changeAlarmState(ALERT, "Putting the alarm into alert mode"); }},
    {SCHEDULE,  []() { changeAlarmState(SCHEDULE, "Putting the alarm on scheduled mode"); }},
    {99,        []() { handlePinChange("your_command_string_here", 0); }},
    {1990,      playMonkeyIslandTheme}
};

void setup() {
    Serial.begin(115200);

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BL_PIN, OUTPUT);
    digitalWrite(BL_PIN, HIGH);

    display.begin();
    display.setContrast(60);
    displayState();

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

    Homey.begin("Alarm Keypad");
    Homey.setClass("remote");

    Homey.addAction("Set Alarm State", setState);
    Homey.addAction("handleEufyStateChange",  handleEufyStateChange);
    Homey.addCondition("Get Alarm State", getState);
}

void loop() {
    Homey.loop();

    char key = keypad.getKey();

    if (key) {
        beep(); // Produce a beep sound
        displayKeyboardEntry((shouldHideKeyboardEntry) ? '*' : key);
        Serial.println(key);

        if (key == '*') {
            shouldHideKeyboardEntry = false;
        }

        if (key == '#') {
            clearKeyboardEntry();
            // Process the command when # is pressed
            parseCommand(currentCommand);
            currentCommand = ""; // Clear the current command
            shouldHideKeyboardEntry = true;
        } else {
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

    Serial.print("Entered PIN code: ");
    Serial.println(pinCode);

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

        executeCommand(commandValue);
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

void clearKeyboardEntry() {
    display.fillRect(0, 20, 84, 48, WHITE);
    display.display();
    display_x = 0;
    display_y = 20;
}

void displayKeyboardEntry(char c) {
    display.setTextSize(1);
    display.setTextColor(BLACK);
    display.setCursor(display_x,display_y);
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
    size_t arraySize = sizeof(stateTexts) / sizeof(stateTexts[0]);

    const char *statusText = "ERROR";

    // Search the array for the entry for the current state
    for (size_t i = 0; i < arraySize; ++i) {
        if (stateTexts[i].key == state) {
            statusText = stateTexts[i].value;
            break;
        }
    }

    display.getTextBounds(statusText, x, y, &x1, &y1, &w, &h); //calc width of new string
    display.setCursor(x - w / 2, y);
    display.setTextColor(WHITE);
    display.print(statusText);

    // Display the text on the screen
    display.display();
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
    Serial.println("getState(): state is "+String(state));
    return Homey.returnResult(state);
}

String normalizeString(const String& input) {
    String result = input;
    result.trim();   // Remove leading/trailing spaces
    result.toLowerCase(); // Convert to lowercase
    return result;
}

int mapEufyState(const String& state) {
    String normalizedState = normalizeString(state);

    auto it = eufyStateMapping.find(normalizedState);
    if (it == eufyStateMapping.end()) {
        Serial.print("Invalid eufy state: ");
        Serial.println(state);
        return -1; 
    }
    return it->second;
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

void executeCommand(int commandValue) {
    auto it = commandHandlers.find(commandValue);
    if (it == commandHandlers.end()) {
        invalidCommand();
        return;
    }
    it->second(); // Call the function mapped to the command
}

void changeAlarmState(int newState, const char* message) {
    Serial.println(message);
    state = newState;
    applyState();
    playSuccessNotes();
}

void handlePinChange(const String& command, int separatorIndex) {
    int newPinIndex = command.indexOf('*', separatorIndex + 1);
    if (newPinIndex != -1) {
        String newPin = command.substring(newPinIndex + 1);
        pinCode = newPin;
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
    playMonkeyIslandTune(9); // Replace with actual buzzer pin
}

void invalidCommand() {
    Serial.println("Invalid command");
    playErrorNotes();
}

