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

byte rowPins[ROWS] = { 13, 12, 14, 27}; // Connect to the row pinouts of the keypad
byte colPins[COLS] = {26, 25, 33};    // Connect to the column pinouts of the keypad

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
        {1, "SLAPEN"},
        {2, "AFWEZIG"},
        {8, "SCHEMA"}
};

void setState();
void getState();
void applyState();
void displayState();
void beep();
void displayKeyboardEntry(char c);
void clearKeyboardEntry();
void parseCommand(const String& command);
void savePinCodeToFlash(String& newPinCode);
void playSuccessNotes();
void playErrorNotes();

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

        switch (commandValue) {
            case 0: {
                Serial.println("Turning off the alarm");
                state = commandValue;
                applyState();
                playSuccessNotes();
                break;
            }
            case 1: {
                Serial.println("Putting the alarm into sleep mode");
                state = commandValue;
                applyState();
                playSuccessNotes();
                break;
            }
            case 2: {
                Serial.println("Putting the alarm in away mode");
                state = commandValue;
                applyState();
                playSuccessNotes();
                break;
            }
            case 8: {
                Serial.println("Putting the alarm on scheduled mode");
                state = commandValue;
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

void getState() {
    Serial.println("getState(): state is "+String(state));
    return Homey.returnResult(state);
}


void applyState() {
    Serial.println("applyState(): new state is " + String(state));
    Homey.setCapabilityValue("state", state);

    displayState();
}
