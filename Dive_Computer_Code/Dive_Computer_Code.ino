/******************************************************************
 * This is the Arduino Micro code for the DIY Dive Computer. 
 * 
 * Written by Victor Konshin.
 * BSD license, check license.txt for more information.
 * All text above must be included in any redistribution.
 ******************************************************************/


#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "RTClib.h"

#define OLED_RESET 4
#define HEADER_DATA_GAP 9
//#define SHOW_LAYOUT

Adafruit_SSD1306 display(OLED_RESET);
RTC_DS1307 RTC;
unsigned long divestart = 0;
int diveTime = 0;
byte depth = 0;
byte maxDepth = 0;
byte nitrogen = 0;
byte oxygen = 10;
byte secondStore = 0;
byte batteryLevel = 255;

boolean clockBlink = false;
boolean time12Hour = true;
boolean batteryBlink = false;
boolean nitrogenBlink = false;
boolean oxygenBlink = false;

void setup()   {                
  Serial.begin(57600);
  Wire.begin();
  RTC.begin();

  if (! RTC.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }
  DateTime now = RTC.now();
  divestart = now.unixtime();

  display.begin(SSD1306_SWITCHCAPVCC, 0x3D);  // initialize with the I2C addr 0x3D
  display.clearDisplay();
  display.setTextColor(WHITE);

  delay(500);
}


void loop() {
  DateTime now = RTC.now();

  diveTime = now.unixtime() - divestart;

  display.clearDisplay();

  depth = analogRead(A0) / 4;

  if (depth > maxDepth) maxDepth = depth;

  //drawDepth( 0, 32, 3, false );
  //drawClock( 0, 0, 2, true);
  drawDiveTime(diveTime, 61, 0, 2, false); // Parameters: Time in seconds, x, y, size, bold
  drawClock(61, 25, 1, false); // Parameters: x, y, size, bold
  drawDepth( maxDepth, 14, 32, 2, true, "Max:"); // Parameters: depth, x, y, size, bold, header string
  drawDepth( depth, 4, 0, 3, true, "Depth:"); // Parameters: depth, x, y, size, bold, header string
  drawSaturation(nitrogen, true);
  drawSaturation(oxygen, false);
  drawBatteryIndicator(batteryLevel, 80, 50);
  nitrogen++;
  oxygen++;
  batteryLevel--;
  display.display();

}

void drawBatteryIndicator(byte value, byte x, byte y) {

  float level = ( 15.0 / 255.0 ) * value;

  if (value < 85) {
    if (batteryBlink) {
      batteryBlink = !batteryBlink;
      return;
    }
    batteryBlink = !batteryBlink;
  }

  display.drawRect(x, y, 15, 7, WHITE);
  display.fillRect(x, y, level, 7, WHITE);
  // the battery nub
  display.drawLine(x + 15, y + 2, x + 15, y + 4, WHITE);
  //display.drawLine(101, 59, 101, 61, WHITE);
}

void drawSaturation(byte value, boolean nitrogen) {

  float level = ( 53.0 / 255.0 ) * value;
  Serial.println(level);

  display.setTextSize(1);
  byte x = 0;

  if (value > 170) {
    if (nitrogen) {
      if (nitrogenBlink) {
        nitrogenBlink = !nitrogenBlink;
        return;
      }
      nitrogenBlink = !nitrogenBlink;
    } 
    else {
      if (oxygenBlink) {
        oxygenBlink = !oxygenBlink;
        return;
      }
      oxygenBlink = !oxygenBlink;
    }
  }

  if (nitrogen) {
    display.setCursor(0,54);
    display.println("N");
  } 
  else {
    display.setCursor(122,54);
    display.println("O");
    x = 122;
  }
  display.fillRect(x, 53 - level, 5, level, WHITE);
}

// Good up to 99 min and 99 sec. Font sizes 1-4 are valid
void drawDiveTime(unsigned long seconds, byte x, byte y, byte size, boolean bold) {

  String timeString = secondsToString( seconds );

  int width = ( ( 6 * 5) * size) - size;
  int height = (7 * size) + HEADER_DATA_GAP;
  if ( width < 59 ) width = 59;

  display.setCursor( ( ( width - 59 ) / 2) + x, y );
  display.setTextSize(1);
  display.println("Dive Time:");

  display.setTextSize(size);
  if ( width > 59 ) {
    display.setCursor( x, y + HEADER_DATA_GAP );
  } 
  else {
    display.setCursor( ( ( width - ( ( 6 * 5 ) * size ) ) / 2 ) + x, y + HEADER_DATA_GAP);
  }

  display.print(timeString);

  if ( bold ) {
    if ( width > 59 ) {
      display.setCursor( x + 1, y + HEADER_DATA_GAP );
    } 
    else {
      display.setCursor( ( ( width - ( ( 6 * 5 ) * size ) ) / 2 ) + x + 1, y + HEADER_DATA_GAP);    
    }
    display.print(timeString);
  }

#ifdef SHOW_LAYOUT
  display.drawRect(x, y, width, height, WHITE);
#endif
}

