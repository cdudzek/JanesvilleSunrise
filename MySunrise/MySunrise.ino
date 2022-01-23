/*
 * "Janesville Sunrise"
 * December 2018 - Mr. D.
 * https://github.com/cdudzek/JanesvilleSunrise
 * 
 * Free to use, no warranty. If it breaks, not my problem.
 * See LICENSE document for Apache License 2.0
 */

/* CUSTOM BUILD TASKS:
 *   
 *  The default alarm clock is set with TimeSpan AlarmTime(...). Alarm settings will be saved to the Arduino EEPROM once changed.
 *  
 *  Change ledPin to match the pin for your LED output voltage. Must be a PWM-capable pin on Arduinos.
 *  
 *  Change encLow and encHigh to the pins attached to your rotary encoder. They do not need to be interrupt-capable pins. 
 *  Change encBtn to one pin attached to the rotary encoder's button. This does not need to be an interrupt-capable pin. 
 *  Attach the rotary common/center pin to ground.
 *  Connect the other rotary button pin with a 10K resistor to ground (like a standard input button).
 *  To change the direction of the dial (CW / CCW) just reverse the encLow and encHigh values or swap the wires on the encoder itself.
 *  
 *  Change the address of your 7-Seg LED at SEVENSEG_ADDR if necessary.
 *  
 *  Change the TIME_TO_FADE to the number of seconds for the lights to fade in from completely off to completely on. Ex. 20 minutes = 20*60
 *  Note the Alarm Time setting is for peak brightness, not the beginning of the fade-in. Lights will fade-out for the same duration after the peak alarm time.
 *  
 *  Change the AlarmOnDays array to turn the sunrise alarm on or off for each day of the week (Sunday through Saturday).
 */

/* Build information and libraries:
 *
 *  Required libraries: 
 *  Adafruit LED Backpack library: https://github.com/adafruit/Adafruit_LED_Backpack
 *  Adafruit GFX library: https://github.com/adafruit/Adafruit-GFX-Library
 *  ClickEncoder library: https://github.com/0xPIT/encoder/tree/arduino
 *  TimerOne library: http://playground.arduino.cc/Code/Timer1
 *  
 *  Products in use:
 *  Adafruit Blue 7-Segment LED with I2C backpack: https://www.adafruit.com/product/881
 *  Rotary Encoder (control knob): https://www.adafruit.com/product/377
 *  DS3231 RTC (Real-Time Clock): https://www.adafruit.com/product/3013
 *  Arduino Mega Protoshield (for circuitry): https://www.adafruit.com/product/192
 *  Arduino Mega 2560: https://store.arduino.cc/usa/arduino-mega-2560-rev3
 *  N-Channel MOSFET to drive LED power: I used an IRF530, a compatible product would be: https://www.adafruit.com/product/355
 *  
 *  Panel jacks such as https://www.adafruit.com/product/610 can be used to mount input/output connectors on your enclosure.
 *  If you desire an external USB connection for debugging/updating, consider a panel-mounted cable 
 *  such as https://www.adafruit.com/product/907 or https://www.adafruit.com/product/3258, depending on your board's USB port.
 *  
 *  Tested on Arduino Mega 2560, should work with Arduino Uno.
 *  Discovered the Teensy 3.2 does not have enough oomph to drive my 12V LED strands, may work with lower power output. 
 *  
 *  Circuitry built on Mega Protoshield, but any breadboard/protoboard could be used or a custom PCB could be made for this. 
 *  
 *  The DS3231 RTC will keep time accurately, and will maintain it during power loss if the battery is installed. Cell battery may last over 5 years.
 *  
 *  I built the clock into a wooden jewelry box enclosure (from Hobby Lobby). Size 5-7/8" x 4-3/4" x 3-1/4" was able to fit Arduino Mega.
 *  Holes were cut in the top lid for the clock display and rotary encoder/button, and in the rear for the input power and LED output connectors. 
 */

#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>
#include <ClickEncoder.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <RTClib.h>
#include <TimerOne.h>

// TODO: Quick settings for Teensy or Mega pinouts
#define USING_TEENSY_SETTINGS
//#define USING_MEGA_SETTINGS

// TODO: Update pins for LED output and encoder/button if necessary
#ifdef USING_MEGA_SETTINGS
#define ledPin    8
#define encLow    2
#define encHigh   3
#define encBtn    19
// LCD character display pins (14-pin or 16-pin header)
// 1=GND, 2=+5v, 3=Pot/Contrast, 4=RS, 5=RW/GND, 6=EN, 7-10=empty, 11-14=DS4-DS7, 15=+Backlight, 16=-Backlight
#define LCD_RS    9
#define LCD_EN    10
#define LCD_D4    4
#define LCD_D5    5
#define LCD_D6    6
#define LCD_D7    7
bool USE_LCD = true;
bool USE_SEVENSEG = false;

#elif defined USING_TEENSY_SETTINGS
#define ledPin    9
#define encLow    15
#define encHigh   16
#define encBtn    12
// LCD character display pins (14-pin or 16-pin header)
// 1=GND, 2=+5v, 3=Pot/Contrast, 4=RS, 5=RW/GND, 6=EN, 7-10=empty, 11-14=DS4-DS7, 15=+Backlight, 16=-Backlight
#define LCD_RS    9
#define LCD_EN    10
#define LCD_D4    4
#define LCD_D5    5
#define LCD_D6    6
#define LCD_D7    7
bool USE_LCD = false;
bool USE_SEVENSEG = true;
#endif

// TODO: Update the I2C address for the 7-Segment LED backpack if necessary
#define SEVENSEG_ADDR  0x70

// TODO: Update for 16x2 or 20x4 LCD display
#define LCD_ROWS 4
#define LCD_COLS 20
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// TODO: Amount of time (in seconds) to fade in the lighting
// 20 minutes:
#define TIME_TO_FADE (10*60) 

// TODO: Adjust Max LED PWM brightness based on your controller
// Arduino 8-bit = 255
// Teensy 10-bit = 1023, 12-bit = 4095
#define LED_MAX_PWM_OUTPUT 255
#define ANALOG_RESOLUTION_BITS 8

