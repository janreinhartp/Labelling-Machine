#include <Arduino.h>
#include "PCF8575.h"
PCF8575 pcf8575(0x22);
// Declaration for LCD
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 20, 4);
#include "control.h"
#include <Preferences.h>

Preferences Settings;
byte enterChar[] = {
    B10000,
    B10000,
    B10100,
    B10110,
    B11111,
    B00110,
    B00100,
    B00000};

byte fastChar[] = {
    B00110,
    B01110,
    B00110,
    B00110,
    B01111,
    B00000,
    B00100,
    B01110};
byte slowChar[] = {
    B00011,
    B00111,
    B00011,
    B11011,
    B11011,
    B00000,
    B00100,
    B01110};

// Declaration of LCD Variables
const int NUM_MAIN_ITEMS = 3;
const int NUM_SETTING_ITEMS = 2;
const int NUM_TESTMACHINE_ITEMS = 4;

int currentMainScreen;
int currentSettingScreen;
int currentTestMenuScreen;
bool settingFlag, settingEditFlag, testMenuFlag, runAutoFlag, refreshScreen = false;

bool CutterStatusSensor, LinearStatusSensor = false;

String menu_items[NUM_MAIN_ITEMS][2] = { // array with item names
    {"SETTING", "ENTER TO EDIT"},
    {"TEST MACHINE", "ENTER TO TEST"},
    {"RUN AUTO", "ENTER TO RUN AUTO"}};

String setting_items[NUM_SETTING_ITEMS][2] = { // array with item names
    {"LENGTH", "MILLIS"},
    {"SAVE"}};

int parametersTimer[NUM_SETTING_ITEMS] = {1};
int parametersTimerMaxValue[NUM_SETTING_ITEMS] = {60000};

String testmachine_items[NUM_TESTMACHINE_ITEMS] = { // array with item names
    "CONVEYOR",
    "STICKER ROLLER",
    "BOTTLE ROLLER",
    "EXIT"};

char *secondsToHHMMSS(int total_seconds)
{
  int hours, minutes, seconds;

  hours = total_seconds / 3600;         // Divide by number of seconds in an hour
  total_seconds = total_seconds % 3600; // Get the remaining seconds
  minutes = total_seconds / 60;         // Divide by number of seconds in a minute
  seconds = total_seconds % 60;         // Get the remaining seconds

  // Format the output string
  static char hhmmss_str[7]; // 6 characters for HHMMSS + 1 for null terminator
  sprintf(hhmmss_str, "%02d%02d%02d", hours, minutes, seconds);
  return hhmmss_str;
}

int cVFD = P0;
int cLabel = P2;
int cRoller = P1;

Control rConveyor(0);
Control rSticker(0);
Control rRoller(0);

static const int buttonPin = 25;
int buttonStatePrevious = HIGH;

static const int buttonPin2 = 26;
int buttonStatePrevious2 = HIGH;

static const int buttonPin3 = 27;
int buttonStatePrevious3 = HIGH;

bool sensorState1, sensorState2;
int sensor1 = 2;
int sensor2 = 4;

unsigned long minButtonLongPressDuration = 2000;
unsigned long buttonLongPressUpMillis;
unsigned long buttonLongPressDownMillis;
unsigned long buttonLongPressEnterMillis;
bool buttonStateLongPressUp = false;
bool buttonStateLongPressDown = false;
bool buttonStateLongPressEnter = false;

const int intervalButton = 50;
unsigned long previousButtonMillis;
unsigned long buttonPressDuration;
unsigned long currentMillis;

const int intervalButton2 = 50;
unsigned long previousButtonMillis2;
unsigned long buttonPressDuration2;
unsigned long currentMillis2;

const int intervalButton3 = 50;
unsigned long previousButtonMillis3;
unsigned long buttonPressDuration3;
unsigned long currentMillis3;

unsigned long currentMillisRunAuto;
unsigned long previousMillisRunAuto;
unsigned long intervalRunAuto = 200;

unsigned long currentMillisLinear;
unsigned long previousMillisLinear;

void initRelays()
{
  pcf8575.pinMode(cVFD, OUTPUT);
  pcf8575.digitalWrite(cVFD, LOW);

  pcf8575.pinMode(cLabel, OUTPUT);
  pcf8575.digitalWrite(cLabel, LOW);

  pcf8575.pinMode(cRoller, OUTPUT);
  pcf8575.digitalWrite(cRoller, LOW);

  pcf8575.begin();
}

