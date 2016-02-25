#include <Adafruit_GFX.h>
#include <gfxfont.h>
#include <Adafruit_LEDBackpack.h>
#include <Wire.h>
#include <RTClib.h>


int TEST_ledPin = 13;
int snoozePin = 9;
int hourPin = 8, minutePin = 7;

#define BUTTON_TIMEOUT_S 3
#define FLASH_INTERVAL_MS 750

typedef enum {NORMAL, ALARM, SET_TIME, SET_ALARM,} state;
state currentState = NORMAL;

RTC_Millis rtc;
Adafruit_7segment matrix = Adafruit_7segment();
unsigned long _previousMillis = 0;
boolean waitingOnButtonLift;
boolean dots = false;

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
};
Alarm alarm;

 
void setup() 
{
  Serial.begin(9600);
//  rtc.begin(DateTime(F(__DATE__), F(__TIME__)));
  matrix.begin(0x70);
  pinMode(snoozePin, INPUT_PULLUP);
  pinMode(hourPin, INPUT_PULLUP);
  pinMode(minutePin, INPUT_PULLUP);
  pinMode(TEST_ledPin, OUTPUT);
}

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
  if(currentState == SET_TIME || currentState == SET_ALARM) {
    if(!dots) {
      matrix.clear();
      matrix.writeDisplay();
      return;
    }
  }

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

void changeState() {
    if (digitalRead(snoozePin) == LOW) {
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

void changeTime() {
  if (digitalRead(hourPin) == LOW){
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
  } else if (digitalRead(minutePin) == LOW) {
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

void buttonCheck() {

  boolean hourPressed = digitalRead(hourPin) == LOW;
  boolean minutePressed = digitalRead(minutePin) == LOW;
  boolean snoozePressed = digitalRead(snoozePin) == LOW;

  if(waitingOnButtonLift && (hourPressed || minutePressed || snoozePressed)) {
    Serial.println("Waiting on button press ");
    Serial.println(waitingOnButtonLift);
    Serial.println(hourPressed);
    Serial.println(minutePressed);
    Serial.println(snoozePressed);
    return;
  }

  waitingOnButtonLift = false;

  digitalWrite(TEST_ledPin, snoozePressed ? LOW : HIGH);

  if (hourPressed && minutePressed)
  {
    changeState();
  }
  else if (hourPressed || minutePressed) {
    changeTime();
  }
  else if (snoozePressed && currentState==ALARM) {
    snooze();
  }
}

void alarmCheck() {
  DateTime now = rtc.now();
  int t = now.hour()*100 + now.minute();
  if(t == alarm._time)
    startAlarm();
}

void printTime(int t) {
  matrix.print(t % 1200);
  // Show dot for AM/PM
//  matrix.writeDigitNum(2, i++, true);
}

void snooze() {
  /*
   * Calculate new time of alarm and silence
   */
}

void startAlarm()
{
  /*
   * Play different track depending on times snoozed
   */
}

unsigned long sec(unsigned long x)
{
  return x*1000000;
}