// TODO: Turn the alarm on/off for each day of the week
bool AlarmOnDays[] = { false, true, true, true, true, false, false };   // array={Su,M,Tu,W,Th,F,Sa}
const String DaysOfTheWeek[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};    // For serial debugging output

#define CONST_DEAD 0xDEAD
#define CONST_BEEF 0xBEEF
#define CONST_SETTINGS_VERSION 1
struct ClockSettings {
  unsigned int dead = CONST_DEAD;    // To test if settings are read correctly from EEPROM
  int SettingsVersion;
  int LCDBrightness;    // 0-15
  int MaxLEDBrightness; // 0 - LED_MAX_PWM_OUTPUT
  bool Use24HR;
  bool AlarmDays[7];
  int Alarm1Hours;
  int Alarm1Minutes;
  int Alarm2Hours;        // Alarm2 not currently in use
  int Alarm2Minutes;
  bool Alarm1Enabled;
  bool Alarm2Enabled;
  unsigned int beef = CONST_BEEF;    // To test if settings are read correctly from EEPROM
};

ClockSettings CurrentSettings;      // Where current settings are stored in memory
Adafruit_7segment matrix = Adafruit_7segment();
RTC_DS3231 rtc;
ClickEncoder *encoder;
ClickEncoder::Button encoderButton;
long oldPosition  = 0;    // For encoder position
long newPosition  = 0;    // For encoder position
bool UserSunrise;     // Temporary (user-started) sunrise
bool Cancelled;   // Allow the light to be turned off
DateTime UserDateTime;
DateTime CurrentTime;
DateTime AlarmDateTime;
TimeSpan AlarmTime(0, 6, 40, 0);    // Default alarm time = (0, Hour, Minute, Second); Can be set using the rotary dial
int hours, minutes, seconds;
long millisec;

/*
 * Displaying characters on 7-segment (non-alpha)
 * values for the segments are :
  1 = top
  2 = upper right
  4 = lower right
  8 = bottom
  16 = lower left
  32 = upper left
  64 = middle
 */
#define CHAR7_A 0x77
#define CHAR7_b 0x7c
#define CHAR7_C 0x39
#define CHAR7_c 0x58
#define CHAR7_d 0x5e
#define CHAR7_E 0x79
#define CHAR7_h 0x74
#define CHAR7_H 0x76
#define CHAR7_L 0x38
#define CHAR7_l 0x30
#define CHAR7_n 0x54
#define CHAR7_O 0x3F
#define CHAR7_o 0x5c
#define CHAR7_P 0x73
#define CHAR7_r 0x50
#define CHAR7_t 0x78
#define CHAR7_u 0x1c

void setup() {
  Serial.begin(9600);

  // Default Settings
  CurrentSettings.LCDBrightness = 15;
  CurrentSettings.MaxLEDBrightness = LED_MAX_PWM_OUTPUT;
  CurrentSettings.Use24HR = false;
  CurrentSettings.Alarm1Hours = AlarmTime.hours();
  CurrentSettings.Alarm1Minutes = AlarmTime.minutes();
  CurrentSettings.Alarm2Hours = AlarmTime.hours();
  CurrentSettings.Alarm2Minutes = AlarmTime.minutes();
  CurrentSettings.Alarm1Enabled = true;
  CurrentSettings.Alarm2Enabled = false;
  ReadEEprom();
  CurrentSettings.dead = CONST_DEAD;
  CurrentSettings.beef = CONST_BEEF;
  CurrentSettings.SettingsVersion = CONST_SETTINGS_VERSION;

  if (! rtc.begin()) {    
    while (1) {
      Serial.println("Couldn't find RTC");
    }
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  hours = minutes = seconds = 0; 
  Cancelled = false;
  UserSunrise = false;
  UserDateTime = rtc.now(); // Initialize variable, though we are not using it yet

  // Clear the displays
  if (USE_SEVENSEG)
  {
    matrix.begin(SEVENSEG_ADDR);
    matrix.setBrightness(CurrentSettings.LCDBrightness % 16);    // 0-15
  }
  if (USE_LCD)
    lcd.begin(LCD_COLS, LCD_ROWS);

#ifdef USING_TEENSY_SETTINGS
  analogWriteResolution(ANALOG_RESOLUTION_BITS);
#endif
  
  pinMode(ledPin, OUTPUT);
  encoder = new ClickEncoder(encLow, encHigh, encBtn, 4, LOW);    // Adafruit #377 encoder = 4 steps/notch
  encoder->setAccelerationEnabled(false);
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);   // Timer1 to service encoder
}

void loop() {
  GetCurrentTime();
  if ((CurrentTime.hour() != hours) || (CurrentTime.minute() != minutes) || (CurrentTime.second() != seconds)) {
    hours = CurrentTime.hour();
    minutes = CurrentTime.minute();
    seconds = CurrentTime.second();
    UpdateDisplay();
  }

  // See if user wants to do anything
  CheckButtonState();

  // Adjust the lights
  int fade = max(GetAlarmFade(), GetUserFade());
  analogWrite(ledPin, map(fade, 0, LED_MAX_PWM_OUTPUT, 0, CurrentSettings.MaxLEDBrightness));
}

// timerIsr(): called once per ms by Timer1 to handle rotary encoder without using interrupt pins
void timerIsr() {
  encoder->service();
}

// FormatHMS(): return HH or MM string with or without zeroes for proper formatting
String FormatHMS(int val, bool addzero = true) {
  if (addzero && (val < 10))
    return ("0" + String(val));
  else if (val < 10)
    return (" " + String(val));
  else
    return String(val);
}

String CenterString(String str, int columns = LCD_COLS) {  
  int diff = (columns - str.length())/2;
  for (int sp = 0; sp < diff; sp++)
    str = " " + str;
  for (int sp = str.length(); sp < columns; sp++)
    str += " ";   
  return str.substring(0, 20);
}

