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
#include "PITimer.h"

#define OLED_RESET 3 // OLED Reset pin
#define HEADER_DATA_GAP 9 // The gap between the heading line and the data line on most display items.
//#define SHOW_LAYOUT // Uncomment this to see bounding boxes on UI elements - makes laying out items easier.

Adafruit_SSD1306 display(OLED_RESET); // Allocate for the OLED

// Dive parameter storage
uint32_t  diveStart            = 0; // This stores the dive start time in unixtime.
int       diveTime             = 0; // This stores the current dive time in seconds
int       depth                = 0; // Current Depth
int       maxDepth             = 0; // Max Depth
int       nitrogen             = 0; // Current nitrogen saturation level
int       oxygen               = 0; // Current oxygen saturation level.
int       temperature          = 85; // Current temp.
int       batteryLevel         = 128; // Current battery capacity remaining.
int       displayMode          = 0; // Stores the current display mode: 0 = menu, 1 = dive, 2 = log, 3 = settings. 
int       currentMenuOption    = 0; // Stores which menu option is being displayed. 0 = dive, 1 = log, 2 = settings.
int       diveModeDisplay      = 0; // There are multiple dive mode displays. This is the one currently being viewed.

boolean   inWater              = false; // Goes true if the water sensor detects that the device is submerged.
int       inWaterCounter       = 0; //  Counter used to determine if the device is in water.
boolean   diveMode             = false; // Stores dive mode or surface mode state: 0 = surface mode, 1 = dive mode.
boolean   alertShowing         = false; // true = there is an alert showing. 
uint32_t  alertTime            = 0; // Stores time the alert was displayed.
uint32_t  lastDataRecord       = 0; // Stores the last time data was recorded in unixtime.
uint32_t  lastColonStateChange = 0; // Stores the last time the clock was updates to that the blink will happen at 1Hz.

DateTime  now;                      // Stores the current date/time for use throughout the code.
const int chipSelect           = 4; // For the SD card breakout board

// These will be user preferences
boolean   time12Hour           = true; // true = 12 hour time, false = 24 hour time.
boolean   clockBlink           = true; // true = blink the colon, false = don't blink the colon.
boolean   ferinheight          = true; // true = Ferinheight, false = Celsius.

int       recordInterval       = 29; // This is the inteval in which it will record data to the SD card minus one.

static unsigned char PROGMEM dive_logo_bmp[] =
{ 
  B00011111, B11111111, B11111111,
  B00001111, B11111111, B11111111,
  B00000111, B11111111, B11111111,
  B10000011, B11111111, B11111111,
  B11000001, B11111111, B11111111,
  B11100000, B11111111, B11111111,
  B11110000, B01111111, B11111111,
  B11111000, B00111111, B11111111,
  B11111100, B00011111, B11111111,
  B11111110, B00001111, B11111111,
  B11111111, B00000111, B11111111,
  B11111111, B10000011, B11111111,
  B11111111, B11000001, B11111111,
  B11111111, B11100000, B11111111,
  B11111111, B11110000, B01111111,
  B11111111, B11111000, B00111111,
  B11111111, B11111100, B00011111,
  B11111111, B11111110, B00001111,
  B11111111, B11111111, B00000111,
  B11111111, B11111111, B10000011,
  B11111111, B11111111, B11000001,
  B11111111, B11111111, B11100000,
  B11111111, B11111111, B11110000,
  B11111111, B11111111, B11111000 };

static unsigned char PROGMEM log_logo_bmp[] =
{   
  B00000100, B00000000, B00100000,
  B00000111, B00000000, B11100000,
  B00000111, B11000011, B11100000,
  B00110111, B11100111, B11101100,

  B00110111, B11100111, B11101100,
  B00110111, B11100111, B11101100,
  B10110111, B11100111, B11101101,
  B10110111, B11100111, B11101101,
  B10110111, B11100111, B11101101,
  B10110111, B11100111, B11101101,
  B10110111, B11100111, B11101101,
  B10110111, B11100111, B11101101,
  B10110111, B11100111, B11101101,


  B10110111, B11100111, B11101101,
  B10110111, B11100111, B11101101,
  B10110111, B11100111, B11101101,
  B10110111, B11100111, B11101101,

  B10110011, B11100111, B11001101,
  B10111110, B11100111, B01111101,
  B10111111, B00100100, B11111101,
  B10111111, B10100101, B11111101,
  B11100001, B11000011, B10000111,
  B11111111, B00100100, B11111111,
  B11111111, B11000011, B11111111 };