void InitializeButtons()
{
  pinMode(buttonPin, INPUT);
  pinMode(buttonPin2, INPUT);
  pinMode(buttonPin3, INPUT);
  pinMode(sensor1, INPUT);
  pinMode(sensor2, INPUT);
}

void StopAll()
{
  rConveyor.stop();
  rSticker.stop();
  rRoller.stop();

  pcf8575.digitalWrite(cVFD, HIGH);
  pcf8575.digitalWrite(cLabel, HIGH);
  pcf8575.digitalWrite(cRoller, HIGH);
}

void readSensors()
{
  if (digitalRead(sensor1) == true)
  {
    sensorState1 = false;
  }
  else
  {
    sensorState1 = true;
  }

  if (digitalRead(sensor2) == true)
  {
    sensorState2 = false;
  }
  else
  {
    sensorState2 = true;
  }
}

int runAutoStatus = 0;
bool InitialMoveSticker = false;
int counter = 0;

void runAuto()
{
  switch (runAutoStatus)
  {
    // Wait for Bottle
    // Run Sticker Wait for sensor
  case 0:
    // Wait for Bottle
    if (sensorState1 == true)
    {
      runAutoStatus = 1;
      InitialMoveSticker = true;
      counter += 1;
    }
    break;
  case 1:
    // Wait for Bottle
    if (InitialMoveSticker == true)
    {
      if (sensorState2 == true)
      {
        rSticker.relayOn();
        pcf8575.digitalWrite(cLabel, false);
      }
      else
      {
        delay(300);
        InitialMoveSticker = false;
      }
    }
    else
    {
      if (sensorState2 == true)
      {
        rSticker.relayOff();
        pcf8575.digitalWrite(cLabel, true);
        runAutoStatus = 0;
      }
      else
      {
        rSticker.relayOn();
        pcf8575.digitalWrite(cLabel, false);
      }
    }
    break;
  default:
    break;
  }
}

void readButtonUpState()
{
  if (currentMillis - previousButtonMillis > intervalButton)
  {
    int buttonState = digitalRead(buttonPin);
    if (buttonState == LOW && buttonStatePrevious == HIGH && !buttonStateLongPressUp)
    {
      buttonLongPressUpMillis = currentMillis;
      buttonStatePrevious = LOW;
    }
    buttonPressDuration = currentMillis - buttonLongPressUpMillis;
    if (buttonState == LOW && !buttonStateLongPressUp && buttonPressDuration >= minButtonLongPressDuration)
    {
      buttonStateLongPressUp = true;
    }
    if (buttonStateLongPressUp == true)
    {
      // Insert Fast Scroll Up
      refreshScreen = true;
      if (settingFlag == true)
      {
        if (settingEditFlag == true)
        {
          if (parametersTimer[currentSettingScreen] >= parametersTimerMaxValue[currentSettingScreen] - 1)
          {
            parametersTimer[currentSettingScreen] = parametersTimerMaxValue[currentSettingScreen];
          }
          else
          {
            parametersTimer[currentSettingScreen] += 10;
          }
        }
        else
        {
          if (currentSettingScreen == NUM_SETTING_ITEMS - 1)
          {
            currentSettingScreen = 0;
          }
          else
          {
            currentSettingScreen++;
          }
        }
      }
      else if (testMenuFlag == true)
      {
        if (currentTestMenuScreen == NUM_TESTMACHINE_ITEMS - 1)
        {
          currentTestMenuScreen = 0;
        }
        else
        {
          currentTestMenuScreen++;
        }
      }
      else if (runAutoFlag == true)
      {
      }
      else
      {
        if (currentMainScreen == NUM_MAIN_ITEMS - 1)
        {
          currentMainScreen = 0;
        }
        else
        {
          currentMainScreen++;
        }
      }
    }

    if (buttonState == HIGH && buttonStatePrevious == LOW)
    {
      buttonStatePrevious = HIGH;
      buttonStateLongPressUp = false;
      if (buttonPressDuration < minButtonLongPressDuration)
      {
        // Short Scroll Up
        refreshScreen = true;
        if (settingFlag == true)
        {
          if (settingEditFlag == true)
          {
            if (parametersTimer[currentSettingScreen] >= parametersTimerMaxValue[currentSettingScreen] - 1)
            {
              parametersTimer[currentSettingScreen] = parametersTimerMaxValue[currentSettingScreen];
            }
            else
            {
              parametersTimer[currentSettingScreen] += 10;
            }
          }
          else
          {
            if (currentSettingScreen == NUM_SETTING_ITEMS - 1)
            {
              currentSettingScreen = 0;
            }
            else
            {
              currentSettingScreen++;
            }
          }
        }
        else if (testMenuFlag == true)
        {
          if (currentTestMenuScreen == NUM_TESTMACHINE_ITEMS - 1)
          {
            currentTestMenuScreen = 0;
          }
          else
          {
            currentTestMenuScreen++;
          }
        }
        else if (runAutoFlag == true)
        {
        }
        else
        {
          if (currentMainScreen == NUM_MAIN_ITEMS - 1)
          {
            currentMainScreen = 0;
          }
          else
          {
            currentMainScreen++;
          }
        }
      }
    }
    previousButtonMillis = currentMillis;
  }
}

