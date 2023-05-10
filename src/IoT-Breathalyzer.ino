#include <Wire.h>
#include "Grove_LCD_RGB_Backlight.h"
#include "Particle.h"
#include "neopixel.h"

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

// #define LINEAR_BAC_CALC

#define MAX_ROW 0
#define AVG_ROW 1

#define SENSOR_READ_TIME_DIFFERENCE 2000
#define WARMING_UP_LED_TIME_DIFFERENCE 500
#define READING_LED_TIME_DIFFERENCE 200
#define WARMING_UP_MODE_TIME 5000
#define READING_MODE_TIME 10000
#define COOLDOWN_TIME 10000
#define MS_BETWEEN_SAMPLES 20
#define DEBOUNCE_TIME 50
#define DOUBLE_CLICK_WAIT_TIME 500
#define RECENT_FINISH_HOLD_LED_TIME_MS 10000
#define UPLOAD_PERIOD 1000
#define SOLID 0

#define HIGH_PPM 15000
#define MEDIUM_PPM 10000

#define PIXEL_COUNT 1
#define PIXEL_TYPE WS2812
#define LED_INDEX 0
#define OFF 0

//Pins
#define PIXEL_PIN D3
#define BUTTON_PIN D2
#define MQ3_PIN A1

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
unsigned long int readingLastCalled = 0;
unsigned long int cooldownLastCalled = 0;

const int displayBacklightR = 255;
const int displayBacklightG = 0;
const int displayBacklightB = 0;
int intensity = 100;
int ledFlashOn = 0;
int lastButtonReading = LOW;
int maxRawValue = 0;
int maxPPM = 0;
int avgPPM = 0;
int smallSampleCount = 0;
int fullSampleCount = 0;
int countdown1 = 0;
int countdown2 = 0;
float maxBAC = 0;
float avgBAC = 0;
float smallSampleTotal = 0;
float fullSampleTotal = 0;
float ppm = 0;
float baseLinePPM = 0;
bool watchingButton = false;
bool recentlyFinished = false;

float calculatePPM(float rawValue);
void updateDisplay();
void handleLED(int timeDifference, int color);
BUTTON_ACTION checkButton(int buttonReading);
float calculateBAC(float ppm);

int PixelColorRed = strip.Color(0, intensity, 0);
int PixelColorGreen  = strip.Color(intensity,  0,  0);
int PixelColorYellow = strip.Color(  intensity, intensity, 0);
int PixelColorOff = strip.Color(  0,  0,  0);

void setup() {
  Serial.begin(9600);     // Initialize Serial communication

  pinMode(BUTTON_PIN, INPUT_PULLDOWN); // Setup button input
  strip.begin(); // Begin LED management

  // LCD Setup:
  lcd.begin(16, 2);
  lcd.setRGB(displayBacklightR, displayBacklightG, displayBacklightB);
  lcd.setCursor(0, 0);
  lcd.print("WARMING UP...");

  deviceMode = WARMING_UP;
  stateChangeTime = millis() + WARMING_UP_MODE_TIME;

  // Declare cloud virables
  Particle.variable("avgPPM", avgPPM); //cloud variable declaration of average PPM value
  Particle.variable("maxPPM", maxPPM);

  // Get baseline of PPM
  baseLinePPM = calculatePPM(analogRead(MQ3_PIN));

  // Wait a bit
  delay(100);
}