// UpdateDisplay(): Update the 7-segment clock display
void UpdateDisplay() {
  int hr = hours;
  String ampm = "";

  if (!CurrentSettings.Use24HR)  
  {
    if (hr > 12)
    {
      hr -= 12;
      ampm = " PM";
    }
    else if (hr == 12)
      ampm = " PM";
    else 
      ampm = " AM";
  }
  
  if (USE_SEVENSEG)
  {
    matrix.print((hr*100) + minutes);
    matrix.drawColon(true);
    matrix.writeDisplay();
  }
  if (USE_LCD)
  {
    //lcd.clear();
    String str;
    lcd.setCursor(0, 0);
    str = String(hr) + ":" + FormatHMS(minutes) + ":" + FormatHMS(seconds) + ampm;
    lcd.print(CenterString(str, LCD_COLS));
    lcd.setCursor(0, 1);
    if (LCD_COLS == 20)
      str = DaysOfTheWeek[CurrentTime.dayOfTheWeek()];
    else
      str = DaysOfTheWeek[CurrentTime.dayOfTheWeek()].substring(0,3);
    str += " " + FormatHMS(CurrentTime.month()) + "/" + FormatHMS(CurrentTime.day()) + "/" + CurrentTime.year(); 
    lcd.print(CenterString(str, LCD_COLS));
    
    if (LCD_ROWS == 4)
    {
      lcd.setCursor(0, 2);
      ampm = "";
      hr = AlarmDateTime.hour();
      if (!CurrentSettings.Use24HR)  
      {
        if (hr > 12)
        {
          hr -= 12;
          ampm = " PM";
        }
        else 
          ampm = " AM";
      }
      int next = NextAlarmDayOfWeek();
      str = "Alarm ";
      if ((next < 0 || next >= 7) || !CurrentSettings.Alarm1Enabled)
      {
        str += "Off";
        lcd.print(CenterString(str, LCD_COLS));
        lcd.setCursor(0, 3);
        lcd.print(CenterString(" ", LCD_COLS));
      }
      else
      {
        str += String(hr) + ":" + FormatHMS(AlarmDateTime.minute()) + ampm;
        lcd.print(CenterString(str, LCD_COLS));

        lcd.setCursor(0, 3);
        if (next >= 0 && next < 7)
        {
          str = "Next: ";
          if (next == CurrentTime.dayOfTheWeek() && 
            (CurrentTime.hour() < AlarmDateTime.hour()) &&
            (CurrentTime.minute() < AlarmDateTime.minute()))
            str += "Today";
          else if (next == (CurrentTime.dayOfTheWeek()+1)%7)
            str += "Tomorrow";
          else 
            str += DaysOfTheWeek[next];
          lcd.print(CenterString(str, LCD_COLS));
        }
      }      
    }
  }
  
  Serial.println(DaysOfTheWeek[CurrentTime.dayOfTheWeek()] + " " + FormatHMS(hours) + ":" + FormatHMS(minutes) + ":" + FormatHMS(seconds) + " (" + String(millisec) + " ms) Fade=" + String(GetAlarmFade()));
}

int NextAlarmDayOfWeek() {
  bool enabled = false;
  // check if at least one day is enabled
  for (int d = 0; d < 7; d++)
    if (AlarmOnDays[d])
      enabled = true;
  if (!enabled) 
    return -1;
    
  int dow = CurrentTime.dayOfTheWeek();
  dow %= 7; // just in case
  
  do 
  {
    if (AlarmOnDays[dow])
    {
      if (dow == CurrentTime.dayOfTheWeek() && 
        (hours < AlarmDateTime.hour()) &&
        (minutes < AlarmDateTime.minute()))
        break;
      else if (dow != CurrentTime.dayOfTheWeek())
        break;
      else
        dow++;
    }
    else 
      dow++;  // increment
    dow %= 7; // loop around 7 days
  }
  while (dow != CurrentTime.dayOfTheWeek());

  //Serial.println("Next alarm day is " + DaysOfTheWeek[dow]);
  
  return dow;  
}

// ReadEEprom(): restore settings after power loss
void ReadEEprom() {
  ClockSettings tmpset;
  unsigned int offset = 0;
  unsigned int dead = 0;
  unsigned int beef = 0;
  String stmp; 
  EEPROM.get(offset, dead);
  stmp = String(dead, HEX);
  stmp.toUpperCase();
  Serial.println("Offset " + String(offset) + ": dead = 0x" + stmp);
  offset += sizeof(dead);
  EEPROM.get(offset, tmpset.SettingsVersion);
  Serial.println("Offset " + String(offset) + ": SettingsVersion = " + String(tmpset.SettingsVersion));
  offset += sizeof(ClockSettings::SettingsVersion);
  EEPROM.get(offset, tmpset.LCDBrightness);
  Serial.println("Offset " + String(offset) + ": LCDBrightness = " + String(tmpset.LCDBrightness));
  offset += sizeof(ClockSettings::LCDBrightness);
  EEPROM.get(offset, tmpset.MaxLEDBrightness);
  Serial.println("Offset " + String(offset) + ": MaxLEDBrightness = " + String(tmpset.MaxLEDBrightness));
  offset += sizeof(ClockSettings::MaxLEDBrightness);
  EEPROM.get(offset, tmpset.Use24HR);
  Serial.print("Offset " + String(offset));
  Serial.println(tmpset.Use24HR ? ": Use24HR" : ": Use12HR");
  offset += sizeof(ClockSettings::Use24HR);
  Serial.println("Offset " + String(offset) + ": AlarmDays[]");
  for (int d = 0; d < 7; d++) {
    EEPROM.get(offset, tmpset.AlarmDays[d]);
    offset += sizeof(bool);
  }
  EEPROM.get(offset, tmpset.Alarm1Hours);
  Serial.println("Offset " + String(offset) + ": Alarm1Hours = " + String(tmpset.Alarm1Hours));
  offset += sizeof(ClockSettings::Alarm1Hours);
  EEPROM.get(offset, tmpset.Alarm1Minutes);
  Serial.println("Offset " + String(offset) + ": Alarm1Minutes = " + String(tmpset.Alarm1Minutes));
  offset += sizeof(ClockSettings::Alarm1Minutes);
  EEPROM.get(offset, tmpset.Alarm2Hours);
  Serial.println("Offset " + String(offset) + ": Alarm2Hours = " + String(tmpset.Alarm2Hours));
  offset += sizeof(ClockSettings::Alarm2Hours);
  EEPROM.get(offset, tmpset.Alarm2Minutes);
  Serial.println("Offset " + String(offset) + ": Alarm2Minutes = " + String(tmpset.Alarm2Minutes));
  offset += sizeof(ClockSettings::Alarm2Minutes);
  EEPROM.get(offset, tmpset.Alarm1Enabled);
  Serial.print("Offset " + String(offset));
  Serial.println(tmpset.Alarm1Enabled ? ": Alarm 1 On" : ": Alarm 1 Off");
  offset += sizeof(ClockSettings::Alarm1Enabled);
  EEPROM.get(offset, tmpset.Alarm2Enabled);
  Serial.print("Offset " + String(offset));
  Serial.println(tmpset.Alarm2Enabled ? ": Alarm 2 On" : ": Alarm 2 Off");
  offset += sizeof(ClockSettings::Alarm2Enabled);
  EEPROM.get(offset, beef);
  stmp = String(beef, HEX);
  stmp.toUpperCase();
  Serial.println("Offset " + String(offset) + ": beef = 0x" + stmp);
  offset += sizeof(beef);
  Serial.println("Total length " + String(offset));  
  
  if (dead == 0xDEAD && beef == 0xBEEF) {
    CurrentSettings = tmpset;
    Serial.println("Successfully loaded settings from memory.");
  }
  else
    Serial.println("Could not load settings from memory.");
}