void readButtonDownState()
{
  if (currentMillis2 - previousButtonMillis2 > intervalButton2)
  {
    int buttonState2 = digitalRead(buttonPin2);
    if (buttonState2 == LOW && buttonStatePrevious2 == HIGH && !buttonStateLongPressDown)
    {
      buttonLongPressDownMillis = currentMillis2;
      buttonStatePrevious2 = LOW;
    }
    buttonPressDuration2 = currentMillis2 - buttonLongPressDownMillis;
    if (buttonState2 == LOW && !buttonStateLongPressDown && buttonPressDuration2 >= minButtonLongPressDuration)
    {
      buttonStateLongPressDown = true;
    }
    if (buttonStateLongPressDown == true)
    {
      refreshScreen = true;
      if (settingFlag == true)
      {
        if (settingEditFlag == true)
        {
          if (parametersTimer[currentSettingScreen] <= 0)
          {
            parametersTimer[currentSettingScreen] = 0;
          }
          else
          {
            parametersTimer[currentSettingScreen] -= 10;
          }
        }
        else
        {
          if (currentSettingScreen == 0)
          {
            currentSettingScreen = NUM_SETTING_ITEMS - 1;
          }
          else
          {
            currentSettingScreen--;
          }
        }
      }
      else if (testMenuFlag == true)
      {
        if (currentTestMenuScreen == 0)
        {
          currentTestMenuScreen = NUM_TESTMACHINE_ITEMS - 1;
        }
        else
        {
          currentTestMenuScreen--;
        }
      }
      else if (runAutoFlag == true)
      {
      }
      else
      {
        if (currentMainScreen == 0)
        {
          currentMainScreen = NUM_MAIN_ITEMS - 1;
        }
        else
        {
          currentMainScreen--;
        }
      }
    }

    if (buttonState2 == HIGH && buttonStatePrevious2 == LOW)
    {
      buttonStatePrevious2 = HIGH;
      buttonStateLongPressDown = false;
      if (buttonPressDuration2 < minButtonLongPressDuration)
      {
        refreshScreen = true;
        if (settingFlag == true)
        {
          if (settingEditFlag == true)
          {
            if (currentSettingScreen == 2)
            {
              if (parametersTimer[currentSettingScreen] <= 2)
              {
                parametersTimer[currentSettingScreen] = 2;
              }
              else
              {
                parametersTimer[currentSettingScreen] -= 1;
              }
            }
            else
            {
              if (parametersTimer[currentSettingScreen] <= 0)
              {
                parametersTimer[currentSettingScreen] = 0;
              }
              else
              {
                parametersTimer[currentSettingScreen] -= 10;
              }
            }
          }
          else
          {
            if (currentSettingScreen == 0)
            {
              currentSettingScreen = NUM_SETTING_ITEMS - 1;
            }
            else
            {
              currentSettingScreen--;
            }
          }
        }
        else if (testMenuFlag == true)
        {
          if (currentTestMenuScreen == 0)
          {
            currentTestMenuScreen = NUM_TESTMACHINE_ITEMS - 1;
          }
          else
          {
            currentTestMenuScreen--;
          }
        }
        else if (runAutoFlag == true)
        {
        }
        else
        {
          if (currentMainScreen == 0)
          {
            currentMainScreen = NUM_MAIN_ITEMS - 1;
          }
          else
          {
            currentMainScreen--;
          }
        }
      }
    }
    previousButtonMillis2 = currentMillis2;
  }
}