static unsigned char PROGMEM settings_logo_bmp[] =
{ 
  B00000000, B00000000, B00000000,
  B00000000, B00000000, B00110000,
  B00000000, B00000000, B11111100,
  B00000000, B00000000, B11111100,

  B00000000, B00000001, B11001110,
  B00000000, B00000001, B11001110,
  B00000000, B11000000, B11111100,
  B00010001, B11100010, B11111100,

  B00111001, B11100111, B00110000,
  B01111111, B11111111, B10000000,
  B00111111, B11111111, B00000000,
  B00011111, B11111110, B00000000,

  B00011111, B00111110, B00000000,
  B01111110, B00011111, B10000000,
  B11111100, B00001111, B11000000,
  B11111100, B00001111, B11000000,

  B01111110, B00011111, B10000000,
  B00011111, B00111110, B00000000,
  B00011111, B11111110, B00000000,
  B00111111, B11111111, B00000000,

  B01111111, B11111111, B10000000,
  B00111001, B11100111, B00000000,
  B00010001, B11100010, B00000000,
  B00000000, B11000000, B00000000 };

// *******************************************************************
// ***************************** Setup *******************************
// *******************************************************************

void setup()   {                
  Serial.begin(115200);
  Wire.begin();

  // Display Setup
  display.begin(SSD1306_SWITCHCAPVCC, 0x3D);  // initialize with the I2C addr 0x3D
  display.display(); // show splashscreen
  delay(2000);
  display.clearDisplay();
  display.display(); 
  display.setTextColor(WHITE);

  // Make sure the real time clock is set correctly and set some inital parameters
  Teensy3Clock.set(DateTime(__DATE__, __TIME__).unixtime());
  now = Teensy3Clock.get();
  lastDataRecord = Teensy3Clock.get();
  lastColonStateChange = lastDataRecord;
  diveStart = now.unixtime();

  // initialize access to the SD card
  Serial.print("Initializing SD card...");
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);

  //testing
  //showAlert (0, "You are about to     become bent and die! Time to surface!");
  //  Setup the user input buttons.
  pinMode(5,INPUT); // right button
  pinMode(6,INPUT); // left button
  attachInterrupt(5, rightPressed, FALLING);
  attachInterrupt(6, leftPressed, FALLING);

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    showAlert (0, "Failed to initialize the SD card.  Please make sure the card isinstalled correctly.");
  } 
  else {
    Serial.println("card initialized.");
  }


  // Give everything a half sec to settle
  delay(500);
}

// *******************************************************************
// *******************************************************************
// **************************** Main Loop ****************************
// *******************************************************************
// *******************************************************************

void loop() {

  now = DateTime(Teensy3Clock.get());
  depth = analogRead(A0) / 2;


  // In water detection.  This will tell if the user is in the water. Not exactly sure what it will be used for at this point but the code is here.
  if (analogRead(A1) == 0) {
    inWaterCounter++;
  } 
  else {
    inWaterCounter--;
  }

  if (inWaterCounter > 100) {
    inWater = true;
    if (inWaterCounter > 150) inWaterCounter = 150;
  }

  if ( inWaterCounter < 0 ) {
    inWater = false;
    inWaterCounter = 0;
  }

  //  Serial.print("In water counter: ");
  //  Serial.print(inWaterCounter);
  //  Serial.print(" In Water Flag: ");
  //  Serial.println(inWater);

  // Dive start detection.  When the user decends below 4 feet, then the dive will start. 
  if (diveMode == false && depth > 3) {
    diveMode = true;
    diveStart = now.unixtime(); // reset the timer.
    diveTime = 0;
    inWater = true;  // If you are 4 feet under water, it's safe to assume that you are in the water...
    inWaterCounter = 150;
    displayMode = 1; //  Since this conditional is only called when a dive starts, then put the display into dive mode if it's not already.
  } 

  // TODO: Probably need something similar to the inWaterCounter to prevent multiple dive start records if the user is hanging out between 3 and 4 feet.
  if (diveMode == true && depth < 4) { 
    diveMode = false;
  }

  if (diveMode) {
    diveTime = now.unixtime() - diveStart;
  }

  display.clearDisplay();

  if (!alertShowing) {
    // 0 = menu, 1 = dive, 2 = log, 3 = settings
    if (displayMode == 0) {
      // Menu Display
      drawMenu();
    } 
    else if (displayMode == 1) {
      // Dive Display
      if (diveModeDisplay == 0) {
        drawDiveDiveDiplayB();
      } 
      else if (diveModeDisplay == 1) {
        drawDiveDiveDiplayA();
      } 
      else {
        drawDiveDiveDiplayC();
      }
    }
    else if (displayMode == 2) {
      // Log Book Display
      drawLogBook();
    }

    else if (displayMode == 3) {
      // Settings Display
      drawSettings();
    }
  }
  display.display();
}