// WriteEEprom(): save settings to non-volatile memory in case of power loss
void WriteEEprom() {
  // From https://www.arduino.cc/en/Tutorial/EEPROMUpdate
  // "the EEPROM has also a limit of 100,000 write cycles per single location, therefore avoiding rewriting the same value in any location will increase the EEPROM overall life."
  ClockSettings tmpset;
  EEPROM.get(0, tmpset);
  bool putdays = false;  
  unsigned int offset = 0;
  String stmp;

  stmp = String(CurrentSettings.dead, HEX);
  stmp.toUpperCase();
  Serial.println("Offset " + String(offset) + ": dead = 0x" + stmp);
  if (tmpset.dead != CONST_DEAD)
    EEPROM.put(offset, CONST_DEAD);
  offset += sizeof(ClockSettings::dead);
  Serial.println("Offset " + String(offset) + ": SettingsVersion = " + String(CurrentSettings.SettingsVersion));
  if (tmpset.SettingsVersion != CurrentSettings.SettingsVersion)
    EEPROM.put(offset, CurrentSettings.SettingsVersion);
  offset += sizeof(ClockSettings::SettingsVersion);
  Serial.println("Offset " + String(offset) + ": LCDBrightness = " + String(CurrentSettings.LCDBrightness));
  if (tmpset.LCDBrightness != CurrentSettings.LCDBrightness)
    EEPROM.put(offset, CurrentSettings.LCDBrightness);
  offset += sizeof(ClockSettings::LCDBrightness);
  Serial.println("Offset " + String(offset) + ": MaxLEDBrightness = " + String(CurrentSettings.MaxLEDBrightness));
  if (tmpset.MaxLEDBrightness != CurrentSettings.MaxLEDBrightness)
    EEPROM.put(offset, CurrentSettings.MaxLEDBrightness);
  offset += sizeof(ClockSettings::MaxLEDBrightness);
  Serial.print("Offset " + String(offset));
  Serial.println(CurrentSettings.Use24HR ? ": Use24HR" : ": Use12HR");
  if (tmpset.Use24HR != CurrentSettings.Use24HR)
    EEPROM.put(offset, CurrentSettings.Use24HR);
  offset += sizeof(ClockSettings::Use24HR);
  Serial.println("Offset " + String(offset) + ": AlarmDays[]");
  for (int d = 0; d < 7; d++) {
    if (tmpset.AlarmDays[d] != CurrentSettings.AlarmDays[d])
      putdays = true;
  }
  if (putdays) {
    for (int d = 0; d < 7; d++) {
      EEPROM.put(offset, CurrentSettings.AlarmDays[d]);
      offset += sizeof(bool);
    }
  }
  else 
    offset += sizeof(ClockSettings::AlarmDays);
  Serial.println("Offset " + String(offset) + ": Alarm1Hours = " + String(CurrentSettings.Alarm1Hours));
  if (tmpset.Alarm1Hours != CurrentSettings.Alarm1Hours)
    EEPROM.put(offset, CurrentSettings.Alarm1Hours);
  offset += sizeof(ClockSettings::Alarm1Hours);
  Serial.println("Offset " + String(offset) + ": Alarm1Minutes = " + String(CurrentSettings.Alarm1Minutes));
  if (tmpset.Alarm1Minutes != CurrentSettings.Alarm1Minutes)
    EEPROM.put(offset, CurrentSettings.Alarm1Minutes);
  offset += sizeof(ClockSettings::Alarm1Minutes);
  Serial.println("Offset " + String(offset) + ": Alarm2Hours = " + String(CurrentSettings.Alarm2Hours));
  if (tmpset.Alarm2Hours != CurrentSettings.Alarm2Hours)
    EEPROM.put(offset, CurrentSettings.Alarm2Hours);
  offset += sizeof(ClockSettings::Alarm2Hours);
  Serial.println("Offset " + String(offset) + ": Alarm2Minutes = " + String(CurrentSettings.Alarm2Minutes));
  if (tmpset.Alarm2Minutes != CurrentSettings.Alarm2Minutes)
    EEPROM.put(offset, CurrentSettings.Alarm2Minutes);
  offset += sizeof(ClockSettings::Alarm2Minutes);
  Serial.print("Offset " + String(offset));
  Serial.println(CurrentSettings.Alarm1Enabled ? ": Alarm 1 On" : ": Alarm 1 Off");
  if (tmpset.Alarm1Enabled != CurrentSettings.Alarm1Enabled)
    EEPROM.put(offset, CurrentSettings.Alarm1Enabled);
  offset += sizeof(ClockSettings::Alarm1Enabled);
  Serial.print("Offset " + String(offset));
  Serial.println(CurrentSettings.Alarm2Enabled ? ": Alarm 2 On" : ": Alarm 2 Off");
  if (tmpset.Alarm2Enabled != CurrentSettings.Alarm2Enabled)
    EEPROM.put(offset, CurrentSettings.Alarm2Enabled);
  offset += sizeof(ClockSettings::Alarm2Enabled);
  stmp = String(CurrentSettings.beef, HEX);
  stmp.toUpperCase();
  Serial.println("Offset " + String(offset) + ": beef = 0x" + stmp);
  if (tmpset.beef != CONST_BEEF)
    EEPROM.put(offset, CONST_BEEF);
  offset += sizeof(ClockSettings::beef);
  Serial.println("Total length " + String(offset));  
}

