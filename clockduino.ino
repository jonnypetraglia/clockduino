#include <Adafruit_GFX.h>
#include <gfxfont.h>
#include <Adafruit_LEDBackpack.h>
#include <Wire.h>
#include <RTClib.h>

#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>

int snoozeLedPin = 5;
int snoozeBtnPin = 10;
int hourBtnPin = 9, minuteBtnPin = 8;


int VS1053_reset = -1,
    VS1053_chipselect = 7,
    VS1053_dataselect = 6,
    VS1053_cardselect = 4,
    VS1053_dreq = 3;

#define BUTTON_TIMEOUT_S 3
#define FLASH_INTERVAL_MS 750

#define ERR_NO_AUDIO 4
#define ERR_NO_STORAGE 7
#define ERR_NO_BACKGROUND 12

// Different states the clock can be in
typedef enum {NORMAL, ALARMED, SNOOZED, MENU, SET_ARMED, SET_TIME, SET_ALARM, SET_SNOOZE, SET_VOLUME} state;
state currentState = NORMAL;

// Objects for the peripherals
class Alarm {
  public:
    int _time = 0;
    int _snooze = 5;
    int _snoozeCount = 0;
    void addHour() {
      _time = (_time + 100) % 1200;
    }
    void addMinute() {
      _time += 1;
    }
    int hitSnooze() {
      _snoozeCount += 1;
    }
    int dismiss() {
      _snoozeCount = 0;
    }
    boolean check(int t) {
      return t == _time + (_snooze * _snoozeCount);
    }
};
Alarm alarm;
RTC_Millis rtc;
Adafruit_7segment matrix = Adafruit_7segment();
Adafruit_VS1053_FilePlayer player = Adafruit_VS1053_FilePlayer(VS1053_reset, VS1053_chipselect, VS1053_dataselect, VS1053_dreq, VS1053_cardselect);


unsigned long _previousMillis = 0; // Tracks time since last loop iteration
boolean waitingOnButtonLift;       // Tracks whether or not a button has been pressed and is awaiting release
boolean dots = false;              // Tracks the status of the dots for flashing

// One-time setup to initialize the application
void setup() 
{
  Serial.begin(9600);
//  rtc.begin(DateTime(F(__DATE__), F(__TIME__)));
  matrix.begin(0x70);
  pinMode(snoozeBtnPin, INPUT_PULLUP);
  pinMode(hourBtnPin, INPUT_PULLUP);
  pinMode(minuteBtnPin, INPUT_PULLUP);
  pinMode(snoozeLedPin, OUTPUT);

  if (! player.begin()) {
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     haltWithError(ERR_NO_AUDIO);
  }
   if (!SD.begin(VS1053_cardselect)) {
    Serial.println(F("SD failed, or not present"));
    haltWithError(ERR_NO_STORAGE);
  }
  if (! player.useInterrupt(VS1053_FILEPLAYER_PIN_INT)) {
    Serial.println(F("DREQ pin is not an interrupt pin"));
    haltWithError(ERR_NO_BACKGROUND);
  }
  
  player.setVolume(20,20);
  player.sineTest(0x44, 500);
  Serial.println("Playing started");

  matrix.print(8888);
  matrix.writeDisplay();
  delay(1000);
}

// Main program loop; updates display every half second & calls other programs to check on state
void loop() 
{
  if(millis() - _previousMillis >= 500) {
    dots = !dots;
    updateDisplay();
    _previousMillis = millis();
  }

  alarmCheck();
  buttonCheck();
}


// Draws the display, depending on the state
void updateDisplay() {
  switch(currentState) {
    case SET_TIME:
      Serial.println("SET_TIME");
      break;
    case SET_ALARM:
      Serial.println("SET_ALARM");
      break;
    case NORMAL:
      Serial.println("NORMAL");
      break;
  }
  /*** Just clear the display if the period is in the middle of a flash ***/
  if(currentState == SET_TIME || currentState == SET_ALARM) {
    if(!dots) {
      matrix.clear();
      matrix.writeDisplay();
      return;
    }
  }

  // Otherwise, just display the alarm or time
  if(currentState == SET_ALARM) {
    printTime(alarm._time);
  } else {
    DateTime now = rtc.now();
    printTime(now.hour()*100 + now.minute());
  }
  matrix.drawColon(dots);
  matrix.writeDisplay();
  _previousMillis = millis();
}

