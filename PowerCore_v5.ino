/*
  Code for running the touchscreen and sensor components used in the power bank project named PowerCore. 
  It contains the drivers for the screen, as well as, the UI for accessing the Voltage, Current, Power, Capacity, and Time to Charge. 
  This also includes the functions for the collecting and displaying values from an INA219 current sensor.
*/
//Libraries
// Adafruit GFX Library - Version: 1.5.3
#include <Adafruit_GFX.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_SPITFT_Macros.h>
#include <gfxfont.h>


#include <SD.h>
#include <SPI.h>

#include <Wire.h>
#include <Adafruit_INA219.h>

// MCUFRIEND_kbv - Version: Latest
#include <MCUFRIEND_kbv.h>
#include <Adafruit_TFTLCD.h> // Hardware-specific library
#include <TouchScreen.h> //Touchscreen library

//Defining pins for LCD
#define LCD_RESET A4 //Reset pin
#define LCD_CS A3 // Chip Select to Analog 3
#define LCD_CD A2 // Command/Data to Analog 2
#define LCD_WR A1 // LCD Write to Analog 1
#define LCD_RD A0 // LCD Read to Analog 0
#define YP A3 // must be an analog pin, use "An" notation!
#define XM A2 // must be an analog pin, use "An" notation!
#define YM 9 // can be a digital pin
#define XP 8 // can be a digital pin

//Define extreme ends of the touchscreen|For calibration
#define TS_MINX 190
#define TS_MAXX 920
#define TS_MINY 190
#define TS_MAXY 900

//Assigning names to color values
#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF
#define DARKGRAY 0x7BEF

//Max and Min Pressure to Register a Touch
#define MINPRESSURE 200
#define MAXPRESSURE 1000

//Variables for timer to update screen
long previousLCDMillis = 0;
long lcdInterval = 100;
bool screenChange = true;

//Variables for SD functions
File sensorData;
const int chipSelect = 10;

//Variables for Sensor Info
float shuntvoltage = 0;
float busvoltage = 0;
float current_mA = 0;
float loadvoltage = 0;
float energy = 0;
Adafruit_INA219 ina219;
//String to Display on screen
String volt;
String mA; 
String mW;
String mWh; 
String Time_Left;

////////////////////////////////////////////////////
//              Screen and UI                     //
////////////////////////////////////////////////////

MCUFRIEND_kbv tft;//Creates TFT object and provides SPI communication
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);//Creates Touchscreen object using specified pins|300 is touch resistance

/*******UI details ********/
#define BUTTON_X 80
#define BUTTON_Y 200
#define BUTTON_W 100
#define BUTTON_H 40
#define BUTTON_TEXTSIZE 2

#define TEXT_X 5
#define TEXT_Y 30
#define TEXT_SPACING 30
#define TEXT_SIZE 2
#define TEXT_COLOR BLACK

/***************************/

//Current USB port info on Screen
int USB_port = 1;

//Creating Buttons to cycle through pages
Adafruit_GFX_Button buttons[2];
char buttonlabels[2][10]= {"Next", "Previous"};
uint16_t buttoncolor = DARKGRAY;

//Creating labels for the information on the display
String Labels[6] = {"Voltage:", "Current:", "Power:", "Capacity:", "TimeLeft:"};

void setup() {
  Serial.begin(9600);
  Serial.println(F("PowerCore Program"));//Name of Battery Program
  Serial1.begin(9600);
  uint16_t ID = tft.readID();//reads ID of screen to start LCD driver
  tft.begin(ID);
  tft.setRotation(1);//rotates screen 90 degrees
  tft.fillScreen(WHITE);
  
  Serial.println("Display: " + String(tft.width()) + String(tft.height())); 
  
  //Power XBee
  pinMode(45, OUTPUT);
  
  //Setup SD
  pinMode(10, OUTPUT);
  sd_setup();
  
  //Initializes Current Sensor 
  Serial.println("Measuring voltage and current with INA219");
  ina219.begin();
  
  //Initializes and Draws Buttons
  buttons[0].initButton(&tft, BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H, buttoncolor, buttoncolor,
  WHITE, buttonlabels[1], BUTTON_TEXTSIZE);
  buttons[0].drawButton();
  buttons[1].initButton(&tft, BUTTON_X + 160, BUTTON_Y, BUTTON_W, BUTTON_H, buttoncolor, buttoncolor,
  WHITE, buttonlabels[0], BUTTON_TEXTSIZE);
  buttons[1].drawButton();

}

//Sets dimensions of Screen to Consts
const int SCREEN_WIDTH = tft.width();
const int SCREEN_HEIGHT = tft.height();

