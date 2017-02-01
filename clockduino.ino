// https://github.com/adafruit/RTClib
//    (Maybe instead https://github.com/mizraith/RTClib.git)
// ChronoDot v2.1 - DS3231 - https://www.adafruit.com/products/255
#include <RTClib.h>

// https://github.com/adafruit/Adafruit_LED_Backpack
// Adafruit 1.2" 7-segment clock display - https://www.adafruit.com/products/1266
#include <Adafruit_GFX.h>
#include <gfxfont.h>
#include <Adafruit_LEDBackpack.h>
#define LED_DOTS_MIDDLE  2
#define LED_DOTS_LEFTTOP 4
#define LED_DOTS_LEFTBOT 8
#define LED_DOTS_RIGHT   16

// https://github.com/adafruit/Adafruit_VS1053_Library
// Adafruit Music Maker Shield w/ amp - VS1053 - https://www.adafruit.com/products/1788
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>
#define VS1053_RESET -1
#define VS1053_CHIPSELECT 7
#define VS1053_DATASELECT 6
#define VS1053_CARDSELECT 4
#define VS1053_DREQ 3

// https://github.com/bneedhamia/sdconfigfile
#include <SDConfigFile.h>
#define CONFIG_FILE "config.cfg"


#define ERR_NO_CLOCK 2
#define ERR_NO_AUDIO 4
#define ERR_NO_STORAGE 7
#define ERR_NO_BACKGROUND 12


#define PIN_LED_SNOOZE 5
#define PIN_BTN_SNOOZE 10
#define PIN_BTN_HOUR   9
#define PIN_BTN_MINUTE 8


// Different states the clock can be in
typedef enum {SET_ARMED, SET_TIME, SET_ALARM, SET_SNOOZE, SET_VOLUME, SET_BRIGHTNESS,
              NORMAL, ALARMED, MENU} state;
state currentState = NORMAL, currentMenuItem = SET_ARMED;

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
      if(_time % 100 == 59) {
        _time -= 59;
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
RTC_DS3231 rtc;
Adafruit_7segment matrix = Adafruit_7segment();
Adafruit_VS1053_FilePlayer player = Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CHIPSELECT, VS1053_DATASELECT, VS1053_DREQ, VS1053_CARDSELECT);


unsigned long _previousMillis = 0; // Tracks time since last loop iteration
boolean waitingOnButtonLift;       // Tracks whether or not a button has been pressed and is awaiting release
boolean dots = false;              // Tracks the status of the dots for flashing
unsigned int brightness = 15;

// One-time setup to initialize the application
void setup() 
{
  Serial.begin(9600);
  matrix.begin(0x70);
  pinMode(PIN_BTN_SNOOZE, INPUT_PULLUP);
  pinMode(PIN_BTN_HOUR,   INPUT_PULLUP);
  pinMode(PIN_BTN_MINUTE, INPUT_PULLUP);
  pinMode(PIN_LED_SNOOZE, OUTPUT);

  if (! rtc.begin()) {
    Serial.println(F("Couldn't find RTC"));
     haltWithError(ERR_NO_CLOCK);
  }
  if (! player.begin()) {
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     haltWithError(ERR_NO_AUDIO);
  }
  if (!SD.begin(VS1053_CARDSELECT)) {
    Serial.println(F("SD failed, or not present"));
    haltWithError(ERR_NO_STORAGE);
  }
  if (! player.useInterrupt(VS1053_FILEPLAYER_PIN_INT)) {
    Serial.println(F("DREQ pin is not an interrupt pin"));
    haltWithError(ERR_NO_BACKGROUND);
  }

  readConfig();
  
  player.setVolume(100-alarm._volume, 100-alarm._volume);
  beep();

  matrix.setBrightness(brightness);
  matrix.print(8888);
  matrix.writeDisplay();
  delay(500);
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
  digitalWrite(PIN_LED_SNOOZE, alarm._snoozeCount > 0 || currentState==ALARMED ? HIGH : LOW);

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
}

// Update AM/PM, armed, & center dots
void updateDots(boolean isPM) {
  matrix.writeDigitRaw(2, (isPM ? LED_DOTS_LEFTBOT : 0) | (alarm._armed? LED_DOTS_LEFTTOP : 0) | (dots ? LED_DOTS_MIDDLE : 0));
}

void enterMenu() {
  currentState = MENU;
  currentMenuItem = SET_ARMED;
  play(alarm._armed ? "mmenu001.mp3" : "mmenu000.mp3");
}

void exitMenu() {
  currentState = NORMAL;
  beep();
}

void returnToMenu() {
  currentState = MENU;
  SD.remove(CONFIG_FILE);
  File cfgFile = SD.open(CONFIG_FILE, FILE_WRITE);
  if(!cfgFile) {
    Serial.print("Error writing to config");
    beep(); beep(); return;
  }
  cfgFile.print(F("armed="));
  cfgFile.println(alarm._armed ? F("true") : F("false"));
  cfgFile.print(F("alarm="));
  cfgFile.println(alarm._time);
  cfgFile.print(F("snooze="));
  cfgFile.println(alarm._snooze);
  cfgFile.print(F("volume="));
  cfgFile.println(alarm._volume);
  cfgFile.print(F("brightness="));
  cfgFile.println(brightness);
  cfgFile.close();
}

void selectMenuItem() {
  if(currentMenuItem == SET_ARMED) {
    alarm._armed = !alarm._armed;
    returnToMenu();
    navigateMenu();
  } else {
    currentState = currentMenuItem;
    beep();
  }
}