void loop() {
  currentTime = millis();  // Get the current time and use it when we don't want the tick to change while we're just processing things

  // Check the button
  buttonState = checkButton(digitalRead(BUTTON_PIN));

  // Device mode state machine
  switch (deviceMode) {
    case WARMING_UP: {
      // Wait, flash the led red, and count down how long we have to wait while warming up the mq3 sensor
      static unsigned long int readingLastCalled = millis();
      static int countdown = WARMING_UP_MODE_TIME / 1000;
      if(currentTime > stateChangeTime) {
        deviceMode = IDLE;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("READY...");
      }  

      if(currentTime > nextLedFlashTime) {
        ledFlashOn = !ledFlashOn;
        nextLedFlashTime += WARMING_UP_LED_TIME_DIFFERENCE;
      }  
      Serial.print(ledFlashOn);

      if (millis() - readingLastCalled > 1000) {
        lcd.setCursor(14, 0);
        if(countdown <= 9) {
          lcd.print(0);
          lcd.setCursor(15, 0);
        }

        lcd.print(--countdown);
        readingLastCalled = millis();
      }

      handleLED(WARMING_UP_LED_TIME_DIFFERENCE, PixelColorRed);
      } break;
    case IDLE:
      // Mode when nothing is happening and we're just waiting for a button press
      if (buttonState == PRESSED || buttonState == HOLD) {
        deviceMode = READING;
        stateChangeTime = millis() + READING_MODE_TIME;
        readingLastCalled = millis();
        countdown1 = READING_MODE_TIME / 1000;
        lcd.clear();
        Serial.print("Button press");
      }

      break;
    case READING: {
      // Button has been pressed and we're now reading
      // While we wait for the time to elapse, collect many samples
      // and do plenty of averaging to get an accurate value, as well as check for the max value.
      // Also occasionally show the current value to the user
      if (currentTime > stateChangeTime) {
        deviceMode = COOLDOWN;
        float avgRawValue = fullSampleTotal / fullSampleCount;
        cooldownLastCalled = millis();
        countdown2 = COOLDOWN_TIME / 1000;
        maxPPM = calculatePPM(maxRawValue);
        avgPPM = calculatePPM(avgRawValue);
        maxBAC = calculateBAC(maxRawValue);
        avgBAC = calculateBAC(avgRawValue);

        String StringMaxPPM = String(maxPPM);
        String StringAvgPPM = String(avgPPM);

        Particle.publish("PPMevent", StringMaxPPM);
        Particle.publish("PPMevent2", StringAvgPPM);

        updateDisplay();

        if(avgPPM >= HIGH_PPM) {
          handleLED(SOLID, PixelColorRed);
        } else if (avgPPM >= MEDIUM_PPM) {
          handleLED(SOLID, PixelColorYellow);
        } else {
          handleLED(SOLID, PixelColorGreen);
        }
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
        if(smallSampleAvg > maxRawValue) {
          maxRawValue = smallSampleAvg;
        }

        smallSampleTotal = 0;
        smallSampleCount = 0;

        lcd.setCursor(0, 0);
        lcd.print("READING...");

        if (millis() - readingLastCalled > 1000) {
          lcd.setCursor(14, 0);
          if(countdown1 <= 9) {
            lcd.print(0);
            lcd.setCursor(15, 0);
          }

          lcd.print(--countdown1);
          readingLastCalled = millis();
        }

        lcd.setCursor(0, 1);
        if (displayMode == PPM) {
          lcd.print("PPM:");
          Serial.print("PPM: ");
          lcd.setCursor(4, 1);
          float ppm = calculatePPM(smallSampleAvg);
          lcd.print(ppm);
          Serial.println(ppm);
        } else if (displayMode == BAC) {
          lcd.print("BAC:");
          Serial.print("BAC: ");
          lcd.setCursor(4, 1);
          float bac = calculateBAC(calculatePPM(smallSampleAvg));
          lcd.print(bac);
          Serial.println(bac);
        }
      }

      handleLED(READING_LED_TIME_DIFFERENCE, PixelColorYellow);
    } break;
    case COOLDOWN: {
      // Wait and countdown until cooldown time as elapsed
      if (countdown2 == 0) {
        deviceMode = IDLE;
      }

      if (millis() - cooldownLastCalled > 1000) {
        lcd.setCursor(14, 0);
        if(countdown2 <= 9) {
          lcd.print(0);
          lcd.setCursor(15, 0);
        }

        lcd.print(--countdown2);
        cooldownLastCalled = millis();
      }
    } break;
    default:
      // We somehow aren't in a valid mode. Indicate something is wrong.
      handleLED(READING_LED_TIME_DIFFERENCE, PixelColorRed);
      break;
  }

  strip.show();
}



float calculatePPM(float rawValue) {
  float voltage = rawValue * 0.00122100122; // 0.00122100122 is 5/4095.0, processor is slow so need to avoid division.
  return voltage * 909.090909091; // 909.090909091 is 1000/1.1
}

float calculateBAC(float rawValue) {
  float voltage = rawValue * 0.00122100122; // 0.00122100122 is 5/4095.0, processor is slow so need to avoid division.

#ifdef LINEAR_BAC_CALC
  float ppm = voltage * 909.090909091;

  return (ppm - baseLinePPM) / 4600.0;
#else
  int R0 = 287; // Calculated from doing RS_gas / 60 during one test
  int R2 = 2000;

  float RS_gas = ((5.0 * R2)/voltage) - R2; 
   /*-Replace the value of R0 with the value of R0 in your test -*/
  float ratio = RS_gas/R0;// ratio = RS/R0
  double x = 0.4*ratio;   
  float BAC = pow(x,-1.431);  //BAC in mg/L

  return BAC * 0.0001;
#endif
}

// Method to update the display with Max and avg ppm or bac values
void updateDisplay() {
  lcd.clear();

  if(displayMode == PPM) {
    lcd.setCursor(0, MAX_ROW);
    lcd.print("MAX PPM:");
    lcd.setCursor(8, MAX_ROW);
    lcd.print(maxPPM);

    lcd.setCursor(0, AVG_ROW);
    lcd.print("AVG PPM:");
    lcd.setCursor(8, AVG_ROW);
    lcd.print(avgPPM);
  } else {
    lcd.setCursor(0, MAX_ROW);
    lcd.print("MAX BAC:");
    lcd.setCursor(8, MAX_ROW);
    lcd.print(maxBAC);

    lcd.setCursor(0, AVG_ROW);
    lcd.print("AVG BAC:");
    lcd.setCursor(8, AVG_ROW);
    lcd.print(avgBAC);
  }
}

// LED control function using a desired color and time between flashes
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

// Function to check if the button is currently being held, double clicked or single clicked, and handle debouncing - custom made
BUTTON_ACTION checkButton(int buttonReading) {
  if (!watchingButton && buttonReading == HIGH && lastButtonReading == LOW) {
    buttonHoldBeginTime = millis();
    debounceEndWaitTime += DEBOUNCE_TIME;
    watchingButton = true;
  }
  
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