/******************************************************************
 * This is the Teensy 3.0 code for the DIY Dive Computer. 
 * 
 * Written by Victor Konshin.
 * BSD license, check license.txt for more information.
 * All text above must be included in any redistribution.
 ******************************************************************/

#include <Wire.h>
#include <Adafruit_GFX.h>
//#include "UILibrary.h"
#include <Adafruit_SSD1306.h>
#include <SD.h>
#include "RTClib.h"
#include "PITimer.h"
#include <Bounce.h>
#include <SPI.h>
//#include "LowPower.h"

#define OLED_RESET 3 // OLED Reset pin
//#define SHOW_LAYOUT // Uncomment this to see bounding boxes on UI elements - makes laying out items easier.

Adafruit_SSD1306 display(OLED_RESET); // Allocate for the OLED

// Dive parameter storage
uint32_t  diveStart            = 0; // This stores the dive start time in unixtime.
int       diveTime             = 0; // This stores the current dive time in seconds
int       depth                = 0; // Current Depth
int       maxDepth             = 0; // Max Depth
int       nitrogen             = 0; // Current nitrogen saturation level
int       oxygen               = 0; // Current oxygen saturation level.
int       temperature          = 75; // Current temp.
int       batteryLevel         = 128; // Current battery capacity remaining.
int       displayMode          = 0; // Stores the current display mode: 0 = menu, 1 = dive, 2 = log, 3 = settings. 
int       currentMenuOption    = 0; // Stores which menu option is being displayed. 0 = dive, 1 = log, 2 = settings.
int       diveModeDisplay      = 0; // There are multiple dive mode displays. This is the one currently being viewed.
uint32_t  last8Depths[8];
int       depthsPointer        = 0;
int       accentRate           = 0; // This will go away when accent rate is calculated from recent pressure readings.

unsigned long      lLastDebounceTime    = 0;  // the last time the output pin was toggled
unsigned long      rLastDebounceTime    = 0;  // the last time the output pin was toggled

int       debounceDelay        = 300; // the debounce time; increase if the output flickers

boolean   accentBlink          = false;
boolean   batteryBlink         = false; 
boolean   inWater              = false; // Goes true if the water sensor detects that the device is submerged.
int       inWaterCounter       = 0; //  Counter used to determine if the device is in water.
boolean   diveMode             = false; // Stores dive mode or surface mode state: 0 = surface mode, 1 = dive mode.
boolean   alertShowing         = false; // true = there is an alert showing. 
uint32_t  alertTime            = 0; // Stores time the alert was displayed.
uint32_t  lastDataRecord       = 0; // Stores the last time data was recorded in unixtime.
uint32_t  lastColonStateChange = 0; // Stores the last time the clock was updates to that the blink will happen at 1Hz.

DateTime  now;                      // Stores the current date/time for use throughout the code.
const int sdChipSelect           = 10; // For the SD card breakout board
const int psChipSelect           = 9; // For the SD card breakout board
const int alertLEDPin            = 4; // A blinking LED is attached to this pin.
const int wakeUpPin              = A1; // Pin for water wake up.
const int rightButton            = 5; // Pin for the right button.
const int leftButton             = 6; // Pin for the left button.

// These will be user preferences
boolean   time12Hour           = true; // true = 12 hour time, false = 24 hour time.
boolean   clockBlink           = true; // true = blink the colon, false = don't blink the colon.
boolean   ferinheight          = true; // true = Ferinheight, false = Celsius.
boolean   doubleAccentBelowRec = true; // True = 60ft/min accent rate below recreational limits (130ft). 30ft/min otherwise.
int       recordInterval       = 30; // This is the inteval in which it will record data to the SD card in minutes.

// Pressure sensor constants:
const long c1=2216;
const long c2=4932;
const long c3=356;
const long c4=223;
const long c5=2273;
const long c6=55;

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