void drawLogBook() {
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Log Book under construction.");

  display.fillRect(0, 55, 13, 9, WHITE);
  display.setTextColor(BLACK);
  display.setCursor( 1, 56 );
  display.setTextSize(1);
  display.println("<<");

}

void drawSettings() {
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Settings under construction.");  

  display.fillRect(0, 55, 13, 9, WHITE);
  display.setTextColor(BLACK);
  display.setCursor( 1, 56 );
  display.setTextSize(1);
  display.println("<<");

}


void drawMenu() {
  display.setTextColor(WHITE);

  drawClock(0, 0, 1, false); // Parameters: x, y, size, bold
  drawTemp(97, 0, 1, false);
  drawBattery(50, 5);

  display.setTextSize(1);
  if (currentMenuOption == 0) {
    display.setCursor( 37, 56 );      
    display.println("Dive Mode");
    display.drawBitmap(52, 22, dive_logo_bmp, 24, 24, WHITE);


  } 
  else if (currentMenuOption == 1) {
    display.setCursor( 40, 56 );      
    display.println("View Log");
    display.drawBitmap(52, 22, log_logo_bmp, 24, 24, WHITE);

  } 
  else if (currentMenuOption == 2) {
    display.setCursor( 40, 56 );      
    display.println("Settings");
    display.drawBitmap(52, 22, settings_logo_bmp, 24, 24, WHITE);

  }

  display.fillRect(115, 55, 13, 9, WHITE);
  display.setTextColor(BLACK);
  display.setCursor( 116, 56 );
  display.setTextSize(1);
  display.println(">>");

  display.fillRect(0, 55, 19, 9, WHITE);
  display.setTextColor(BLACK);
  display.setCursor( 1, 56 );
  display.setTextSize(1);
  display.println("SEL");

  display.setTextColor(WHITE);

  if (diveMode) {
    drawDepth( depth, 0, 26, 1, true, "Depth:" ); // Parameters: depth, x, y, size, bold, header string
    //drawDiveTime(diveTime, 71, 26, 1, true); // Parameters: Time in seconds, x, y, size, bold
  }  
}


// *******************************************************************
// ************************ User Interactions ************************
// *******************************************************************

void rightPressed() {
  if (alertShowing) {
    killAlert();
    return;
  }
  if (displayMode == 0) { // Menu
    currentMenuOption ++;
    if (currentMenuOption > 2) currentMenuOption = 0;
  }

  if (displayMode == 1) { // Dive
    diveModeDisplay++;
    if ( diveModeDisplay > 2 ) diveModeDisplay = 0;
  }
}

void leftPressed() {

  if (displayMode == 0) { // Menu
    displayMode = currentMenuOption + 1;
    return;
  }

  if (displayMode == 1) { // Dive
    displayMode = 0; 
    return;
  }

  if (displayMode == 2) { // Log
    displayMode = 0; 
    return;
  }

  if (displayMode == 3) { // Settings
    displayMode = 0; 
    return;
  }


}

// *******************************************************************
// *********************** Dive Mode Displays ************************
// *******************************************************************

void drawDiveDiveDiplayA() {

  if (depth > maxDepth) maxDepth = depth;
  display.setTextColor(WHITE);

  //drawDepth( 0, 32, 3, false );
  //drawClock( 0, 0, 2, true);
  drawDiveTime(diveTime, 61, 0, 2, false); // Parameters: Time in seconds, x, y, size, bold
  drawClock(61, 25, 1, false); // Parameters: x, y, size, bold
  drawDepth( maxDepth, 6, 32, 3, false, "Max:"); // Parameters: depth, x, y, size, bold, header string
  drawDepth( depth, 6, 0, 3, false, "Depth:"); // Parameters: depth, x, y, size, bold, header string
  drawSaturation(nitrogen, true);
  drawSaturation(oxygen, false);
  drawTemp(61, 42, 1, false);

  display.fillRect(115, 55, 13, 9, WHITE);
  display.setTextColor(BLACK);
  display.setCursor( 116, 56 );
  display.setTextSize(1);
  display.println(">>");

  display.fillRect(0, 55, 13, 9, WHITE);
  display.setTextColor(BLACK);
  display.setCursor( 1, 56 );
  display.setTextSize(1);
  display.println("<<");

  nitrogen++;
  oxygen++;

  if (nitrogen > 255) nitrogen = 0;
  if (oxygen > 255) oxygen = 0;

  logData();

}