// CheckButtonState(): Handle user actions on the rotary button
// Single-click - Cancel the sunrise if it is currently lit up, reset automatically when the alarm time has lapsed
// Double-click - Enter Setup Mode, turn dial to adjust, single click to advance to next step
// Hold-click - Start a sunrise immediately, does not repeat once it has lapsed
void CheckButtonState() {
  encoderButton = encoder->getButton();
  if (encoderButton != ClickEncoder::Open) {
    Serial.print("Button: ");
    switch (encoderButton) {
      case ClickEncoder::Pressed:
        // Don't do anything
        //Serial.println("ClickEncoder::Pressed");
        break;
      case ClickEncoder::Released:
        // Don't do anything
        //Serial.println("ClickEncoder::Released");
        break;
      case ClickEncoder::Held:
        StartUserFade();
        Serial.println("ClickEncoder::Held, starting sunrise");
        break;
      case ClickEncoder::Clicked:
        if (max(GetAlarmFade(), GetUserFade()) == 0)
        {
          // Alarm is not running
          CurrentSettings.Alarm1Enabled = !CurrentSettings.Alarm1Enabled;
        }
        // Turn off lights if they are on
        Cancelled = !Cancelled;
        UserSunrise = false;
        Serial.print("ClickEncoder::Clicked, ");
        Serial.println(Cancelled ? "Alarm Cancelled" : "Alarm Enabled");
        break;
      case ClickEncoder::DoubleClicked:
        Serial.println("ClickEncoder::DoubleClicked, entering setup mode");
        SetupMode();
        break;
      default: break;
    }
  }
}

// StartUserFade(): begin sunrise sequence right now, does not repeat automatically
void StartUserFade() {
  int h = 0;
  int m = 0;
  int s = 0;
  int ttf = TIME_TO_FADE;
  if (ttf > (60*60)) {    // over 1 hour
    s = ttf % (60*60);
    h = (ttf - s) / (60*60);  // number of hours
  }
  if (s > 60) {   // over 1 minute
    ttf = s;
    s = ttf % 60;
    m = (ttf - m) / 60; // number of minutes
  }
  UserDateTime = rtc.now() + TimeSpan(0,h,m,s);
  UserSunrise = true;
}

// GetUserFade(): Calculate the current sunrise intensity from the user-initiated sunrise
long GetUserFade() {
  long fade = 0;
  if (UserSunrise) {
    long diff = abs(UserDateTime.secondstime() - CurrentTime.secondstime());      
    if (diff >= 0 && diff < TIME_TO_FADE) {
      //fade = map(diff, TIME_TO_FADE, 0, 0, 255);
      long fmap = map(diff, TIME_TO_FADE, 0, 1, 505);
      fade = fmap * (fmap - 1) / 1000;    // non-linear rise in brightness
    }
  }

  // Reset alarm when time has passed
  if (fade == 0)
    UserSunrise = false;
    
  return fade;
}

// GetAlarmFade(): Calculate the current sunrise intensity from the alarm-initiated sunrise
long GetAlarmFade() {
  long fade = 0;
  if (AlarmOnDays[CurrentTime.dayOfTheWeek()]) {
    long diff = abs(AlarmDateTime.secondstime() - CurrentTime.secondstime());      
    if (diff >= 0 && diff < TIME_TO_FADE) {
      //fade = map(diff, TIME_TO_FADE, 0, 0, 255);
      long fmap = map(diff, TIME_TO_FADE, 0, 1, 505);
      fade = fmap * (fmap - 1) / 1000;    // non-linear rise in brightness
    }
  }

  // Clear Cancelled status to reset alarm when time has passed
  if (fade == 0)
    Cancelled = false;
  else if (Cancelled)
    fade = 0;
    
  return fade;
}

// GetCurrentTime(): Update the current time from the RTC and set the alarm for the current date
void GetCurrentTime() {
  millisec = millis();
  CurrentTime = rtc.now();

  // Update AlarmDateTime to today, at the specified time, at zero seconds (using TimeSpan to store the hr/min values)
  AlarmDateTime = DateTime(CurrentTime.year(), CurrentTime.month(), CurrentTime.day(), AlarmTime.hours(), AlarmTime.minutes(), 0);
}

// FlashDisplay(): Flash the entire display on/off, for use when entering settings modes
void FlashDisplay(int count = 3, int delayms = 100) {
  if (USE_SEVENSEG)
  {
    for (int c = 0; c < count; c++) {
      matrix.setBrightness(0);
      delay(delayms);
      matrix.setBrightness(CurrentSettings.LCDBrightness % 16);
      delay(delayms);
    }
  }
}

// SetupMode(): Allow the user to change all options. Begin by double-clicking the rotary button.
void SetupMode() {
  // Set the 12/24 hr mode
  // Set the Alarm Time (hours then mins)
  // Set the LCD brightness
  // Set the max LED output brightness
  // Save settings to EEPROM in case of power failure

  if (USE_LCD)
  {
    lcd.clear();
    lcd.setCursor(0,LCD_ROWS/2 - 1);
    lcd.print(CenterString("Setup Mode")); 
  }
  Set12hr24hr();
  SetClock();
  SetAlarm1Time();
  if (USE_SEVENSEG)
    SetLcdBrightness();
  SetMaxLedBrightness();
  encoder->setAccelerationEnabled(false);  
  analogWrite(ledPin, 0);
  WriteEEprom();
}