static unsigned char PROGMEM up_arrow[] =
{ 
  B00011000, 
  B00111100, 
  B01111110, 
  B11111111 };


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


  pinMode(alertLEDPin, OUTPUT); // Warning LED
  digitalWrite(alertLEDPin, LOW);

  //  Setup the user input buttons.
  pinMode(rightButton,INPUT); // right button
  pinMode(leftButton,INPUT); // left button
  attachInterrupt(rightButton, rightPressed, FALLING);
  attachInterrupt(leftButton, leftPressed, FALLING);

  // Wake up pin is also the "in water" pin
  pinMode(wakeUpPin, INPUT);   

  // Setup Pressure Sensor
  pinMode(psChipSelect, OUTPUT);

  // initialize access to the SD card
  Serial.print("Initializing SD card...");
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(sdChipSelect, OUTPUT);

  // see if the card is present and can be initialized:
  if (!SD.begin(sdChipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    showAlert (0, "Failed to initialize the SD card.  Please make sure the card isinstalled correctly.");
  } 
  else {
    Serial.println("card initialized.");
  }
  //showAlert (0, "You are severly bent and about to die. Youshould surface while you can.");


  // Give everything a half sec to settle
  delay(500);
}

// *******************************************************************
// *******************************************************************
// **************************** Main Loop ****************************
// *******************************************************************
// *******************************************************************

