/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#line 1 "c:/Users/alext/gitRepositories/IoT-Breathalyzer/src/IoT-Breathalyzer.ino"
#include <Wire.h>
#include "Grove_LCD_RGB_Backlight.h"
#include "Particle.h"
#include "neopixel.h"

void setup();
void loop();
float getBAC(float PPM);
#line 6 "c:/Users/alext/gitRepositories/IoT-Breathalyzer/src/IoT-Breathalyzer.ino"
enum DEVICE_MODE
{
  WARMING_UP  = 0,
  IDLE,
  READING,
  NUM_MODES,
  COOLDOWN
};

enum DISPLAY_MODE
{
  PPM,
  BAC
};

enum BUTTON_ACTION
{
  UNPRESSED = 0,
  PRESSED,
  HOLD,
  DOUBLE_CLICK
};

#define MAX_ROW 0
#define AVG_ROW 1
#define MAX_BAC_COL 8
#define AVG_BAC_COL 8
#define MAX_PPM_COL 8
#define AVG_PPM_COL 8

#define SENSOR_READ_TIME_DIFFERENCE 2000
#define WARMING_UP_LED_TIME_DIFFERENCE 1000
#define READING_LED_TIME_DIFFERENCE 500
#define WARMING_UP_MODE_TIME 20000
#define READING_MODE_TIME 10000
#define MS_BETWEEN_SAMPLES 20
#define DEBOUNCE_TIME 50
#define DOUBLE_CLICK_WAIT_TIME 500
#define RECENT_FINISH_HOLD_LED_TIME_MS 10000
#define SOLID 0

#define HIGH_PPM 15000
#define MEDIUM_PPM 10000

#define PIXEL_COUNT 1
#define PIXEL_TYPE WS2812
#define LED_INDEX 0
#define OFF 0

//Pins
#define PIXEL_PIN D4
#define BUTTON_PIN D3
#define MQ3_PIN A0

rgb_lcd lcd;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);

DEVICE_MODE deviceMode = WARMING_UP;
DISPLAY_MODE displayMode = PPM;
BUTTON_ACTION buttonState = UNPRESSED;

// Time Variables
unsigned long int currentTime = 0;
unsigned long int nextSensorReadTime = 0;
unsigned long int lastSensorReadTime = 0;
unsigned long int buttonHoldBeginTime = 0;
unsigned long int debounceEndWaitTime = 0;
unsigned long int nextLedFlashTime = 0;
unsigned long int stateChangeTime = 0;

// Define the pin for the MQ-2 sensor
const int colorR = 255;
const int colorG = 0;
const int colorB = 0;
int intensity = 100;
int ledFlashOn = 0;
int lastButtonReading = LOW;
int maxVoltage = 0;
int maxPPM;
int avgPPM;
int maxBAC;
int avgBAC;
int smallSampleTotal = 0;
int smallSampleCount = 0;
int fullSampleTotal = 0;
int fullSampleCount = 0;
float ppm;

bool watchingButton = false;
bool recentlyFinished = false;

float findPPM(float rawValue);
void updateDisplay();
void handleLED(int timeDifference, int color);
BUTTON_ACTION checkButton(int buttonReading);

int PixelColorRed = strip.Color(0, intensity, 0);
int PixelColorGreen  = strip.Color(intensity,  0,  0);
int PixelColorYellow = strip.Color(  intensity, intensity, 0);
int PixelColorOff = strip.Color(  0,  0,  0);

void setup() {
  Serial.begin(9600);     // Initialize the Serial communication

  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
  strip.begin();

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.setRGB(colorR, colorG, colorB);

  deviceMode = WARMING_UP;
  stateChangeTime = millis() + WARMING_UP_MODE_TIME;

  // Particle.variable("doublePPM", double_ppm); //cloud variable declaration of double PPM value
  //Particle.variable("averagePPM", average_ppm); //cloud variable declaration of average PPM value
  //particle.function('getPPM', readMQ3SensorDOUBLE); //cloud function that returns ppm

  delay(100);
}