void readButtonEnterState()
{
  if (currentMillis3 - previousButtonMillis3 > intervalButton3)
  {
    int buttonState3 = digitalRead(buttonPin3);
    if (buttonState3 == LOW && buttonStatePrevious3 == HIGH && !buttonStateLongPressEnter)
    {
      buttonLongPressEnterMillis = currentMillis3;
      buttonStatePrevious3 = LOW;
    }
    buttonPressDuration3 = currentMillis3 - buttonLongPressEnterMillis;
    if (buttonState3 == LOW && !buttonStateLongPressEnter && buttonPressDuration3 >= minButtonLongPressDuration)
    {
      buttonStateLongPressEnter = true;
    }
    if (buttonStateLongPressEnter == true)
    {
      // Insert Fast Scroll Enter
      Serial.println("Long Press Enter");
    }

    if (buttonState3 == HIGH && buttonStatePrevious3 == LOW)
    {
      buttonStatePrevious3 = HIGH;
      buttonStateLongPressEnter = false;
      if (buttonPressDuration3 < minButtonLongPressDuration)
      {
        refreshScreen = true;
        if (currentMainScreen == 0 && settingFlag == true)
        {
          if (currentSettingScreen == NUM_SETTING_ITEMS - 1)
          {
            settingFlag = false;
            // saveSettings();
            // loadSettings();
            currentSettingScreen = 0;
            // setTimers();
          }
          else
          {
            if (settingEditFlag == true)
            {
              settingEditFlag = false;
            }
            else
            {
              settingEditFlag = true;
            }
          }
        }
        else if (currentMainScreen == 1 && testMenuFlag == true)
        {
          if (currentTestMenuScreen == NUM_TESTMACHINE_ITEMS - 1)
          {
            currentMainScreen = 0;
            currentTestMenuScreen = 0;
            testMenuFlag = false;
            // stopAllMotors();
          }
          else if (currentTestMenuScreen == 0)
          {
            if (rConveyor.getMotorState() == false)
            {
              rConveyor.relayOn();
              pcf8575.digitalWrite(cVFD, false);
            }
            else
            {
              rConveyor.relayOff();
              pcf8575.digitalWrite(cVFD, true);
            }
          }
          else if (currentTestMenuScreen == 1)
          {
            if (rSticker.getMotorState() == false)
            {
              // Software Interlock
              rSticker.relayOn();
              pcf8575.digitalWrite(cLabel, false);
            }
            else
            {
              rSticker.relayOff();
              pcf8575.digitalWrite(cLabel, true);
            }
          }
          else if (currentTestMenuScreen == 2)
          {
            if (rRoller.getMotorState() == false)
            {
              // Software Interlock
              rRoller.relayOn();
              pcf8575.digitalWrite(cRoller, false);
            }
            else
            {
              rRoller.relayOff();
              pcf8575.digitalWrite(cRoller, true);
            }
          }
        }
        else if (currentMainScreen == 2 && runAutoFlag == true)
        {
          StopAll();
          runAutoFlag = false;
          runAutoStatus = 0;
        }
        else
        {
          if (currentMainScreen == 0)
          {
            settingFlag = true;
          }
          else if (currentMainScreen == 1)
          {
            testMenuFlag = true;
          }
          else if (currentMainScreen == 2)
          {
            runAutoFlag = true;
            runAutoStatus = 0;
            rConveyor.relayOn();
            pcf8575.digitalWrite(cVFD, false);
            rRoller.relayOn();
            pcf8575.digitalWrite(cRoller, false);
            // timerLinearHoming.start();
            counter = 0;
          }
        }
      }
    }
    previousButtonMillis3 = currentMillis3;
  }
}

void ReadButtons()
{
  currentMillis = millis();
  currentMillis2 = millis();
  currentMillis3 = millis();
  readButtonEnterState();
  readButtonUpState();
  readButtonDownState();
  readSensors();
}

void initializeLCD()
{
  lcd.init();
  lcd.clear();
  lcd.createChar(0, enterChar);
  lcd.createChar(1, fastChar);
  lcd.createChar(2, slowChar);
  lcd.backlight();
  refreshScreen = true;
}

