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
typedef enum {NORMAL, ALARMED, MENU,
              SET_ARMED, SET_TIME, SET_ALARM, SET_SNOOZE, SET_VOLUME, SET_BRIGHTNESS} state;
state currentState = NORMAL, currentMenuItem = SET_ARMED;

// For dots:  // 0x2 is middle, 0x4 is left-top, 0x8 is left-bottom, 0x16 is right

// Objects for the peripherals
class Alarm {
  public:
    boolean _armed = true; //DEBUG: false;
    int _time = 629; //DEBUG: 0;
    int _snooze = 1; //DEBUG: 5;
    int _snoozeCount = 0;
    int _volume = 10;
    
    void addHour() {
      _time = (_time + 100) % 2400;
    }
    void addMinute() {
      if(_time % 59 == 59) {
        _time -= 59;
        addHour();
      } else
        _time += 1;
    }
    int hitSnooze() {
      _snoozeCount += 1;
    }
    int reset() {
      _snoozeCount = 0;
    }
    boolean check(int t) {
      return _armed && (t == _time + (_snooze * _snoozeCount));
    }
};
Alarm alarm;
RTC_Millis rtc;
Adafruit_7segment matrix = Adafruit_7segment();
Adafruit_VS1053_FilePlayer player = Adafruit_VS1053_FilePlayer(VS1053_reset, VS1053_chipselect, VS1053_dataselect, VS1053_dreq, VS1053_cardselect);


unsigned long _previousMillis = 0; // Tracks time since last loop iteration
boolean waitingOnButtonLift;       // Tracks whether or not a button has been pressed and is awaiting release
boolean dots = false;              // Tracks the status of the dots for flashing
unsigned int brightness = 15;

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
  
  player.setVolume(100-alarm._volume, 100-alarm._volume);
  beep();

  matrix.setBrightness(brightness);
  matrix.print(8888);
  matrix.writeDisplay();
  delay(500);
}

void beep() {
  player.sineTest(0x44, 250);
  player.stopPlaying();
}

// Main program loop; updates display every half second & calls other programs to check on state
void loop() 
{
  if(millis() - _previousMillis >= 500) {
    dots = !dots;
    updateDisplay();
    _previousMillis = millis();
  }

  if(currentState != SET_TIME && currentState != SET_ALARM)
    alarmCheck();
  buttonCheck();
}


// Draws the display, depending on the state
void updateDisplay() {
  
  //Light up the Snooze LED if we're currently snoozing
  digitalWrite(snoozeLedPin, alarm._snoozeCount > 0 || currentState==ALARMED ? HIGH : LOW);

  // Display is flashing and is currently blank
  if(currentState != NORMAL && currentState != ALARMED && currentState != MENU) {
    if(!dots) {
      matrix.clear();
      matrix.writeDisplay();
      return;
    }
  }

  if(currentState == MENU || currentState == SET_ARMED) {
    int i = 0;
    switch(currentMenuItem) {
      case SET_BRIGHTNESS:  i += 1;
      case SET_VOLUME:  i += 1;
      case SET_SNOOZE:  i += 1;
      case SET_ALARM:   i += 1;
      case SET_TIME:    i += 1;
      case SET_ARMED:   i += 1;
    }
    matrix.print(i);
  }

  boolean AMPM = false;
  switch(currentState) {
    case SET_ALARM:
      printTime(alarm._time);
      AMPM = alarm._time >= 1200;
      break;
    case SET_SNOOZE:
      matrix.print(alarm._snooze);
      break;
    case SET_VOLUME:
      matrix.print(alarm._volume);
      break;
    case SET_BRIGHTNESS:
      matrix.print(brightness);
      break;
      
    case NORMAL:
    case ALARMED:
    case SET_TIME:
      DateTime now = rtc.now();
      printTime(now.hour()*100 + now.minute());
      AMPM = now.hour() >= 12;
      break;
  }

  updateDots(AMPM);
  matrix.writeDisplay();
  _previousMillis = millis();
}