void drawClock(byte x, byte y, byte size, boolean bold) {

  String timeString;

  DateTime now = RTC.now();  
  int hour = now.hour();
  int seconds = now.second();

  if (time12Hour) {
    if (now.hour() > 12) {
      hour -= 12;
    }
  }

  if ( hour < 10 ) {
    timeString = " ";
    timeString = String( timeString + hour );
  } 
  else {
    timeString = String( hour );
  }

  if (clockBlink) {
    timeString = String( timeString + ':' );
  } 
  else {
    timeString = String( timeString + ' ' );
  }

  if (secondStore != seconds) {
    secondStore = seconds;
    clockBlink = !clockBlink;

  }


  if (now.minute() < 10) {
    timeString = String( timeString + '0' );
  }

  timeString = String( timeString + now.minute() );

  int width = ( ( 6 * timeString.length()) * size) - size;
  int height = (7 * size) + HEADER_DATA_GAP;
  if ( width < 29 ) width = 29;

  display.setCursor( ( ( width - 29 ) / 2) + x, y );
  display.setTextSize(1);
  display.println("Time:");

  display.setTextSize(size);
  if ( width > 29 ) {
    display.setCursor( x, y + HEADER_DATA_GAP );
  } 
  else {
    display.setCursor( ( ( width - ( ( 6 * 5 ) * size ) ) / 2 ) + x, y + HEADER_DATA_GAP);
  }

  timeString.replace('0', 'O');
  display.print(timeString);

  if ( bold ) {
    if ( width > 29 ) {
      display.setCursor( x + 1, y + HEADER_DATA_GAP );
    } 
    else {
      display.setCursor( ( ( width - ( ( 6 * 5 ) * size ) ) / 2 ) + x + 1, y + HEADER_DATA_GAP);    
    }
    display.print(timeString);
  }

#ifdef SHOW_LAYOUT
  display.drawRect(x, y, width, height, WHITE);
#endif
}


// Good up to three digits. Sizes 1-4 are valid
void drawDepth(byte value, byte x, byte y, byte size, boolean bold, String header) {

  int headerLength = header.length();
  int headerWidth = ( 6 * headerLength);

  int width = ( ( 6 * 3 ) * size) - size + 1;
  int height = (7 * size) + HEADER_DATA_GAP;

  if ( width < headerWidth ) width = headerWidth;

  int depthWidth = 5 * size;
  if (value > 9) {
    depthWidth = 11 * size;
  }
  if (value > 99) {
    depthWidth = 17 * size;
  }

  display.setCursor( ( ( width - headerWidth ) / 2) + x + 1, y );
  display.setTextSize(1);
  display.println(header);

  display.setTextSize(size);
  display.setCursor( ( ( width - depthWidth ) / 2 ) + x + 1, y + HEADER_DATA_GAP );
  String depthString =  String(value);
  depthString.replace( '0', 'O' );
  display.print(depthString);

  if ( bold ) {
    display.setCursor( ( ( ( width - depthWidth ) / 2 ) + x) + 2, y + HEADER_DATA_GAP );
    display.print(depthString);
  }

#ifdef SHOW_LAYOUT
  display.drawRect(x, y, width, height, WHITE);
#endif
}

String secondsToString( unsigned long diveSeconds) {

  int minutes = diveSeconds / 60;
  int seconds = diveSeconds % 60;

  // In case of overflow
  if (minutes > 99) minutes = minutes % 99;

  String timeString;

  if ( minutes < 10 ) {
    timeString = String( '0' );
    timeString = String( timeString + minutes );
  } 
  else {
    timeString = String( minutes );
  }

  timeString = String( timeString + ':' );

  if ( seconds < 10 ) {
    timeString = String( timeString + '0' + seconds );
  } 
  else {
    timeString = String( timeString + seconds );
  }

  timeString.replace('0', 'O');

  return timeString;
}





