void loop() {

  nitrogen++;
  oxygen++;
  if (nitrogen > 255) nitrogen = 0;
  if (oxygen > 255) oxygen = 0;

  now = DateTime(Teensy3Clock.get());

  // Read Depth 
  depth = analogRead(A0) / 2;
  // Set Max Depth
  if (depth > maxDepth) maxDepth = depth;

  last8Depths[depthsPointer] = depth;
  depthsPointer++;
  if (depthsPointer > 7) depthsPointer = 0;

  // Fake battery level for testing.
  //  batteryLevel -= 1;
  //  if (batteryLevel < 0) batteryLevel = 255;

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
    maxDepth = 0;
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
    logData();
  }

  if (!alertShowing) {
    display.clearDisplay();

    // 0 = menu, 1 = dive, 2 = log, 3 = settings
    if (displayMode == 0) {
      // Menu Display
      drawMenu();
    } 
    else if (displayMode == 1) {
      // Dive Display
      if (diveModeDisplay == 0) {
        drawDiveDiveDiplayA();
      } 
      else if (diveModeDisplay == 1) {
        drawDiveDiveDiplayB();
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
    display.display();
  }
}


void drawLogBook() {

  display.fillRect(0, 0, 128, 9, WHITE);

  display.setTextColor(BLACK);
  display.setTextSize(1);
  display.setCursor(40, 1);
  display.print("Log Book");

  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  display.print("Under construction");

  drawButtonOptions("MENU", ">>", true, false);


}

void drawSettings() {
  display.fillRect(0, 0, 128, 9, WHITE);

  display.setTextColor(BLACK);
  display.setTextSize(1);
  display.setCursor(40, 1);
  display.print("Settings");

  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  display.print("Under construction");

  drawButtonOptions("MENU", ">>", true, false);
}

void drawMenu() {
  display.setTextColor(WHITE);

  //  drawClock(0, 0, 9, 1, false); // Parameters: x, y, size, bold
  //  drawTemp(97, 0, 9, 1, false);
  //  drawBattery(50, 5, 20, false);
  //drawTimeTempBar(0, 0, 128, true);

  display.setTextColor(WHITE);

  display.setTextSize(1);
  if (currentMenuOption == 0) {
    display.setCursor( 37, 44 );      
    display.println("Dive Mode");
    display.drawBitmap(52, 16, dive_logo_bmp, 24, 24, WHITE);
  } 
  else if (currentMenuOption == 1) {
    display.setCursor( 40, 44 );      
    display.println("View Log");
    display.drawBitmap(52, 16, log_logo_bmp, 24, 24, WHITE);
  } 
  else if (currentMenuOption == 2) {
    display.setCursor( 40, 44 );      
    display.println("Settings");
    display.drawBitmap(52, 16, settings_logo_bmp, 24, 24, WHITE);
  }

  display.fillRect(0, 0, 128, 9, WHITE);
  display.setTextColor(BLACK);
  display.setCursor( 36, 1 );
  display.setTextSize(1);
  display.println("MAIN MENU:");

  //
  //  display.fillRect(0, 55, 19, 9, WHITE);
  //  display.setTextColor(BLACK);
  //  display.setCursor( 1, 56 );
  //  display.setTextSize(1);
  //  display.println("SEL");

  drawButtonOptions("SEL", ">>", true, true);


  display.setTextColor(WHITE);

  if (diveMode) {
    drawDepth( depth, 0, 26, 9, 1, true, "Depth:" ); // Parameters: depth, x, y, size, bold, header string
    //drawDiveTime(diveTime, 71, 26, 1, true); // Parameters: Time in seconds, x, y, size, bold
  }  
}


// *******************************************************************
// ************************ User Interactions ************************
// *******************************************************************

void rightPressed() {

  // TODO: Replace debounce code with Debounce library since it provides more functionality.

  if ((millis() - rLastDebounceTime) < debounceDelay) {
    return;
  }

  rLastDebounceTime = millis();

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

  // TODO: Replace debounce code with Debounce library since it provides more functionality.

  if ((millis() - lLastDebounceTime) < debounceDelay) {
    return;
  }

  lLastDebounceTime = millis();

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

  display.setTextColor(WHITE);

  drawDiveTime(diveTime, 62, 0, 9, 2, false); // Parameters: Time in seconds, x, y, size, bold
  //  drawClock(61, 0, 1, false); // Parameters: x, y, size, bold
  drawDepth( maxDepth, 14, 31, 9, 2, false, "Max:"); // Parameters: depth, x, y, size, bold, header string
  drawDepth( depth, 6, 0, 9, 3, true, "Depth:"); // Parameters: depth, x, y, size, bold, header string
  //  drawTemp(92, 0, 1, false);
  // drawAccentRate(96, 31);
  drawAccentRateBars(71, 24);
  drawAccentRate(70, 45, 1, true, true);
  drawTimeTempBar(14, 56, 100, true);

  drawSaturation(nitrogen, true);
  drawSaturation(oxygen, false);

  drawButtonOptions("^^", ">>", true, true);

}

void drawDiveDiveDiplayB() {

  if (depth > maxDepth) maxDepth = depth;
  display.setTextColor(WHITE);

  drawDiveTime(diveTime, 56, 28, 9, 2, false); // Parameters: Time in seconds, x, y, size, bold
  drawDepth( maxDepth, 10, 28, 9, 2, false, "Max:"); // Parameters: depth, x, y, size, bold, header string
  drawDepth( depth, 10, 2, 9, 2, true, "Depth:"); // Parameters: depth, x, y, size, bold, header string
  drawAccentRateArrows(96, 0);

  drawTimeTempBar(14, 55, 100, true);

  drawSaturation(nitrogen, true);
  drawSaturation(oxygen, false);

  drawButtonOptions("^^", ">>", true, true);

}

// ++++++ DEPTH ONLY ++++++
void drawDiveDiveDiplayC() {

  if (depth > maxDepth) maxDepth = depth;
  display.setTextColor(WHITE);

  drawDepth( depth, 20, 0, 13, 5, true, "Depth:"); // Parameters: depth, x, y, size, bold, header string
  drawAccentRateArrows(110, 16);

  drawSaturation(nitrogen, true);
  drawSaturation(oxygen, false);

  drawButtonOptions("^^", ">>", true, true);

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

void drawButtonOptions(String left, String right, boolean showTimeTemp, boolean showBattery) {

  int leftWidth = ( left.length() * 6 ) - 1;
  int rightWidth = ( right.length() * 6 ) - 1;

  display.fillRect(0, 55, leftWidth + 2, 9, WHITE);
  display.setTextColor(BLACK);
  display.setCursor( 1, 56 );
  display.setTextSize(1);
  display.println(left);

  display.fillRect( 128 - ( rightWidth + 2 ), 55, rightWidth + 2, 9, WHITE);
  display.setTextColor(BLACK);
  display.setCursor( 128 - ( rightWidth + 1 ), 56 );
  display.setTextSize(1);
  display.println(right);

  // (leftWidth + rightWidth + 4);
  if (showTimeTemp) {
    drawTimeTempBar(leftWidth + 3, 55, 128 - ( ( leftWidth + 3 ) + ( rightWidth + 3 ) ) , showBattery);
  }
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
void drawDiveTime(unsigned long seconds, int x, int y, int headingGap, int size, boolean bold) {

  String timeString = secondsToString( seconds );

  int width = ( ( 6 * 5) * size) - size;
  int height = (7 * size) + headingGap;
  if ( width < 59 ) width = 59;

  display.setCursor( ( ( width - 59 ) / 2) + x, y );
  display.setTextSize(1);
  display.println("Dive Time:");

  display.setTextSize(size);
  if ( width > 59 ) {
    display.setCursor( x, y + headingGap );
  } 
  else {
    display.setCursor( ( ( width - ( ( 6 * 5 ) * size ) ) / 2 ) + x, y + headingGap);
  }

  display.print(timeString);

  if ( bold ) {
    if ( width > 59 ) {
      display.setCursor( x + 1, y + headingGap );
    } 
    else {
      display.setCursor( ( ( width - ( ( 6 * 5 ) * size ) ) / 2 ) + x + 1, y + headingGap);    
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

void drawClock(int x, int y, int headingGap, int size, boolean bold) {

  String timeString = createTimeString(false);

  int width = ( ( 6 * timeString.length()) * size) - size;
  int height = (7 * size) + headingGap;
  if ( width < 29 ) width = 29;

  display.setCursor( ( ( width - 29 ) / 2) + x, y );
  display.setTextSize(1);
  display.println("Time:");

  display.setTextSize(size);
  if ( width > 29 ) {
    display.setCursor( x, y + headingGap );
  } 
  else {
    display.setCursor( ( ( width - ( ( 6 * 5 ) * size ) ) / 2 ) + x, y + headingGap);
  }

  display.print(timeString);

  if ( bold ) {
    if ( width > 29 ) {
      display.setCursor( x + 1, y + headingGap );
    } 
    else {
      display.setCursor( ( ( width - ( ( 6 * 5 ) * size ) ) / 2 ) + x + 1, y + headingGap);    
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
    if (now.hour() > 11) {
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
void drawTemp(int x, int y, int headingGap, int size, boolean bold) {

  int headerLength = 5; // "Temp:"
  int headerWidth = ( 6 * headerLength);

  int width = ( ( 6 * 4 ) * size) - size + 1;
  int height = (7 * size) + headingGap;

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
  display.setCursor( ( ( width - tempWidth ) / 2 ) + x + 1, y + headingGap );
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
    display.setCursor( ( ( ( width - tempWidth ) / 2 ) + x) + 2, y + headingGap );
    display.print(depthString);
  }

#ifdef SHOW_LAYOUT
  display.drawRect(x, y, width, height, WHITE);
#endif
}

// *******************************************************************
// ************************** Draw Battery ***************************
// *******************************************************************

void drawBattery(int x, int y, float width, boolean color) { // if color = false, battey is white, true = black battery.

  uint16_t colorValue = WHITE;
  if (color) colorValue = BLACK;

  if (batteryLevel < 64) {
    if (batteryBlink) {
      colorValue = BLACK;
      if (color) colorValue = WHITE;
      batteryBlink = false;
    } 
    else {
      batteryBlink = true;
    }
  }

  int levelWidth = ( width / 255.0f ) * batteryLevel;
  display.drawRect(x, y, width, 7, colorValue);
  display.fillRect(x + 1, y + 1, levelWidth - 2, 5, colorValue);
  display.drawLine(x + width, y + 2, x + width, y + 4, colorValue);
}


// *******************************************************************
// *********************** Draw Time Temp Bar ************************
// *******************************************************************

// Bar has a fixed height of 9 pixels.
void drawTimeTempBar( int x, int y, int w, boolean showBattery ) {

  // Draw Bar
  display.fillRect(x, y, w, 9, WHITE);

  // DrawTime
  display.setTextColor(BLACK);
  display.setTextSize(1);

  display.setCursor( x + 1, y + 1 );
  String timeString = createTimeString(true);
  int timeWidth = ( timeString.length() * 6 ) - 1;
  display.print(timeString);

  // Draw Temp
  String tempString = String(temperature);
  tempString.replace( '0', 'O' );

  if (ferinheight ) {
    tempString += String(" F");
  } 
  else {
    tempString += String(" C");
  }

  int tempWidth = ( tempString.length() * 6 ) - 1;

  display.setCursor( x + w - tempWidth - 1, y + 1 );
  display.print(tempString);

  display.setCursor( ( x + w - 13 ), y - 1 );
  display.write(9);

  Serial.print ("x = ");
  Serial.print (x);
  Serial.print (" w = ");
  Serial.print (w);
  Serial.print (" timeWidth = ");
  Serial.print (timeWidth);
  Serial.print (" tempWidth = ");
  Serial.print (tempWidth);


  // Draw Battery
  if (showBattery) {
    int batteryWidth = w - ( timeWidth + tempWidth + 16);
    Serial.print (" batteryWidth = ");
    Serial.println (batteryWidth);

    drawBattery( x + timeWidth + 8, y + 1, batteryWidth, true);
  }
}

// *******************************************************************
// *************************** Draw Depth ****************************
// *******************************************************************


// Good up to three digits. Sizes 1-4 are valid
void drawDepth(int value, int x, int y, int headingGap, int size, boolean bold, String header) {

  int headerLength = header.length();
  int headerWidth = ( 6 * headerLength);

  int width = ( ( 6 * 3 ) * size) - size + 1;
  int height = (7 * size) + headingGap;

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
  display.setCursor( ( ( width - depthWidth ) / 2 ) + x + 1, y + headingGap);
  String depthString =  String(value);
  depthString.replace( '0', 'O' );
  display.print(depthString);

  if ( bold ) {
    display.setCursor( ( ( ( width - depthWidth ) / 2 ) + x) + 2, y + headingGap );
    display.print(depthString);
  }

#ifdef SHOW_LAYOUT
  display.drawRect(x, y, width, height, WHITE);
#endif
}

// *******************************************************************
// ************************ Draw Accent Rate *************************
// *******************************************************************

void drawAccentRate(int x, int y, int textSize, boolean background, boolean black) {

  uint8_t color = WHITE;
  if (black) color = BLACK;

if (background) {
  display.fillRect(x,y,49,9, WHITE); 
}
  display.setCursor( x + 1, y + 1 );
  display.setTextColor(color);
  display.setTextSize(textSize);

  if ( accentRate < 10 && accentRate > -10 ) {
    display.print(" ");
  }
  if ( accentRate < 100 && accentRate > -100 ) {
    display.print(" ");
  }
  if( accentRate == 0 ) {
    display.print(" ");
  } 
  else if (accentRate > 0) {
    display.print("+");
  }

  display.print(accentRate);

  display.print("ft/m");



}

void drawAccentRateArrows(int x, int y) {

  accentRate++; // TODO: Calculate accent rate here.  value in feet per minute.
  if (accentRate > 40) accentRate = -10;

  int accentRateMultiplier = 1;
  if ( depth > 130 && doubleAccentBelowRec ) accentRateMultiplier = 2;

  uint16_t colorValue = WHITE;

  if ( accentRate > ( 30 * accentRateMultiplier) ) {
    if (accentBlink) {
      colorValue = BLACK;
      accentBlink = false;
    } 
    else {
      accentBlink = true;
    }
    display.drawBitmap(x, y, up_arrow, 8, 4, colorValue);
    digitalWrite(alertLEDPin, HIGH);

  } 
  else {
    digitalWrite(alertLEDPin, LOW);
  }

  if (accentRate > ( 24 * accentRateMultiplier) ) {
    display.drawBitmap(x, y + 4, up_arrow, 8, 4, colorValue);
  }

  if (accentRate > ( 18 * accentRateMultiplier)) {
    display.drawBitmap(x, y + 8, up_arrow, 8, 4, colorValue);
  }

  if (accentRate > ( 12 * accentRateMultiplier)) {
    display.drawBitmap(x, y + 12, up_arrow, 8, 4, colorValue);
  }

  if (accentRate > ( 6 * accentRateMultiplier)) {
    display.drawBitmap(x, y + 16, up_arrow, 8, 4, colorValue);
  }

  if (accentRate > 0) {
    display.drawBitmap(x, y + 20, up_arrow, 8, 4, colorValue);
  }

  //  display.setCursor( x, y + 18 );
  //  display.setTextColor(WHITE);
  //  display.setTextSize(1);
  //  display.println(accentRate);

}

void drawAccentRateBars(int x, int y) {

  accentRate++; // TODO: Calculate accent rate here.  value in feet per minute.
  if ( accentRate > 120 ) accentRate = -10;

  int accentRateMultiplier = 1;
  if ( depth > 130 && doubleAccentBelowRec ) accentRateMultiplier = 2;

  uint16_t colorValue = WHITE;

  if (accentRate > ( 30 * accentRateMultiplier) ) {
    if (accentBlink) {
      colorValue = BLACK;
      accentBlink = false;
    } 
    else {
      accentBlink = true;
    }
    display.fillRect(x + 40, y, 7, 20, colorValue);
    digitalWrite(alertLEDPin, HIGH);

  } 
  else {
    digitalWrite(alertLEDPin, LOW);
  }

  if (accentRate > ( 24 * accentRateMultiplier) ) {
    display.fillRect(x + 32, y + 8, 7, 12, colorValue);
  }

  if (accentRate > ( 18 * accentRateMultiplier) ) {
    display.fillRect(x + 24, y + 12, 7, 8, colorValue);
  }

  if (accentRate > ( 12 * accentRateMultiplier) ) {
    display.fillRect(x + 16, y + 16, 7, 4, colorValue);
  }

  if (accentRate > ( 6 * accentRateMultiplier) ) {
    display.fillRect(x + 8, y + 18, 7, 2, colorValue);
  }

  if (accentRate > 0) {
    display.fillRect(x, y + 19, 7, 1, colorValue);
  }

  //  display.setCursor( x, y + 18 );
  //  display.setTextColor(WHITE);
  //  display.setTextSize(1);
  //  display.println(accentRate);

}


void drawLogEntry(int position, DateTime date, int diveNumber) {

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
    digitalWrite(alertLEDPin, HIGH);

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
  digitalWrite(alertLEDPin, LOW);
  PITimer1.stop();
  PITimer0.stop();
  display.clearDisplay();
  display.display();
}

void blinkAlert() {
  //PITimer1.clear();
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

//void enterSleep() {
//    // Allow wake up pin to trigger interrupt on low.
//    attachInterrupt(0, wakeUp, LOW);
//    
//    // Enter power down state with ADC and BOD module disabled.
//    // Wake up when wake up pin is low.
//    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF); 
//    
//    // Disable external pin interrupt on wake up pin.
//    detachInterrupt(0); 
//    
//    // Do something here
//    // Example: Read sensor, data logging, data transmission.
//}
//
//void wakeUp() {
//    // Just a handler for the pin interrupt.
//}