void updateDots(boolean isPM) {
  matrix.writeDigitRaw(2, (isPM ? 8 : 0) + (alarm._armed? 4 : 0) + (dots ? 2 : 0));
}

void navigateMenu() {
  if (digitalRead(hourBtnPin) == LOW){
    switch(currentMenuItem) {
      case SET_ARMED:
        currentMenuItem = SET_TIME;
        player.startPlayingFile("mmenu002.mp3");
        break;
      case SET_TIME: 
        currentMenuItem = SET_ALARM;
        player.startPlayingFile("mmenu003.mp3");
        break;
      case SET_ALARM:
        currentMenuItem = SET_SNOOZE;
        player.startPlayingFile("mmenu004.mp3");
        break;
      case SET_SNOOZE:
        currentMenuItem = SET_VOLUME;
        player.startPlayingFile("mmenu005.mp3");
        break;
      case SET_VOLUME:
        currentMenuItem = SET_BRIGHTNESS;
        player.startPlayingFile("mmenu006.mp3");
        break;
      case SET_BRIGHTNESS:
        currentMenuItem = SET_ARMED;
        player.startPlayingFile(alarm._armed ? "mmenu001.mp3" : "mmenu000.mp3");
        break;
    }
  }
  else if (digitalRead(minuteBtnPin) == LOW) {
    switch(currentMenuItem) {
      case SET_ARMED:
        currentMenuItem = SET_BRIGHTNESS;
        player.startPlayingFile("mmenu006.mp3");
        break;
      case SET_TIME: 
        currentMenuItem = SET_ARMED;
        player.startPlayingFile(alarm._armed ? "mmenu001.mp3" : "mmenu000.mp3");
        break;
      case SET_ALARM:
        currentMenuItem = SET_TIME;
        player.startPlayingFile("mmenu002.mp3");
        break;
      case SET_SNOOZE:
        currentMenuItem = SET_ALARM;
        player.startPlayingFile("mmenu003.mp3");
        break;
      case SET_VOLUME:
        currentMenuItem = SET_SNOOZE;
        player.startPlayingFile("mmenu004.mp3");
        break;
      case SET_BRIGHTNESS:
        currentMenuItem = SET_VOLUME;
        player.startPlayingFile("mmenu005.mp3");
        break;
    }
  }
}

// Set the time
void setTime() {
  DateTime now = rtc.now();
  if (digitalRead(hourBtnPin) == LOW){
    now = now + TimeSpan(0, 1, 0, 0);
  } else if (digitalRead(minuteBtnPin) == LOW) {
    now = now + TimeSpan(0, 0, 1, 0);
  }
  rtc.adjust(now);
}

// Set the alarm
void setAlarm() {
  if (digitalRead(hourBtnPin) == LOW){
    alarm.addHour();
  } else if (digitalRead(minuteBtnPin) == LOW) {
    alarm.addMinute();
  }
}

void setSnooze() {
  if (digitalRead(hourBtnPin) == LOW){
    alarm._snooze = min(alarm._snooze + 1, 59);
  } else if (digitalRead(minuteBtnPin) == LOW) {
    alarm._snooze = max(alarm._snooze - 1, 1);
  }
}

void setVolume() {
  if (digitalRead(hourBtnPin) == LOW){
    alarm._volume = min(alarm._volume + 10, 1000);
  } else if (digitalRead(minuteBtnPin) == LOW) {
    alarm._volume = max(alarm._volume - 10 , 1);
  }
  player.setVolume(100-alarm._volume, 100-alarm._volume);
  beep();
}

void setBrightness() {
  if (digitalRead(hourBtnPin) == LOW){
    brightness = min(brightness + 1, 15);
  } else if (digitalRead(minuteBtnPin) == LOW) {
    brightness = max(brightness - 1, 1);
  }
  matrix.setBrightness(brightness);
}