void loop() {
  currentTime = millis();  // get the current time

  // Check the button
  buttonState = checkButton(digitalRead(BUTTON_PIN));
  Serial.println(buttonState);
  Serial.print("Device Mode: ");
  Serial.println(deviceMode);

  // if (buttonState == DOUBLE_CLICK) {
  //       if(displayMode == PPM) {
  //         displayMode = BAC;
  //       } else {
  //         displayMode = PPM;
  //       }
  // }

  //static unsigned long int cooldownStartTime = 0;

  // Device mode state machine
  
  switch (deviceMode) {
    case WARMING_UP:
      if(currentTime > stateChangeTime) {
        deviceMode = IDLE;
      }  

      if(currentTime > nextLedFlashTime) {
        ledFlashOn = !ledFlashOn;
        nextLedFlashTime += WARMING_UP_LED_TIME_DIFFERENCE;
      }  
      Serial.print(ledFlashOn);

      if (ledFlashOn) {
        strip.setPixelColor(0, PixelColorRed);
      } else {
        strip.setPixelColor(0, PixelColorOff);
      }
      break;
    case IDLE:
      if (buttonState == PRESSED || buttonState == HOLD) {
        deviceMode = READING;
        stateChangeTime = millis() + READING_MODE_TIME;
        Serial.print("Button press");
      }

      break;
    case READING: {
      // if(!(buttonState == HOLD) || (currentTime > stateChangeTime)) {
      //   deviceMode = COOLDOWN;
      //   updateDisplay();
      // }  

      if (currentTime > stateChangeTime) {
        deviceMode = COOLDOWN;
        updateDisplay();
      }

      if (currentTime - lastSensorReadTime > MS_BETWEEN_SAMPLES) {
        smallSampleTotal += analogRead(MQ3_PIN);
        smallSampleCount++;
        lastSensorReadTime = currentTime;
      }

      int smallSampleAvg = 0;
      if (smallSampleCount == 10) { 
        smallSampleAvg = smallSampleTotal * 0.1;
        fullSampleTotal += smallSampleAvg;
        fullSampleCount++;

        // Determine if the average is the max
        if(smallSampleAvg > maxVoltage) {
          maxVoltage = smallSampleAvg;
        }

        smallSampleTotal = 0;
        smallSampleCount = 0;
      }

      lcd.setCursor(0, 0);
      lcd.print("READING...");
      lcd.setCursor(0, 1);
      if (displayMode == PPM) {
        // lcd.print("PPM:");
        Serial.print("PPM: ");
        // lcd.setCursor(4, 1);
        float ppm = findPPM(smallSampleAvg);
        // lcd.print(ppm);
        Serial.println(ppm);
      } else if (displayMode == BAC) {
        // lcd.print("BAC:");
        Serial.print("BAC: ");
        // lcd.setCursor(4, 1);
        float bac = getBAC(findPPM(smallSampleAvg));
        // lcd.print(bac);
        Serial.println(bac);
      }

      handleLED(READING_LED_TIME_DIFFERENCE, PixelColorYellow);
    } break;
    case COOLDOWN: {
      static unsigned long int cooldownStartTime = millis();
      static int countdown = 20;

      if (countdown == 0) {
        deviceMode = IDLE;
      }

      // lcd.setCursor(0, 0);
      // lcd.print("COOLDOWN...");
      lcd.setCursor(13, 1);
      if (millis() - cooldownStartTime > 1000) {
        lcd.print(countdown--);
      }

      if(avgPPM >= HIGH_PPM) {
        handleLED(SOLID, PixelColorRed);
      } else if (avgPPM >= MEDIUM_PPM) {
        handleLED(SOLID, PixelColorYellow);
      } else {
        handleLED(SOLID, PixelColorGreen);
      }

      } break;
    default:
      handleLED(READING_LED_TIME_DIFFERENCE, PixelColorRed);
      break;
  }
  

  strip.show();
}



