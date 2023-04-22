#include <Wire.h>
#include "Grove_LCD_RGB_Backlight.h"
#include "Particle.h"
#include "neopixel.h"

#define WARMING_UP 0
#define IDLE 1
#define READING 2
#define LOW_READING 3
#define HIGH_READING 4
#define INTERPRET_RESULT 5
#define NUM_MODES 6

#define SENSOR_READ_TIME_DIFFERENCE 2000
#define WARMING_UP_LED_TIME_DIFFERENCE 1000
#define READING_LED_TIME_DIFFERENCE 500
#define WARMING_UP_MODE_TIME 20000

#define PIXEL_COUNT 1
#define PIXEL_TYPE WS2812

//Pins
#define PIXEL_PIN D4
#define BUTTON_PIN D3

rgb_lcd lcd;
int lastButtonReading = LOW;

// Neopixel varaibles
int intensity = 100;
int deviceMode = WARMING_UP;
int ledFlashOn = 0;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, PIXEL_TYPE);

// Define the pin for the MQ-2 sensor
const int mq2Pin = A0;
const int colorR = 255;
const int colorG = 0;
const int colorB = 0;
float ppm;

// Variable for the next time the sensor will be read
unsigned long int nextSensorReadTime;
unsigned long int nextLedFlashTime;
unsigned long int stateChangeTime;

void setup() {
  // Initialize the Serial communication
  Serial.begin(9600);

  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
  strip.begin();

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);

  deviceMode = WARMING_UP;
  stateChangeTime = millis() + WARMING_UP_MODE_TIME;
  
  lcd.setRGB(colorR, colorG, colorB);
  
  // Print a message to the LCD.
  lcd.print("PPM");

  delay(100);
}

void loop() {
  unsigned long int currentTime = millis();  // get the current time

  if(currentTime > nextSensorReadTime) {

    // Read the sensor data
    ppm = readMQ3Sensor(analogRead(mq2Pin));

    // Calculate and store the next time to evaluate
    nextSensorReadTime += SENSOR_READ_TIME_DIFFERENCE;

    // Print the sensor data
    Serial.print("PPM: ");
    Serial.print(ppm);
    Serial.println(" ppm");
  }

  // Check the button
  int buttonReading = digitalRead(BUTTON_PIN);
  if(deviceMode != READING && buttonReading == HIGH && lastButtonReading == LOW) {
    deviceMode = READING;
    // Mode tester: deviceMode = (deviceMode + 1) % NUM_MODES;
    Serial.print("Button press");
  }
  lastButtonReading = buttonReading;

  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.setCursor(0, 1);
  // print the number of seconds since reset:
  lcd.print(ppm);

  // Control RGB LED
  int PixelColorRed = strip.Color(0, intensity, 0);
  int PixelColorGreen  = strip.Color(intensity,  0,  0);
  int PixelColorYellow = strip.Color(  intensity, intensity, 0);
  int PixelColorOff = strip.Color(  0,  0,  0);

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

      break;
    case READING:
      if(currentTime > stateChangeTime) {
        deviceMode = INTERPRET_RESULT;
      }  

      if(currentTime > nextLedFlashTime) {
        ledFlashOn = !ledFlashOn;
        nextLedFlashTime += READING_LED_TIME_DIFFERENCE;
      }  

      if (ledFlashOn) {
        strip.setPixelColor(0, PixelColorYellow);
      } else {
        strip.setPixelColor(0, PixelColorOff);
      }
      break;
    case INTERPRET_RESULT:
      break;
    case LOW_READING:
      strip.setPixelColor(0, PixelColorGreen);
      break;
    case HIGH_READING:
      strip.setPixelColor(0, PixelColorRed);
      break;
    default:
      strip.setPixelColor(0, PixelColorOff);
      break;
  }

  strip.show();

  delay(10);
}

float readMQ3Sensor(int rawValue) {
  float voltage = (float)rawValue * 5.0 / 4095.0;

  // Debug
  Serial.print("MQ-2 Sensor Data:\n");
  Serial.print("Raw Value: ");
  Serial.println(rawValue);
  Serial.print("Voltage: ");
  Serial.print(voltage);
  Serial.println(" V");
  // Debug end

  return (voltage / 1.1) * 1000.0;
}