// Check the state of the buttons and act accordingly
void buttonCheck() {

  boolean hourPressed = digitalRead(hourBtnPin) == LOW;
  boolean minutePressed = digitalRead(minuteBtnPin) == LOW;
  boolean snoozePressed = digitalRead(snoozeBtnPin) == LOW;

  if(waitingOnButtonLift && (hourPressed || minutePressed || snoozePressed))
    return;

  waitingOnButtonLift = false;


  if (hourPressed && minutePressed) {
    switch(currentState) {
      case ALARMED:
        currentState = NORMAL;
        alarm.reset();
        player.stopPlaying();
        break;
      case NORMAL:
        Serial.print("Entering Menu");
        currentState = MENU;
        currentMenuItem = SET_ARMED;
        player.startPlayingFile(alarm._armed ? "mmenu001.mp3" : "mmenu000.mp3");
        break;
      default:
        currentState = NORMAL;
        beep();
        break;
    }
    waitingOnButtonLift = true; 
  } else if (hourPressed || minutePressed) {
    switch(currentState) {
      case MENU:
        navigateMenu();
        break;
      case SET_TIME:
        setTime();
        break;
      case SET_ALARM:
        setAlarm();
        break;
      case SET_SNOOZE:
        setSnooze();
        break;
      case SET_VOLUME:
        setVolume();
        break;
      case SET_BRIGHTNESS:
        setBrightness();
        break;
      case NORMAL:
      case SET_ARMED:
        return;
    }
    dots = true;
    updateDisplay();
    waitingOnButtonLift = true;
  } else if (snoozePressed) {
    switch(currentState) {
      case ALARMED:
        snooze();
        break;
      case MENU:
        if(currentMenuItem == SET_ARMED) {
          alarm._armed = !alarm._armed;
          player.startPlayingFile(alarm._armed ? "mmenu001.mp3" : "mmenu000.mp3");
        } else {
          currentState = currentMenuItem;
          beep();
        }
        break;
      case SET_TIME:
      case SET_ALARM:
      case SET_SNOOZE:
      case SET_VOLUME:
      case SET_BRIGHTNESS:
        currentState = MENU;
        beep();
    }
    updateDisplay();
    waitingOnButtonLift = true;
  }
}

void alarmCheck() {
  DateTime now = rtc.now();
  int t = now.hour()*100 + now.minute();
  if(alarm.check(t) || (currentState == ALARMED && !player.playingMusic)) {
    Serial.println("ALARMED");
    currentState = ALARMED;
    player.startPlayingFile("alarming.mp3");
  }
}

void printTime(int t) {
  if(t < 100)
    matrix.print(1200 + t);
  else if(t < 1300)
    matrix.print(t);
  else
    matrix.print(t % 1200);

  if(t >= 1200)
    matrix.writeDigitNum(2, 7, true);
}


void snooze() {
  alarm.hitSnooze();
  player.stopPlaying();
}

void haltWithError(int errorCode) {
  matrix.clear();
  matrix.print(errorCode);
  matrix.writeDigitRaw(1,121);
  matrix.writeDisplay();
  while(1);
}

/* TODO:
 *  - rework navigateMenu (& maybe buttonCheck) to clean up
 *    - simplified way of playing file; stop player beforehand
 *  - use beeps as fallback for if file cannot be played / card is absent
 *  - make prettier startup sound
 *  - Store settings to SD so persists through power change
 *    - http://pastebin.com/2jyCDHcf
 *      - https://www.arduino.cc/en/Tutorial/DumpFile
 *      - https://www.arduino.cc/en/Tutorial/ReadWrite
 *      - https://www.arduino.cc/en/Tutorial/Files
 *    - https://github.com/bneedhamia/sdconfigfile
 *    - https://github.com/nigelb/arduino-FSConf
 *    - https://github.com/stevemarple/IniFile
 *  - Volume, for some reason, does not work; maybe not through headphone jack?
 *  - add menu option to choose track?
 */

unsigned long sec(unsigned long x)
{
  return x*1000000;
}
