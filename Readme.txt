# Clockduino

This is the source code for an Arduino-powered alarm clock I built from scratch.

## Parts & Libraries

Make sure to read the "Setup & Wiring" section on what is needed for setup for these.

  - [ChronoDot v2.1 (DS3231)](https://www.adafruit.com/products/255)
    - https://github.com/adafruit/RTClib
  - [Adafruit 1.2" 7-segment clock display](https://www.adafruit.com/products/1266)
    - https://github.com/adafruit/Adafruit_LED_Backpack
  - [Adafruit Music Maker Shield w/ amp (VS1053)](https://www.adafruit.com/products/1788)
    - https://github.com/adafruit/Adafruit_VS1053_Library
    - also need [Shield-stacking headers](https://www.adafruit.com/products/85)
  - https://github.com/bneedhamia/sdconfigfile
  - "Snooze" [LED Illuminated Pushbutton](https://www.adafruit.com/products/491)
  - 2 [Buttons](https://www.adafruit.com/products/367)
  - 2 [Speakers (4 Ohm, 3 Watt)](https://www.adafruit.com/products/1314)
  - a FAT32 microSD card


## Setup & Wiring

Of the above libraries, most can be installed through the Arduino IDE's built-in
library management. For the rest, a snapshot of the library project is available
in the `lib` directory. In the IDE go to `Sketch > Include Library > Add .ZIP Library`
and add everything in `lib`.

Wiring is detailed in `wiring.txt`, which can be viewed in the
[mermaid](https://knsv.github.io/mermaid/live_editor/) graphing tool online.

## Using the clock

Assuming you have everything plugged in and the code compiles fine, the clock
should start up and briefly show `8888` on the display and beep to test the sound
card.

In the event of an error during start up, the following codes will be shown:
  - E2 = Real Time Clock not detected
  - E4 = Sound card not detected
  - E7 = SD card (through) music player shield
  - E12 = DREQ is not an interrupt pin; incorrect wiring, most likely

The clock's default state is to show the current time. In this state, if you
press both the H & M buttons simultaneously, it will enter the _Menu_.

### Menu
  1. Arm/disarm alarm
  2. Set Time
  3. Set Alarm
  4. Set Snooze (minutes)
  5. Set Volume
  6. Set Brightness

To scroll between menu options, use the H & M buttons. To select an option, press
the Snooze button, then use the H & M buttons to adjust the option (time, volume, etc)
and when finished, press the Snooze button to return to the menu. When you're finished,
press both the H & M buttons to exit the menu and return to the clock.


### The Alarm
To dismiss the alarm, press both the H & M buttons simultaneously.

To snooze, press the snooze button. When snoozed, the snooze light will be turned on
to indicate that the alarm has not yet been dismissed.

The **top left** dot on the clock face indicates PM if it is lit, the **bottom left**
indicates that the alarm is armed.

### Sounds & Files
Below is a list of the files needed/used on the SD card for maximum operation:

  - config.cfg   = configuration; will be created automatically
  - alarming.mp3 = To play for alarm
  - mmenu000.mp3 = Alarm disarmed
  - mmenu001.mp3 = Alarm armed
  - mmenu002.mp3 = Set time
  - mmenu003.mp3 = Set alarm
  - mmenu004.mp3 = Set snooze
  - mmenu005.mp3 = Set volume
  - mmenu006.mp3 = Set brightness

If an audio file is missing, a beep will be used instead.
If the config file is missing, settings (e.g. alarm time) will not be saved across
reboots (i.e. power failure, unplug). Time will still stay in sync as long as the
RTC has a battery.