// Set12hr24hr(): Choose 12-hr or 24-hr display mode
void Set12hr24hr() {
  // Set the 12/24 hr mode


  if (USE_SEVENSEG)
  {
    FlashDisplay();
  }
  delay(200);
  if (USE_LCD)
  {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(CenterString("Clock Display"));
  }
  
  encoder->setAccelerationEnabled(false);
  while (encoder->getButton() != ClickEncoder::Clicked) {
    //encoder->service();
    newPosition += encoder->getValue();
    if (newPosition != oldPosition) {
      CurrentSettings.Use24HR = !CurrentSettings.Use24HR;
      oldPosition = newPosition;
      if (CurrentSettings.Use24HR)
        Serial.println("Using 24hr mode");
      else
        Serial.println("Using 12hr mode");
    }
    // Display 12hr / 24hr for setting display
    if (USE_SEVENSEG)
    {
      matrix.drawColon(false);
        
      if (CurrentSettings.Use24HR) {
        matrix.writeDigitNum(0, 2, false);
        matrix.writeDigitNum(1, 4, false);
      }
      else {
        matrix.writeDigitNum(0, 1, false);
        matrix.writeDigitNum(1, 2, false);
      }
      matrix.writeDigitRaw(3, CHAR7_h); //h
      matrix.writeDigitRaw(4, CHAR7_r); //r
      matrix.writeDisplay();     
    }
    if (USE_LCD)
    {
      lcd.setCursor(0,1);
      if (CurrentSettings.Use24HR)
        lcd.print(CenterString("24 Hour mode"));
      else
        lcd.print(CenterString("12 Hour (AM/PM)"));
    }
  }
}

// SetClock(): Set the RTC Clock time
void SetClock() {
  if (USE_SEVENSEG)
  {
    FlashDisplay(1);
    matrix.drawColon(false);
    matrix.writeDigitRaw(0, CHAR7_C);
    matrix.writeDigitRaw(1, CHAR7_l);
    matrix.writeDigitRaw(3, CHAR7_o);
    matrix.writeDigitRaw(4, CHAR7_c);
    matrix.writeDisplay();
  }
  if (USE_LCD)
  {
    lcd.clear();
    lcd.setCursor(0,LCD_ROWS/2 - 1);
    lcd.print(CenterString("Set Time"));
    lcd.setCursor(0,LCD_ROWS/2);
    lcd.print(CenterString(" "));
  }
  
  delay(400);
  GetCurrentTime();
  encoder->setAccelerationEnabled(false);  
  int newhr = SetClockHours();
  encoder->setAccelerationEnabled(true);
  int newmin = SetClockMinutes(newhr);
  
  rtc.adjust(DateTime(CurrentTime.year(), CurrentTime.month(), CurrentTime.day(), newhr, newmin, 0));
}

// SetClockHours(): Set hours for the RTC
int SetClockHours() {
  int hr1 = 0;
  int hr2 = 0;
  int hrtmp = CurrentTime.hour();
  int hrc = CHAR7_H;
  bool OnOff = true;

  if (USE_LCD)
  {
    lcd.clear();
    lcd.setCursor(0,LCD_ROWS/2 - 1);
    lcd.print(CenterString("Set Hour"));
    lcd.setCursor(0,LCD_ROWS/2);
    lcd.print(CenterString(" "));
  }
  // Flash "Hour A/P" (ex. "10 A") or "Hour24 H" (ex. "15 H")
  newPosition = hrtmp;
  while (encoder->getButton() != ClickEncoder::Clicked) {
    OnOff = !OnOff;
        
    // Calculate time
    if (CurrentSettings.Use24HR) {
      hrc = CHAR7_H;
      hr2 = hrtmp % 10;
      hr1 = (hrtmp - hr2) / 10;
    }
    else {
      if (hrtmp > 12) {
        hrc = CHAR7_P;
        hrtmp -= 12;
      }
      else if (hrtmp == 12) {
        hrc = CHAR7_P;
      }
      else if (hrtmp > 1) {
        hrc = CHAR7_A;
      }
      else if (hrtmp == 0) {    // 0-hour (12A)
        hrc = CHAR7_A;
        hrtmp = 12;
      }
      hr2 = hrtmp % 10;
      hr1 = (hrtmp - hr2) / 10;
    }
    // Update display
    if (USE_SEVENSEG)
    {
      if (OnOff) {
        matrix.drawColon(true);
        if (hr1 > 0)
          matrix.writeDigitNum(0, hr1, false);
        else 
          matrix.writeDigitRaw(0, 0);
        matrix.writeDigitNum(1, hr2, false);
      }
      else {
        matrix.drawColon(false);
        matrix.writeDigitRaw(0, 0);
        matrix.writeDigitRaw(1, 0);
      }
      matrix.writeDigitRaw(3, 0);
      matrix.writeDigitRaw(4, hrc);
      matrix.writeDisplay();
    }
    if (USE_LCD)
    {
      lcd.setCursor(0,LCD_ROWS/2);
      lcd.print(CenterString(String(hrtmp) + ":" + FormatHMS(CurrentTime.minute())));
    }
    newPosition += encoder->getValue();
    if (newPosition != hrtmp) {
      //Serial.print("Encoder " + String(newPosition) + ", ");
      if (newPosition < 0) newPosition = 23;
      newPosition %= 24;
      hrtmp = newPosition;
    }
    delay(150);
  }
  return hrtmp;
}