void printTestScreen(String TestMenuTitle, String Job, bool Status, bool ExitFlag)
{
  lcd.clear();
  lcd.print(TestMenuTitle);
  if (ExitFlag == false)
  {
    lcd.setCursor(0, 2);
    lcd.print(Job);
    lcd.print(" : ");
    if (Status == true)
    {
      lcd.print("ON");
    }
    else
    {
      lcd.print("OFF");
    }
  }

  if (ExitFlag == true)
  {
    lcd.setCursor(0, 3);
    lcd.print("Click to Exit Test");
  }
  else
  {
    lcd.setCursor(0, 3);
    lcd.print("Click to Run Test");
  }
  refreshScreen = false;
}

void printMainMenu(String MenuItem, String Action)
{
  lcd.clear();
  lcd.print(MenuItem);
  lcd.setCursor(0, 3);
  lcd.write(0);
  lcd.setCursor(2, 3);
  lcd.print(Action);
  refreshScreen = false;
}

void printSettingScreen(String SettingTitle, String Unit, int Value, bool EditFlag, bool SaveFlag)
{
  lcd.clear();
  lcd.print(SettingTitle);
  lcd.setCursor(0, 1);

  if (SaveFlag == true)
  {
    lcd.setCursor(0, 3);
    lcd.write(0);
    lcd.setCursor(2, 3);
    lcd.print("ENTER TO SAVE ALL");
  }
  else
  {
    lcd.print(Value);
    lcd.print(" ");
    lcd.print(Unit);
    lcd.setCursor(0, 3);
    lcd.write(0);
    lcd.setCursor(2, 3);
    if (EditFlag == false)
    {
      lcd.print("ENTER TO EDIT");
    }
    else
    {
      lcd.print("ENTER TO SAVE");
    }
  }
  refreshScreen = false;
}

void printRunAuto(String SettingTitle, String Process, String Count)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(SettingTitle);
  lcd.setCursor(0, 1);
  lcd.print(Process);
  lcd.setCursor(0, 2);
  lcd.print("Current Count:");
  lcd.setCursor(0, 3);
  lcd.print(Count);
  refreshScreen = false;
}

void printScreen()
{
  if (settingFlag == true)
  {
    if (currentSettingScreen == NUM_SETTING_ITEMS - 1)
    {
      printSettingScreen(setting_items[currentSettingScreen][0], setting_items[currentSettingScreen][1], parametersTimer[currentSettingScreen], settingEditFlag, true);
    }
    else
    {
      printSettingScreen(setting_items[currentSettingScreen][0], setting_items[currentSettingScreen][1], parametersTimer[currentSettingScreen], settingEditFlag, false);
    }
  }
  else if (testMenuFlag == true)
  {
    switch (currentTestMenuScreen)
    {
    case 0:
      // printTestScreen(testmachine_items[currentTestMenuScreen], "Status", ContactorVFD.getMotorState(), false);
      printTestScreen(testmachine_items[currentTestMenuScreen], "Status", rConveyor.getMotorState(), false);
      break;
    case 1:
      printTestScreen(testmachine_items[currentTestMenuScreen], "Status", rSticker.getMotorState(), false);
      break;
    case 2:
      printTestScreen(testmachine_items[currentTestMenuScreen], "Status", rRoller.getMotorState(), false);
      break;
    case 3:
      printTestScreen(testmachine_items[currentTestMenuScreen], "", true, true);
      break;
    default:
      break;
    }
  }
  else if (runAutoFlag == true)
  {
    switch (runAutoStatus)
    {
    case 0:
      printRunAuto("Waiting For Bottle", "", String(counter));
      break;
    case 1:
      printRunAuto("Dispensing Sticker", "", String(counter));
      break;
    default:
      break;
    }
  }
  else
  {
    printMainMenu(menu_items[currentMainScreen][0], menu_items[currentMainScreen][1]);
  }
}

void setup()
{
  // put your setup code here, to run once:
  initRelays();
  InitializeButtons();
  initializeLCD();
  Settings.begin("timerSetting", false);
  Serial.begin(9600);
  counter = 0;
}

void loop()
{
  ReadButtons();
  Serial.print("Sensor 1 :");
  Serial.println(sensorState1);
  Serial.print("Sensor 2 :");
  Serial.println(sensorState2);
  if (refreshScreen == true)
  {
    printScreen();
    refreshScreen = false;
  }
  if (runAutoFlag == true)
  {
    runAuto();

    unsigned long currentMillisRunAuto = millis();
    if (currentMillisRunAuto - previousMillisRunAuto >= intervalRunAuto)
    {
      previousMillisRunAuto = currentMillisRunAuto;
      refreshScreen = true;
    }
  }
}