void loop() {

//Check if screen is ready to update
digitalWrite(45, HIGH);
long currentLCDMillis = millis();
  if (currentLCDMillis - previousLCDMillis > lcdInterval){
    previousLCDMillis = currentLCDMillis;
    screenChange = true;
    }

  if (screenChange){
    screenChange = false;
    prepareData();
    displayData();
    saveData();
    }

  digitalWrite(13, HIGH);
  TSPoint p = ts.getPoint();//gets raw coordinates of touchscreen
  //digitalWrite(13, HIGH);
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);

  //Checks if the Screen was Pressed Depending on Threshold
  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
    //Converts raw x and y values to fit screen dimensions
    p.x = (SCREEN_WIDTH - map(p.x, TS_MINX, TS_MAXX, SCREEN_WIDTH, 0));
    p.y = (SCREEN_HEIGHT - map(p.y, TS_MINY, TS_MAXY, SCREEN_HEIGHT, 0));
    
  //Serial.println(String(p.y) + " " + String(p.x));
  }
  
  //Checks if Buttons were Pressed
  for (uint8_t b = 0; b < 2; b++) {
    if (buttons[b].contains(p.y, p.x)) {
      buttons[b].press(true); // tell the button it is pressed
      buttons[b].drawButton(true); //draw invert
      cycleUSBport(b);
      delay(100);
      buttons[b].drawButton(); //draw normal button
    } else {
      buttons[b].press(false); // tell the button it is NOT pressed
    }
  }
  Xtransmission();
}

//CUSTOM FUNCTIONS
//////////////////////////////////////////
//Determines USBport from button presses
void cycleUSBport(int b){
  if (b == 0){
        if (USB_port == 1){
          USB_port = 4;
        } else{
          USB_port --;
        }
  }
  else if (b == 1){
        if (USB_port == 4){
          USB_port = 1; 
        } else{
          USB_port ++;
          Serial.println(USB_port);
        }
  }
}

//////////////////////////////////////////

void displayData() {
  tft.setTextColor(BLACK, WHITE);
  tft.setCursor(5, 10);
  tft.setTextSize(TEXT_SIZE);
  tft.print("USB " + String(USB_port));
  tft.setCursor(TEXT_X, TEXT_Y);
  tft.setTextColor(TEXT_COLOR, WHITE);
  tft.setTextSize(TEXT_SIZE);
  tft.print(volt);
  tft.setCursor(TEXT_X, TEXT_Y + (TEXT_SPACING));
  tft.setTextColor(TEXT_COLOR, WHITE);
  tft.setTextSize(TEXT_SIZE);
  tft.print(mA);
  tft.setCursor(TEXT_X, TEXT_Y + 2 * (TEXT_SPACING));
  tft.setTextColor(TEXT_COLOR, WHITE);
  tft.setTextSize(TEXT_SIZE);
  tft.print(mW);
  tft.setCursor(TEXT_X, TEXT_Y +  3 * (TEXT_SPACING));
  tft.setTextColor(TEXT_COLOR, WHITE);
  tft.setTextSize(TEXT_SIZE);
  tft.print(mWh);
  tft.setCursor((SCREEN_WIDTH / 2) + 50 + TEXT_X, TEXT_Y);
  tft.setTextColor(TEXT_COLOR, WHITE);
  tft.setTextSize(TEXT_SIZE);
  tft.print(Time_Left);
  //Serial.println("Done");
}

//////////////////////////////////////////
//Gather values from current sensor and turns them into strings to display on screen
void prepareData() {
  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();
  loadvoltage = busvoltage + (shuntvoltage / 1000);
  energy = energy + loadvoltage * current_mA / 3600;
  volt = Labels[0] + String(loadvoltage) + "V ";
  mA = Labels[1] + String(current_mA) + "mA  ";
  mW = Labels[2] + String(loadvoltage * current_mA) + "mW  ";
  mWh = Labels[3] + String(energy) + "mWh  ";
  Time_Left = Labels[4] + "1h";
  /*
  Serial.print(volt);
  Serial.println("V");
  Serial.print(current_mA);
  Serial.println("mA");
  Serial.print(loadvoltage * current_mA);
  Serial.println("mW");
  Serial.print(energy);
  Serial.println("mWh");
  */
}


//////////////////////////////////////////
void sd_setup()
{
  
  Serial.print("Initializing SD card...");
  // IF FAILURE OCCURS: 
    if (!SD.begin(chipSelect)) {
      Serial.println("initialization failed!");
      return;
    }
  
  Serial.println("initialization done.");
}

void saveData(){
    if(SD.exists("data.csv")){ // check the card is still there
      // now append new data file
      Serial.println("data.csv exists");
      sensorData = SD.open("data.csv", FILE_WRITE);
      if (sensorData){
      sensorData.println(volt + " " + mA + " " + mW + " " + mWh);
      sensorData.close(); // close the file
    }
  }
  else{
    Serial.println("Error writing to file !");
  }
}