// SetClockMinutes(): Set minutes for the RTC
int SetClockMinutes(int hrtmp) {
  int hr1, hr2, min1, min2, mintmp;
  min1 = min2 = 0;
  //hrtmp = CurrentTime.hour();
  mintmp = CurrentTime.minute();
  if (!CurrentSettings.Use24HR) {
    if (hrtmp > 12)
      hrtmp -= 12;
    else if (hrtmp == 0)
      hrtmp = 12;
  }
  hr2 = hrtmp % 10;
  hr1 = (hrtmp - hr2) / 10;
  bool OnOff = true;

  if (USE_LCD)
  {
    lcd.clear();
    lcd.setCursor(0,LCD_ROWS/2 - 1);
    lcd.print(CenterString("Set Minutes"));
    lcd.setCursor(0,LCD_ROWS/2);
    lcd.print(CenterString(" "));
  }
  
  // Flash ":MIN" (ex. ":15")
  newPosition = mintmp;
  while (encoder->getButton() != ClickEncoder::Clicked) {
    OnOff = !OnOff;
        
    min2 = mintmp % 10;
    min1 = (mintmp - min2) / 10;
    // Update display
    if (USE_SEVENSEG)
    {
      if (hr1 > 0)
        matrix.writeDigitNum(0, hr1, false);
      else
        matrix.writeDigitRaw(0, 0);
      matrix.writeDigitNum(1, hr2, false);
      if (OnOff) {
        matrix.drawColon(true);
        matrix.writeDigitNum(3, min1, false);
        matrix.writeDigitNum(4, min2, false);
      }
      else {
        matrix.drawColon(false);
        matrix.writeDigitRaw(3, 0);
        matrix.writeDigitRaw(4, 0);
      }
      matrix.writeDisplay();
    }
    if (USE_LCD)
    {
      lcd.setCursor(0,LCD_ROWS/2);
      lcd.print(CenterString(String(CurrentTime.hour()) + ":" + FormatHMS(mintmp)));
    }
    newPosition += encoder->getValue();
    if (newPosition != mintmp) {
      //Serial.print("Encoder " + String(newPosition) + ", ");
      if (newPosition < 0) newPosition = 59;
      newPosition %= 60;
      mintmp = newPosition;
    }
    delay(150);
  }
  return mintmp;
}

// SetAlarm1Time(), SetAlarm1Hours(), SetAlarm1Minutes(): Set the peak alarm time (lights fully on)
void SetAlarm1Time() {
  // Set the Alarm1 time
    if (USE_LCD)
  {
    lcd.clear();
    lcd.setCursor(0,LCD_ROWS/2 - 1);
    lcd.print(CenterString("Set Alarm"));
    lcd.setCursor(0,LCD_ROWS/2);
    lcd.print(CenterString(" "));
  }
  FlashDisplay(1);
  if (USE_SEVENSEG)
  {
    matrix.drawColon(false);
    matrix.writeDigitRaw(0, CHAR7_A);
    matrix.writeDigitRaw(1, CHAR7_L);
    matrix.writeDigitRaw(3, 0);
    matrix.writeDigitNum(4, 1, false);
    matrix.writeDisplay();
  }
  delay(400);
  encoder->setAccelerationEnabled(false);  
  SetAlarm1Hours();
  encoder->setAccelerationEnabled(true);
  SetAlarm1Minutes();
  AlarmTime = TimeSpan(0, CurrentSettings.Alarm1Hours, CurrentSettings.Alarm1Minutes, 0);
}

void SetAlarm1Hours() {
  int hr1 = 0;
  int hr2 = 0;
  int hrtmp = 0;
  int hrc = CHAR7_H;
  bool OnOff = true;
  String ampm = "";

  if (USE_LCD)
  {
    lcd.clear();
    lcd.setCursor(0,LCD_ROWS/2 - 1);
    lcd.print(CenterString("Set Alarm Hour"));
    lcd.setCursor(0,LCD_ROWS/2);
    lcd.print(CenterString(" "));
  }
  
  // Flash "Hour A/P" (ex. "10 A") or "Hour24 H" (ex. "15 H")
  newPosition = CurrentSettings.Alarm1Hours;
  while (encoder->getButton() != ClickEncoder::Clicked) {
    OnOff = !OnOff;
        
    // Calculate time
    if (CurrentSettings.Use24HR) {
      hrc = CHAR7_H;
      hr2 = CurrentSettings.Alarm1Hours % 10;
      hr1 = (CurrentSettings.Alarm1Hours - hr2) / 10;
      ampm = "";
    }
    else {
      hrtmp = CurrentSettings.Alarm1Hours;
      if (hrtmp > 12) {
        hrc = CHAR7_P;
        hrtmp -= 12;
        ampm = " PM";
      }
      else if (hrtmp == 12) {
        hrc = CHAR7_P;
        ampm = " PM";
      }
      else if (hrtmp > 1) {
        hrc = CHAR7_A;
        ampm = " AM";
      }
      else if (hrtmp == 0) {    // 0-hour (12A)
        hrc = CHAR7_A;
        hrtmp = 12;
        ampm = " AM";
      }
      hr2 = hrtmp % 10;
      hr1 = (hrtmp - hr2) / 10;
    }
    // Update display
    if (USE_SEVENSEG)
    {
      if (OnOff) {
        matrix.drawColon(true);
        if (hr1 > 0)
          matrix.writeDigitNum(0, hr1, false);
        else 
          matrix.writeDigitRaw(0, 0);
        matrix.writeDigitNum(1, hr2, false);
      }
      else {
        matrix.drawColon(false);
        matrix.writeDigitRaw(0, 0);
        matrix.writeDigitRaw(1, 0);
      }
      matrix.writeDigitRaw(3, 0);
      matrix.writeDigitRaw(4, hrc);
      matrix.writeDisplay();
    }
    if (USE_LCD)
    {
      lcd.setCursor(0,LCD_ROWS/2);
      lcd.print(CenterString(String(hrtmp) + ":" + FormatHMS(CurrentSettings.Alarm1Minutes) + ampm));
    }
    
    newPosition += encoder->getValue();
    if (newPosition != CurrentSettings.Alarm1Hours) {
      //Serial.print("Encoder " + String(newPosition) + ", ");
      if (newPosition < 0) newPosition = 23;
      newPosition %= 24;
      CurrentSettings.Alarm1Hours = newPosition;
      Serial.println("Alarm 1 Time " + String(CurrentSettings.Alarm1Hours) + ":" + FormatHMS(CurrentSettings.Alarm1Minutes, true));
    }
    delay(150);
  }
}