// Change state depending on button states
void changeState() {
    if (digitalRead(snoozeBtnPin) == LOW) {
      Serial.println("Pressed hour and min and snooze");
      if(currentState == NORMAL)
        currentState = SET_ALARM;
      else if(currentState == SET_ALARM || currentState == SET_TIME)
        currentState = NORMAL;
    } else {
      Serial.println("Pressed hour and min");
      if(currentState == NORMAL)
        currentState = SET_TIME;
      else if(currentState == SET_TIME)
        currentState = NORMAL;
    }
    waitingOnButtonLift = true;
}

// Set the time
void setTime() {
  if (digitalRead(hourBtnPin) == LOW){
    Serial.println("Pressed hour");
    if(currentState == SET_TIME) {
      DateTime now = rtc.now();
      now = now + TimeSpan(0, 1, 0, 0);
      rtc.adjust(now);
    } else if(currentState == SET_ALARM) {
      alarm.addHour();
    } else {
      return;
    }
    dots = true;
    updateDisplay();
    waitingOnButtonLift = true;
  } else if (digitalRead(minuteBtnPin) == LOW) {
    Serial.println("Pressed minute");
    if(currentState == SET_TIME) {
      DateTime now = rtc.now();
      now = now + TimeSpan(0, 0, 1, 0);
      rtc.adjust(now);
      dots = true;
      updateDisplay();
      waitingOnButtonLift = true;
    }
  }
}

// Check the state of the buttons and act accordingly
void buttonCheck() {

  boolean hourPressed = digitalRead(hourBtnPin) == LOW;
  boolean minutePressed = digitalRead(minuteBtnPin) == LOW;
  boolean snoozePressed = digitalRead(snoozeBtnPin) == LOW;

  if(waitingOnButtonLift && (hourPressed || minutePressed || snoozePressed)) {
    Serial.println("Waiting on button press ");
    Serial.println(waitingOnButtonLift);
    Serial.println(hourPressed);
    Serial.println(minutePressed);
    Serial.println(snoozePressed);
    return;
  }

  waitingOnButtonLift = false;

  digitalWrite(snoozeLedPin, snoozePressed ? HIGH : LOW);

  if (hourPressed && minutePressed)
  {
    changeState();
  }
  else if (hourPressed || minutePressed) {
    setTime();
  }
  else if (snoozePressed && currentState==ALARMED) {
    snooze();
  }
}

void alarmCheck() {
  DateTime now = rtc.now();
  int t = now.hour()*100 + now.minute();
  if(alarm.check(t))
    startAlarm();
}

void printTime(int t) {
  matrix.print(t % 1200);
//  matrix.writeDigitNum(2, i++, true);
}


void snooze() {
  alarm.hitSnooze();
  player.stopPlaying();
  //TODO: Change state to SNOOZED
}

void startAlarm()
{
  /*
   * Play different track depending on times snoozed
   */
}

void haltWithError(int errorCode) {
  matrix.clear();
  matrix.print(errorCode);  //TODO: Display 'E:##'
  matrix.drawColon(dots);
  matrix.writeDisplay();
  while(1);
}

/* TODO:
 *  - pressing both buttons enters Menu (H = next, M = previous, S = confirm, H&M = exit)
 *    1. "Set armed/Alarm Enabled" (H = Y, M = N, S = confirm)
 *    2. "Set alarm" (H = H, M = M, S = confirm)
 *    3. "Set time" (H = H, M = M, S = confirm)
 *    4. "Set volume" (H = +, M = -, S = confirm) [1-100 by 10's?; remember: smaller # = louder]
 *      - beep on each change
 *    5. "Set snooze delay" (H = +, M = -, S = confirm)
 *  - show dot for AM/PM (in printTime?)
 *  - howto dismiss alarm? press both buttons when snoozed?
 *  - show dot when armed
 *  - show Snooze LED when alarming/snoozed
 *  - use SD.begin in main loop so card can be removed when on?
 *    - play "bum-BUM" sound on insert, "BUM-bum" on remove
 *    - maybe use other light for SD?
 *  - use beeps as fallback for if file cannot be played / card is absent
 *  - make prettier startup sound
 *  - add menu option to choose track?
 *  - Store settings to SD so persists through power change
 *    - http://pastebin.com/2jyCDHcf
 *      - https://www.arduino.cc/en/Tutorial/DumpFile
 *      - https://www.arduino.cc/en/Tutorial/ReadWrite
 *      - https://www.arduino.cc/en/Tutorial/Files
 *    - https://github.com/bneedhamia/sdconfigfile
 *    - https://github.com/nigelb/arduino-FSConf
 *    - https://github.com/stevemarple/IniFile
 */

unsigned long sec(unsigned long x)
{
  return x*1000000;
}