void drawDiveDiveDiplayB() {

  if (depth > maxDepth) maxDepth = depth;
  display.setTextColor(WHITE);

  drawDiveTime(diveTime, 56 ,31, 2, false); // Parameters: Time in seconds, x, y, size, bold
  //  drawClock(61, 0, 1, false); // Parameters: x, y, size, bold
  drawDepth( maxDepth, 14, 31, 2, false, "Max:"); // Parameters: depth, x, y, size, bold, header string
  drawDepth( depth, 6, 0, 3, true, "Depth:"); // Parameters: depth, x, y, size, bold, header string
  //  drawTemp(92, 0, 1, false);

  drawTimeTempBar(14, 55, 100);

  drawSaturation(nitrogen, true);
  drawSaturation(oxygen, false);

  display.fillRect(115, 55, 13, 9, WHITE);
  display.setTextColor(BLACK);
  display.setCursor( 116, 56 );
  display.setTextSize(1);
  display.println(">>");

  display.fillRect(0, 55, 13, 9, WHITE);
  display.setTextColor(BLACK);
  display.setCursor( 1, 56 );
  display.setTextSize(1);
  display.println("<<");

  nitrogen++;
  oxygen++;

  logData();

}

void drawDiveDiveDiplayC() {

  if (depth > maxDepth) maxDepth = depth;
  display.setTextColor(WHITE);

  //drawDiveTime(diveTime, 76 ,31, 1, true); // Parameters: Time in seconds, x, y, size, bold
  //  drawClock(61, 0, 1, false); // Parameters: x, y, size, bold
  //drawDepth( maxDepth, 14, 31, 2, false, "Max:"); // Parameters: depth, x, y, size, bold, header string
  drawDepth( depth, 26, 0, 5, true, "Depth:"); // Parameters: depth, x, y, size, bold, header string
  //  drawTemp(92, 0, 1, false);

  drawTimeTempBar(14, 55, 100);

  drawSaturation(nitrogen, true);
  drawSaturation(oxygen, false);

  display.fillRect(115, 55, 13, 9, WHITE);
  display.setTextColor(BLACK);
  display.setCursor( 116, 56 );
  display.setTextSize(1);
  display.println(">>");

  display.fillRect(0, 55, 13, 9, WHITE);
  display.setTextColor(BLACK);
  display.setCursor( 1, 56 );
  display.setTextSize(1);
  display.println("<<");

  nitrogen++;
  oxygen++;

  logData();

}


// *******************************************************************
// ************************** Record Data ****************************
// *******************************************************************