void SetAlarm1Minutes() {
  String atime;
  String ampm = "";
  int hr1, hr2, hrtmp, min1, min2;
  min1 = min2 = 0;
  hrtmp = CurrentSettings.Alarm1Hours;
  if (CurrentSettings.Use24HR) {
    ampm = "";
    if (hrtmp > 12)
      hrtmp -= 12;
    else if (hrtmp == 0)
      hrtmp = 12;
  }
  else
  {
    if (hrtmp >= 12)
      ampm = " PM";
    else 
      ampm = " AM";
  }
  hr2 = hrtmp % 10;
  hr1 = (hrtmp - hr2) / 10;
  bool OnOff = true;

  if (USE_LCD)
  {
    lcd.clear();
    lcd.setCursor(0,LCD_ROWS/2 - 1);
    lcd.print(CenterString("Set Alarm Minutes"));
    lcd.setCursor(0,LCD_ROWS/2);
    lcd.print(CenterString(" "));
  }
  
  // Flash ":MIN" (ex. ":15")
  newPosition = CurrentSettings.Alarm1Minutes;
  while (encoder->getButton() != ClickEncoder::Clicked) {
    OnOff = !OnOff;
        
    min2 = CurrentSettings.Alarm1Minutes % 10;
    min1 = (CurrentSettings.Alarm1Minutes - min2) / 10;
    // Update display
    if (USE_SEVENSEG)
    {
      if (hr1 > 0)
        matrix.writeDigitNum(0, hr1, false);
      else
        matrix.writeDigitRaw(0, 0);
      matrix.writeDigitNum(1, hr2, false);
      if (OnOff) {
        matrix.drawColon(true);
        matrix.writeDigitNum(3, min1, false);
        matrix.writeDigitNum(4, min2, false);
      }
      else {
        matrix.drawColon(false);
        matrix.writeDigitRaw(3, 0);
        matrix.writeDigitRaw(4, 0);
      }
      matrix.writeDisplay();
    }
    if (USE_LCD)
    {
      lcd.setCursor(0,LCD_ROWS/2);
      lcd.print(CenterString(String(hrtmp) + ":" + FormatHMS(CurrentSettings.Alarm1Minutes) + ampm));
    }
    
    newPosition += encoder->getValue();
    if (newPosition != CurrentSettings.Alarm1Minutes) {
      Serial.print("Encoder " + String(newPosition) + ", ");
      if (newPosition < 0) newPosition = 59;
      newPosition %= 60;
      CurrentSettings.Alarm1Minutes = newPosition;
      Serial.println("Alarm 1 Time " + String(CurrentSettings.Alarm1Hours) + ":" + FormatHMS(CurrentSettings.Alarm1Minutes, true));
    }
    delay(150);
  }
}

// SetLcdBrightness: Set the LCD display brightness, range 0-15 for Adafruit I2C backpack
void SetLcdBrightness() {  
  if (!USE_SEVENSEG)
    return;
      
  if (USE_LCD)
  {
    lcd.clear();
    lcd.setCursor(0,LCD_ROWS/2 - 1);
    lcd.print(CenterString("Set Clock Brightness"));
    lcd.setCursor(0,LCD_ROWS/2);
    lcd.print(CenterString(" "));
  }
  
  FlashDisplay(1);
  matrix.drawColon(false);
  matrix.writeDigitRaw(0, CHAR7_L);
  matrix.writeDigitRaw(1, CHAR7_c);
  matrix.writeDigitRaw(3, CHAR7_d);
  matrix.writeDigitRaw(4, 0);
  matrix.writeDisplay();
  delay(400);
  encoder->setAccelerationEnabled(false);
  newPosition = CurrentSettings.LCDBrightness;
  while (encoder->getButton() != ClickEncoder::Clicked) {
    //encoder->service();
    matrix.print(CurrentSettings.LCDBrightness);
    matrix.writeDisplay();
    if (USE_LCD)
    {
      lcd.setCursor(0,LCD_ROWS/2);
      lcd.print(CenterString(String(CurrentSettings.LCDBrightness)));
    }
    
    newPosition += encoder->getValue();
    if (newPosition != CurrentSettings.LCDBrightness) {
      //Serial.print("Encoder " + String(newPosition) + ", ");
      if (newPosition < 0) newPosition = 15;
      newPosition %= 16;
      CurrentSettings.LCDBrightness = newPosition;
      matrix.setBrightness(CurrentSettings.LCDBrightness);
      Serial.println("LCD Brightness = " + String(CurrentSettings.LCDBrightness));
    }
  }
}

// SetMaxLedBrightness(): Set the max LED output brightness for the Sunrise
void SetMaxLedBrightness() {
  if (USE_LCD)
  {
    lcd.clear();
    lcd.setCursor(0,LCD_ROWS/2 - 1);
    lcd.print(CenterString("Set Max Sunlight"));
    lcd.setCursor(0,LCD_ROWS/2);
    lcd.print(CenterString(" "));
  }
  if (USE_SEVENSEG)
  {
    FlashDisplay(1);
    matrix.drawColon(false);
    matrix.writeDigitRaw(0, CHAR7_O);
    matrix.writeDigitRaw(1, CHAR7_u);
    matrix.writeDigitRaw(3, CHAR7_t);
    matrix.writeDigitRaw(4, 0);
    matrix.writeDisplay();
  }
  delay(400);
  encoder->setAccelerationEnabled(true);
  if (CurrentSettings.MaxLEDBrightness > LED_MAX_PWM_OUTPUT || CurrentSettings.MaxLEDBrightness < 0)
    CurrentSettings.MaxLEDBrightness = LED_MAX_PWM_OUTPUT;
  newPosition = CurrentSettings.MaxLEDBrightness;
  while (encoder->getButton() != ClickEncoder::Clicked) {
    //encoder->service();
    if (USE_SEVENSEG)
    {
      matrix.print(CurrentSettings.MaxLEDBrightness);
      matrix.writeDisplay();
    }
    if (USE_LCD)
    {
      lcd.setCursor(0,LCD_ROWS/2);
      lcd.print(CenterString(String(CurrentSettings.MaxLEDBrightness)));
    }
    analogWrite(ledPin, CurrentSettings.MaxLEDBrightness);
    newPosition += encoder->getValue();
    if (newPosition != CurrentSettings.MaxLEDBrightness) {
      //Serial.print("Encoder " + String(newPosition) + ", ");
      if (newPosition < 0) newPosition = LED_MAX_PWM_OUTPUT;
      newPosition %= LED_MAX_PWM_OUTPUT+1;
      CurrentSettings.MaxLEDBrightness = newPosition;
      Serial.println("Max LED Brightness = " + String(CurrentSettings.MaxLEDBrightness));
    }
  }
}
