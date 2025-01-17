/*
 * -------------------------------------------------------------------
 * CircuitSetup.us Time Circuits Display
 * Code adapted from Marmoset Electronics 
 * https://www.marmosetelectronics.com/time-circuits-clock
 * by John Monaco
 * -------------------------------------------------------------------
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "tc_keypad.h"

const char keys[4][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}};

#ifdef GTE_KEYPAD
byte rowPins[4] = {5, 0, 1, 3};
byte colPins[3] = {4, 6, 2};
#else
byte rowPins[4] = {1, 6, 5, 3}; //connect to the row pinouts of the keypad
byte colPins[3] = {2, 0, 4};    //connect to the column pinouts of the keypad
#endif

Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, 4, 3, KEYPAD_ADDR, PCF8574);

bool isEnterKeyPressed = false;
bool isEnterKeyHeld = false;
bool isEnterKeyDouble = false;
bool enterWasPressed = false;
bool dateComplete = false;
bool menuFlag = false;

int enterVal = 0;
long timeNow = 0;

const int maxDateLength = 12;  //month, day, year, hour, min
const int minDateLength = 8;   //month, day, year

char dateBuffer[maxDateLength + 1];
char timeBuffer[2]; // 2 characters to accomodate date and time settings
char yearBuffer[3]; // 4 characters to accomodate year setting
char monthBuffer[2];
int dateIndex = 0;
int timeIndex = 0;
int yearIndex = 0;
int monthIndex = 0;

byte prevKeyState = HIGH;

OneButton enterKey = OneButton(ENTER_BUTTON,
  false,        // Button is active HIGH
  false         // Enable internal pull-up resistor
);

void keypad_setup() {
    keypad.begin(makeKeymap(keys));
    keypad.addEventListener(keypadEvent);  //add an event listener for this keypad
    keypad.setHoldTime(ENTER_HOLD_TIME);   // 3 sec hold time

    //setup pins for white LED and Enter key
    pinMode(WHITE_LED, OUTPUT);
    digitalWrite(WHITE_LED, LOW);  //turn on white LEDs
    pinMode(ENTER_BUTTON, INPUT_PULLDOWN);
    digitalWrite(ENTER_BUTTON, LOW);

    enterKey.attachClick(enterKeyPressed);
    enterKey.attachDuringLongPress(enterKeyHeld);
    //enterKey.attachDoubleClick(enterKeyDouble);
    enterKey.setPressTicks(ENTER_HOLD_TIME);
    enterKey.setClickTicks(ENTER_DOUBLE_TIME);
    enterKey.setDebounceTicks(DEBOUNCE);

    Serial.println("Keypad Setup Complete");
}

char get_key() {
    char getKey = keypad.getKey();
    //Serial.print("Key: ");
    //Serial.println(getKey);
    return getKey;
}

//handle different cases for keypresses
void keypadEvent(KeypadEvent key) {
    switch (keypad.getState()) {
        case PRESSED:
            if (key != '#' || key != '*') {
                play_keypad_sound(key);
                if (isSetUpdate) {
                    recordSetTimeKey(key);
                } else {
                    recordKey(key);
                }
                break;
            }
            break;
        case RELEASED:
            break;
        case HOLD:
            if (key == '0') {
                timeTravel();
                break;
            }
            break;
        case IDLE:
            break;
    }
}

void enterKeyPressed() {
    Serial.println("Enter Key Pressed");
    Serial.print("menuFlag: ");
    Serial.println(menuFlag);
    isEnterKeyPressed = true;
}

void enterKeyHeld() {
    Serial.println("Enter Key held");
    Serial.print("menuFlag: ");
    Serial.println(menuFlag);
    isEnterKeyHeld = true;
}

void enterKeyDouble() {
    Serial.println("Enter Key double click");
    isEnterKeyDouble = true;
}

void recordKey(char key) {
    dateBuffer[dateIndex++] = key;
    if (dateIndex >= maxDateLength) dateIndex = maxDateLength - 1;  // don't overflow, will overwrite end of date next time
    Serial.println(dateIndex);
}

void recordSetTimeKey(char key) {
    if (timeIndex == 0) timeBuffer[0] = key;
    if (timeIndex == 1) timeBuffer[1] = key;
    timeIndex++;
    Serial.println(timeIndex);
    if (timeIndex == 2) timeIndex = 0; // don't overflow, and return to start after buffer has 2 chars in it
}

void recordSetYearKey(char key) {
    yearBuffer[yearIndex++] = key;
    Serial.println(yearIndex);
    if (yearIndex >= 4) yearIndex = 0;  // don't overflow
}

void recordSetMonthKey(char key) {
    monthBuffer[monthIndex++] = key;
    Serial.println(monthIndex);
    if (monthIndex >= 3) monthIndex = 0;  // don't overflow
}

void keypadLoop() {
    enterKey.tick(); //manages the enter key

    if (isEnterKeyHeld && !menuFlag) {
        //if enter key is held go into settings menu
        isEnterKeyHeld = false; //reset
        isEnterKeyPressed = false;
        menuFlag = true;
        enter_menu(); //in tc_menu.cpp
    }

    if (isEnterKeyPressed && !menuFlag) {
        isEnterKeyPressed = false; //reset
        play_file("/enter.mp3", getVolume());
        digitalWrite(WHITE_LED, HIGH);  //turn on white LEDs
        destinationTime.off(); //turn off destination time only
        timeNow = millis();
        enterWasPressed = true;

        if (strlen(dateBuffer) > maxDateLength) {
            Serial.println(F("Date is too long, try again"));
            dateIndex = 0;  //reset
        } else if (strlen(dateBuffer) < minDateLength) {
            Serial.println(F("Date is too short, try again"));
            dateIndex = 0;  //reset
        } else {
            dateComplete = true;
            Serial.print(F("date entered: ["));
            Serial.print(dateBuffer);
            Serial.println(F("]"));

            String dateBufferString(dateBuffer);  //convert char to String so substring can be used

            //copy dates in dateBuffer and make ints
            int _setMonth = dateBufferString.substring(0, 2).toInt();
            int _setDay = dateBufferString.substring(2, 4).toInt();
            int _setYear = dateBufferString.substring(4, 8).toInt();
            int _setHour = dateBufferString.substring(8, 10).toInt();
            int _setMin = dateBufferString.substring(10, 12).toInt();

            //do this first in case the month entered is wrong
            if (_setMonth > 12) _setMonth = 12;
            if (_setMonth == 0) _setMonth = 1;

            //check if day makes sense for the month entered accounting for a leap year.
            if (_setDay > daysInMonth(_setMonth, _setYear)) {
                _setDay = daysInMonth(_setMonth, _setYear); //set to max day in that month
            }

            //hour and min are checked in clockdisplay

            //set date to destination time
            destinationTime.setMonth(_setMonth);
            destinationTime.setDay(_setDay);
            destinationTime.setYear(_setYear);
            destinationTime.setHour(_setHour);
            destinationTime.setMinute(_setMin);
            destinationTime.save();

            dateIndex = 0;  // prepare for next time
            dateBufferString[0] = '\0';
        }
    }
    if (isEnterKeyHeld && menuFlag) {
        isEnterKeyHeld = false; //reset
        isEnterKeyPressed = false;
        //save current setting and exit to full display
    }
    //turn everything back on after entering date
    if ((millis() > timeNow + ENTER_DELAY) && enterWasPressed) {
        destinationTime.showAnimate1();
        timeNow = millis();
        while (millis() < timeNow + 80){
            //wait 80ms
        }
        destinationTime.showAnimate2(); //turn on month
        
        digitalWrite(WHITE_LED, LOW);  //turn off white LEDs
        enterWasPressed = false;       //reset flag
    }
}