// Button press alters the current menu item
void navigateMenu() {
  String filename = "mmenu00";
  if (digitalRead(PIN_BTN_HOUR) == LOW) {
    currentMenuItem = (currentMenuItem + 1) % (NORMAL);
  } else if (digitalRead(PIN_BTN_MINUTE) == LOW) {
    currentMenuItem = (currentMenuItem - 1) % (NORMAL);
    if(currentMenuItem < 0)
      currentMenuItem = NORMAL - 1;
  }
  
  if(currentMenuItem==SET_ARMED)
    filename = filename + (alarm._armed ? 1 : 0);
  else
    filename = filename + (currentMenuItem + 1);

  filename = filename + ".mp3";
  char filenameC[13];
  filename.toCharArray(filenameC, 13);
  play(filenameC);
}

// Button press alters the time
void setTime() {
  DateTime now = rtc.now();
  if (digitalRead(PIN_BTN_HOUR) == LOW){
    now = now + TimeSpan(0, 1, 0, 0);
  } else if (digitalRead(PIN_BTN_MINUTE) == LOW) {
    now = now + TimeSpan(0, 0, 1, 0);
    if(now.minute()==0)
      now = now - TimeSpan(0, 1, 0, 0);
  }
  rtc.adjust(now);
}

// Button press alters the alarm time
void setAlarm() {
  if (digitalRead(PIN_BTN_HOUR) == LOW)
    alarm.addHour();
  else if (digitalRead(PIN_BTN_MINUTE) == LOW)
    alarm.addMinute();
}

// Button press alters the snooze delay
void setSnooze() {
  if (digitalRead(PIN_BTN_HOUR) == LOW)
    alarm._snooze = min(alarm._snooze + 1, 59);
  else if (digitalRead(PIN_BTN_MINUTE) == LOW)
    alarm._snooze = max(alarm._snooze - 1, 1);
}

// Button press alters the volume
void setVolume() {
  if (digitalRead(PIN_BTN_HOUR) == LOW)
    alarm._volume = min(alarm._volume + 10, 1000);
  else if (digitalRead(PIN_BTN_MINUTE) == LOW)
    alarm._volume = max(alarm._volume - 10 , 1);
  player.setVolume(100-alarm._volume, 100-alarm._volume);
  beep();
}

void setBrightness() {
  if (digitalRead(PIN_BTN_HOUR) == LOW){
    brightness = min(brightness + 1, 15);
  } else if (digitalRead(PIN_BTN_MINUTE) == LOW) {
    brightness = max(brightness - 1, 1);
  }
  matrix.setBrightness(brightness);
}

// Check the state of the buttons and act accordingly
void buttonCheck() {

  boolean hourPressed = digitalRead(PIN_BTN_HOUR) == LOW;
  boolean minutePressed = digitalRead(PIN_BTN_MINUTE) == LOW;
  boolean snoozePressed = digitalRead(PIN_BTN_SNOOZE) == LOW;

  if(waitingOnButtonLift && (hourPressed || minutePressed || snoozePressed))
    return;

  // Enter/exit menu & turn off alarm
  if (hourPressed && minutePressed) {
    switch(currentState) {
      case ALARMED:
        dismissAlarm();
        break;
      case NORMAL:
        if(alarm._snoozeCount > 0)
          dismissAlarm();
        else
          enterMenu();
        break;
      default:
        exitMenu();
        break;
    }
  // Singular button press; handle different depending on state
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
  // Snooze & confirm menu selection
  } else if (snoozePressed) {
    if(currentState==ALARMED)
      snooze();
    else if(currentState==MENU)
      selectMenuItem();
    else
      returnToMenu();
  }
  waitingOnButtonLift = hourPressed || minutePressed || snoozePressed;
  if(waitingOnButtonLift)
    updateDisplay();
}

// Check if alarm should be / should start playing
void alarmCheck() {
  DateTime now = rtc.now();
  int t = now.hour()*100 + now.minute();
  if((currentState != ALARMED && alarm.check(t)) || (currentState == ALARMED && !player.playingMusic)) {
    currentState = ALARMED;
    play("alarming.mp3");
  }
}

// Print time, given in the format 1200 (HHMM)
void printTime(int t) {
  if(t < 100)
    matrix.print(1200 + t);
  else if(t < 1300)
    matrix.print(t);
  else
    matrix.print(t % 1200);

  // Write AM/PM dot
  //TODO: Needed?
  if(t >= 1200)
    matrix.writeDigitNum(2, 7, true);
}



void readConfig() {
  SDConfigFile cfg;

  cfg.begin(CONFIG_FILE, 128);
  while (cfg.readNextSetting()) {
    if (cfg.nameIs("armed"))
      alarm._armed = cfg.getBooleanValue();
    else if(cfg.nameIs("alarm"))
      alarm._time = cfg.getIntValue();
    else if(cfg.nameIs("snooze"))
      alarm._snooze = cfg.getIntValue();
    else if(cfg.nameIs("volume"))
      alarm._volume = cfg.getIntValue();
    else if(cfg.nameIs("brightness"))
      brightness = cfg.getIntValue();
  }
  cfg.end();
}

void dismissAlarm() {
  currentState = NORMAL;
  alarm.reset();
  player.stopPlaying();
}

void snooze() {
  alarm.hitSnooze();
  player.stopPlaying();
  currentState = NORMAL; 
}

void play(const char* filename) {
  Serial.print("Playing ");
  Serial.println(filename);
  player.stopPlaying();
  player.startPlayingFile(filename);
}


void beep() {
  player.sineTest(0x44, 250);
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
 *  - use beeps as fallback for if file cannot be played / card is absent
 *  - make prettier startup sound
 *  - Volume, for some reason, does not work; maybe not through headphone jack?
 */