float findPPM(float rawValue) {
  float voltage = rawValue * 0.00122100122; // 0.00122100122 is 5/4095.0, processor is slow so need to avoid division.

  // Debug
  // Serial.print("MQ-2 Sensor Data:\n");
  // Serial.print("Raw Value: ");
  // Serial.println(rawValue);
  // Serial.print("Voltage: ");
  // Serial.print(voltage);
  // Serial.println(" V");
  // Debug end

  return voltage * 909.090909091; // 909.090909091 is 1000/1.1
}

float getBAC(float PPM) {
  // float sensor_volt = rawValue/1024.0*5.0;
  // float RS = (5.0-sensor_volt)/sensor_volt; // 
  // float R0 = RS/60.0; // 60 is found using interpolation
  // Serial.print("R0 = ");
  // Serial.println(R0);
  // return R0;

  // float sensor_volt = rawValue/4095*5.0;
  // float RS_gas = (5.0-sensor_volt)/sensor_volt; // omit *RL

  // /*-Replace the name "R0" with the value of R0 in the demo of First Test -*/
  // //float R0 = RS_gas/60.0;
  // float ratio = RS_gas/60;  // ratio = RS/R0   
  // float BAC = 0.1896*ratio*ratio - 8.6178*ratio/10 + 1.0792;   //BAC in mg/L
  // Serial.print("BAC = ");
  // Serial.println(BAC*0.0001);  //convert to g/dL
  // Serial.print("\n\n");
  // return BAC - 1;
  return 0; // TODO
}

void updateDisplay() {

  if(displayMode == PPM) {
    // lcd.setCursor(0, MAX_ROW);
    // lcd.print("MAX PPM:");
    // lcd.setCursor(MAX_PPM_COL, MAX_ROW);
    // lcd.print(maxPPM);

    // lcd.setCursor(1, AVG_ROW);
    // lcd.print("AVG PPM:");
    // lcd.setCursor(AVG_PPM_COL, AVG_ROW);
    // lcd.print(avgPPM);
  } else {
    // lcd.setCursor(0, MAX_ROW);
    // lcd.print("MAX BAC:");
    // lcd.setCursor(MAX_BAC_COL, MAX_ROW);
    // lcd.print(maxBAC);

    // lcd.setCursor(1, AVG_ROW);
    // lcd.print("AVG BAC:");
    // lcd.setCursor(AVG_BAC_COL, AVG_ROW);
    // lcd.print(avgBAC);
  }
}

void handleLED(int timeDifference, int color) {

  if(timeDifference != SOLID) {
    if(currentTime > nextLedFlashTime) {
      ledFlashOn = !ledFlashOn;
      nextLedFlashTime += READING_LED_TIME_DIFFERENCE;
    }

    if (ledFlashOn) {
      strip.setPixelColor(LED_INDEX, color);
    } else {
      strip.setPixelColor(LED_INDEX, 0);
    }
  } else {
    strip.setPixelColor(LED_INDEX, color);
  }
}

// Return true if falling edge of button press
BUTTON_ACTION checkButton(int buttonReading) {
  // Serial.print("Reading: ");
  // Serial.println(buttonReading);
  if (!watchingButton && buttonReading == HIGH && lastButtonReading == LOW) {
    buttonHoldBeginTime = millis();
    debounceEndWaitTime += DEBOUNCE_TIME;
    watchingButton = true;
  }

// Serial.println(watchingButton);
// Serial.println(buttonHoldBeginTime);
  
  if (watchingButton) {
    if (currentTime > debounceEndWaitTime) {

      if(millis() - buttonHoldBeginTime > DOUBLE_CLICK_WAIT_TIME) {
        if(buttonReading == HIGH){
          lastButtonReading = buttonReading;
          return HOLD;
        } else if (buttonReading == LOW) {
          watchingButton = false;
          return UNPRESSED;
        }
      } else if (lastButtonReading == LOW && buttonReading == HIGH) {
        watchingButton = false;
        lastButtonReading = buttonReading;
        return DOUBLE_CLICK;
      }
    } else {
      return UNPRESSED;
    }
  } 

  lastButtonReading = buttonReading;

  return buttonReading == HIGH ? PRESSED : UNPRESSED;
}
