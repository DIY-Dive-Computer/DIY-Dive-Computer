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
#include <SD.h>
#include "RTClib.h"

#define OLED_RESET 3 // OLED Reset pin
#define HEADER_DATA_GAP 9
//#define SHOW_LAYOUT

Adafruit_SSD1306 display(OLED_RESET); // Allocate for the OLED

uint32_t diveStart = 0; // This stores the dive start time in unixtime.
int diveTime = 0;// This stores the current dive time in seconds
// Dive parameter storage
int depth = 0;
int maxDepth = 0;
int nitrogen = 0;
int oxygen = 10;

uint32_t lastDataRecord = 0; // Stores the last time data was recorded in unixtime.
uint32_t lastColonStateChange = 0; // Stores the last time the clock was updates to that the blink will happen at 1Hz.

DateTime now; // Stores the current date/time for use throughout the code.
const int chipSelect = 4; // For the SD card breakout board

// These will be user preferences
boolean time12Hour = true;
boolean clockBlink = true;
int recordInterval = 29; // This is the inteval in which it will record data to the SD card minus one.

// *******************************************************************
// ***************************** Setup *******************************
// *******************************************************************

void setup()   {                
  Serial.begin(9600);
  Wire.begin();

  // Make sure the real time clock is set
  Teensy3Clock.set(DateTime(__DATE__, __TIME__).unixtime());

  // initialize access to the SD card
  Serial.print("Initializing SD card...");
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return;
  }
  Serial.println("card initialized.");

  now = Teensy3Clock.get();
  lastDataRecord = Teensy3Clock.get();
  lastColonStateChange = lastDataRecord;

  diveStart = now.unixtime();

  // Display Setup
  display.begin(SSD1306_SWITCHCAPVCC, 0x3D);  // initialize with the I2C addr 0x3D
  display.clearDisplay();
  display.setTextColor(WHITE);

  // Give everything a half sec to settle
  delay(500);


}

// *******************************************************************
// **************************** Main Loop ****************************
// *******************************************************************


void loop() {
  now = DateTime(Teensy3Clock.get());
  diveTime = now.unixtime() - diveStart;

  //  ************* Display code *************

  display.clearDisplay();

  depth = analogRead(A0) / 4;

  if (depth > maxDepth) maxDepth = depth;

  //drawDepth( 0, 32, 3, false );
  //drawClock( 0, 0, 2, true);
  drawDiveTime(diveTime, 61, 0, 2, false); // Parameters: Time in seconds, x, y, size, bold
  drawClock(61, 25, 1, false); // Parameters: x, y, size, bold
  drawDepth( maxDepth, 6, 32, 3, false, "Max:"); // Parameters: depth, x, y, size, bold, header string
  drawDepth( depth, 6, 0, 3, false, "Depth:"); // Parameters: depth, x, y, size, bold, header string
  drawSaturation(nitrogen, true);
  drawSaturation(oxygen, false);

  nitrogen++;
  oxygen++;


  display.display();

  //  ************* Data logging code *************

  if ( now.unixtime() > (lastDataRecord + recordInterval) ) {

    lastDataRecord = now.unixtime();
    // make a string of data to log:
    String dataString = generateRecordDataString();

    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    File dataFile = SD.open("datalog.txt", FILE_WRITE);

    // if the file is available, write to it:
    if (dataFile) {
      dataFile.println(dataString);
      dataFile.close();
      // print to the serial port too:
      //Serial.println(dataString);
    }  
    else {
      Serial.println("error opening datalog.txt");
    } 
  }

}

// *******************************************************************
// ********************** Generate Record Data ***********************
// *******************************************************************

String generateRecordDataString() {

  String returnString = "";

  return returnString;  
}

// *******************************************************************
// ************************ Draw Saturation **************************
// *******************************************************************

void drawSaturation(int value, boolean nitrogen) {

  float level = ( 53.0 / 255.0 ) * value;

  display.setTextSize(1);
  int x = 0;
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

// *******************************************************************
// ************************* Draw Dive Time **************************
// *******************************************************************

// Good up to 99 min and 99 sec. Font sizes 1-4 are valid
void drawDiveTime(unsigned long seconds, int x, int y, int size, boolean bold) {

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

// *******************************************************************
// *************************** Draw Clock ****************************
// *******************************************************************

void drawClock(int x, int y, int size, boolean bold) {

  String timeString;

  int hour = now.hour();

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

  if (now.unixtime() > lastColonStateChange) {
    lastColonStateChange = now.unixtime();
    clockBlink = !clockBlink;
  }

  if (clockBlink) {
    timeString = String( timeString + ':' );
  } 
  else {
    timeString = String( timeString + ' ' );
  }


  int minutes = now.minute();

  if (minutes < 10) {
    timeString = String( timeString + '0' );
  }

  timeString = String( timeString + minutes );

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

// *******************************************************************
// *************************** Draw Depth ****************************
// *******************************************************************


// Good up to three digits. Sizes 1-4 are valid
void drawDepth(int value, int x, int y, int size, boolean bold, String header) {

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

// *******************************************************************
// ************************ Draw Accent Rate *************************
// *******************************************************************

void drawAccentRate(void) {
  uint8_t color = WHITE;
  for (int16_t i=min(display.width(),display.height())/2; i>0; i-=5) {
    display.fillTriangle(display.width()/2, display.height()/2-i,
                     display.width()/2-i, display.height()/2+i,
                     display.width()/2+i, display.height()/2+i, WHITE);
    if (color == WHITE) color = BLACK;
    else color = WHITE;
    display.display();
  }
}


// *******************************************************************
// ************************* Utility Methods *************************
// *******************************************************************

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