void logData() {
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

String generateRecordDataString() {

  String returnString = "Nitrogen: ";
  returnString += String (nitrogen);
  return returnString;  
}



// *******************************************************************
// ************************ Draw Saturation **************************
// *******************************************************************

void drawSaturation(int value, boolean nitrogen) {

  float level = ( 47.0 / 255.0 ) * value;

  display.setTextSize(1);
  int x = 0;
  display.setTextColor(WHITE);
  if (nitrogen) {
    display.setCursor(0,47);

    display.println("N");
  } 
  else {
    display.setCursor(122,47);

    display.println("O");
    x = 122;
  }
  display.fillRect(x, 47 - level, 5, level, WHITE);
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

  String timeString = createTimeString(false);

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

String createTimeString(boolean amPm) {
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

  if (time12Hour) {
    if (now.hour() > 12) {
      timeString = String( timeString + "PM" );
    } 
    else {
      timeString = String( timeString + "AM" );
    }
  }

  timeString.replace('0', 'O');

  return timeString;
}

// *******************************************************************
// *************************** Draw Temp ****************************
// *******************************************************************

// Good up to three digits. Sizes 1-4 are valid
void drawTemp(int x, int y, int size, boolean bold) {

  int headerLength = 5; // "Temp:"
  int headerWidth = ( 6 * headerLength);

  int width = ( ( 6 * 4 ) * size) - size + 1;
  int height = (7 * size) + HEADER_DATA_GAP;

  if ( width < headerWidth ) width = headerWidth;

  int tempWidth = 10 * size;

  if (temperature > 9) {
    tempWidth = 16 * size;
  }
  if (temperature > 99) {
    tempWidth = 21 * size;
  }

  display.setCursor( ( ( width - headerWidth ) / 2) + x + 1, y );
  display.setTextSize(1);
  display.println("Temp:");

  display.setTextSize(size);
  display.setCursor( ( ( width - tempWidth ) / 2 ) + x + 1, y + HEADER_DATA_GAP );
  String depthString =  String(temperature);
  depthString.replace( '0', 'O' );

  if (ferinheight ) {
    depthString += String('F');
  } 
  else {
    depthString += String('C');
  }
  display.print(depthString);

  if ( bold ) {
    display.setCursor( ( ( ( width - tempWidth ) / 2 ) + x) + 2, y + HEADER_DATA_GAP );
    display.print(depthString);
  }

#ifdef SHOW_LAYOUT
  display.drawRect(x, y, width, height, WHITE);
#endif
}

// *******************************************************************
// ************************** Draw Battery ***************************
// *******************************************************************

void drawBattery(int x, int y) {
  int levelWidth = ( 24.0f / 255.0f ) * batteryLevel;
  display.drawRect(x, y, 24, 8, WHITE);
  display.fillRect(x, y, levelWidth, 8, WHITE);
  display.drawLine(x + 24, y + 2, x + 24, y + 5, WHITE);
}


// *******************************************************************
// *********************** Draw Time Temp Bar ************************
// *******************************************************************

// Bar has a fixed height of 9 pixels.
void drawTimeTempBar(int x, int y, int w) {

  display.fillRect(x, y, w, 9, WHITE);
  display.setTextColor(BLACK);
  display.setTextSize(1);

  display.setCursor( x + 1, y + 1 );
  display.print(createTimeString(true));

  String depthString =  String(temperature);
  depthString.replace( '0', 'O' );

  if (ferinheight ) {
    depthString += String(" F");
  } 
  else {
    depthString += String(" C");
  }

  display.setCursor( x + w - (depthString.length() * 6), y + 1 );
  display.print(depthString);

  display.setCursor( ( x + w - 13 ), y - 1 );
  display.write(9);
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

void showAlert(int time, String message) { // time = 0 will ask the user to dismiss. Time is in seconds

  if (!alertShowing) {
    alertShowing = true;
    alertTime = now.unixtime();
    display.clearDisplay();
    display.display();

    display.setTextSize(1);
    display.setCursor( 0, 20 );
    display.print(message);

    if (time == 0) {
      display.fillRect(115, 55, 13, 9, WHITE);
      display.setTextColor(BLACK);
      display.setCursor( 116, 56 );
      display.setTextSize(1);
      display.println("OK");
    }

    clockBlink = false;
    if (clockBlink) {
      display.fillRect(0, 0, 128, 18, BLACK);
      display.setTextColor(WHITE);
      display.setCursor( 35, 2 );
      display.setTextSize(2);
      display.println("ALERT:");
    } 
    else {
      display.fillRect(0, 0, 128, 18, WHITE);
      display.setTextColor(BLACK);
      display.setCursor( 35, 2 );
      display.setTextSize(2);
      display.println("ALERT:");
    } 
    display.display();
  }

  PITimer1.period(1);
  PITimer1.start(blinkAlert); 

  if (time > 0) {
    //    if (now.unixtime() > alertTime + time) {
    //      display.clearDisplay();
    //      display.display();
    //      alertShowing = false;
    //    }  
    PITimer0.period(time);
    PITimer0.start(killAlert); 
  }
}

void killAlert() {
  alertShowing = false;
  PITimer1.stop();
  PITimer0.stop();
  display.clearDisplay();
  display.display();
}

void blinkAlert() {
  PITimer1.clear();
  if (now.unixtime() > lastColonStateChange) {
    lastColonStateChange = now.unixtime();
    clockBlink = !clockBlink;
  }

  if (clockBlink) {
    display.fillRect(0, 0, 128, 18, BLACK);
    display.setTextColor(WHITE);
    display.setCursor( 35, 2 );
    display.setTextSize(2);
    display.println("ALERT:");
  } 
  else {
    display.fillRect(0, 0, 128, 18, WHITE);
    display.setTextColor(BLACK);
    display.setCursor( 35, 2 );
    display.setTextSize(2);
    display.println("ALERT:");
  }
  display.display();
}



































