/*
 * TTGO T-Call Water Monitor with OLED Display
 * Features: Float sensor + Rainfall sensor + OLED display
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DFRobot_RainfallSensor.h"

// ===== I2C & OLED Settings =====
#define CUSTOM_SDA_PIN 21       
#define CUSTOM_SCL_PIN 22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// ===== Float Sensor Settings =====
#define FLOAT_PIN 13
#define MIN_STATE_TIME 5000      // 5 second deadband
#define DEBOUNCE_DELAY 100

// ===== Objects =====
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DFRobot_RainfallSensor_I2C rainfallSensor(&Wire);

// ===== Float Sensor Variables =====
int currentReading = HIGH;
int confirmedState = HIGH;
int lastConfirmedState = HIGH;
unsigned long stateStartTime = 0;
unsigned long lastChangeTime = 0;
bool stateConfirmed = false;

// ===== Display Variables =====
unsigned long lastDisplayUpdate = 0;
#define DISPLAY_UPDATE_INTERVAL 1000  // Update display every second

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize I2C
  Wire.begin(CUSTOM_SDA_PIN, CUSTOM_SCL_PIN);
  
  // Initialize OLED Display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    while(1); // Stop if OLED fails
  }
  
  // Initialize Float Sensor
  pinMode(FLOAT_PIN, INPUT_PULLUP);
  currentReading = digitalRead(FLOAT_PIN);
  confirmedState = currentReading;
  lastConfirmedState = currentReading;
  stateStartTime = millis();
  stateConfirmed = true;
  
  // Initialize Rainfall Sensor
  while(!rainfallSensor.begin()){
    Serial.println("Rainfall sensor init error!");
    displayError("Rainfall Sensor\nInit Error!");
    delay(2000);
  }
  
  // Show startup screen
  displayStartup();
  
  Serial.println("=== WATER MONITORING SYSTEM ===");
  Serial.println("Float sensor: GPIO 14");
  Serial.println("OLED: I2C SDA=21, SCL=22");
  Serial.println("System ready!");
}

void loop() {
  // Update float sensor with deadband
  updateFloatSensor();
  
  // Update display every second
  if (millis() - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }
  
  // Print to serial every 10 seconds
  static unsigned long lastSerialPrint = 0;
  if (millis() - lastSerialPrint > 10000) {
    printToSerial();
    lastSerialPrint = millis();
  }
  
  delay(100);
}

void updateFloatSensor() {
  int newReading = digitalRead(FLOAT_PIN);
  
  // Basic debouncing
  if (newReading != currentReading) {
    delay(DEBOUNCE_DELAY);
    newReading = digitalRead(FLOAT_PIN);
    
    if (newReading != currentReading) {
      currentReading = newReading;
      stateStartTime = millis();
      stateConfirmed = false;
      
      Serial.print("Float state change detected: ");
      Serial.println(currentReading == LOW ? "UP (High Water)" : "DOWN (Low Water)");
    }
  }
  
  // Check deadband timer
  if (!stateConfirmed && (millis() - stateStartTime) >= MIN_STATE_TIME) {
    confirmedState = currentReading;
    stateConfirmed = true;
    
    if (confirmedState != lastConfirmedState) {
      if (confirmedState == LOW) {
        Serial.println("*** CONFIRMED: WATER LEVEL HIGH ***");
      } else {
        Serial.println("*** CONFIRMED: WATER LEVEL LOW ***");
      }
      lastConfirmedState = confirmedState;
      lastChangeTime = millis();
    }
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Title
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("WATER MONITOR");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  
  // Water Level Status
  display.setCursor(0, 15);
  display.setTextSize(1);
  display.print("Water Level: ");
  
  if (stateConfirmed) {
    display.setTextSize(2);
    display.setCursor(0, 25);
    if (confirmedState == LOW) {
      display.println("HIGH");
    } else {
      display.println("LOW");
    }
  } else {
    display.setTextSize(1);
    display.setCursor(0, 25);
    display.println("Confirming...");
    unsigned long remaining = MIN_STATE_TIME - (millis() - stateStartTime);
    display.setCursor(0, 35);
    display.print("Wait: ");
    display.print(remaining / 1000);
    display.println("s");
  }
  
  // Rainfall Data
  display.setTextSize(1);
  display.setCursor(0, 45);
  display.print("Rain 1H: ");
  display.print(rainfallSensor.getRainfall(1));
  display.println("mm");
  
  display.setCursor(0, 55);
  display.print("Rain Total: ");
  display.print(rainfallSensor.getRainfall());
  display.println("mm");
  
  display.display();
}

void displayStartup() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10);
  display.println("WATER");
  display.setCursor(10, 30);
  display.println("MONITOR");
  display.setTextSize(1);
  display.setCursor(10, 50);
  display.println("Initializing...");
  display.display();
  delay(2000);
}

void displayError(String message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("ERROR:");
  display.setCursor(0, 20);
  display.println(message);
  display.display();
}

void printToSerial() {
  Serial.println("=== STATUS REPORT ===");
  
  // Water level
  Serial.print("Water Level: ");
  if (stateConfirmed) {
    Serial.println(confirmedState == LOW ? "HIGH" : "LOW");
  } else {
    Serial.println("CONFIRMING...");
  }
  
  // Rainfall data
  Serial.print("Rainfall 1 Hour: ");
  Serial.print(rainfallSensor.getRainfall(1));
  Serial.println(" mm");
  
  Serial.print("Total Rainfall: ");
  Serial.print(rainfallSensor.getRainfall());
  Serial.println(" mm");
  
  Serial.print("Working Time: ");
  Serial.print(rainfallSensor.getSensorWorkingTime());
  Serial.println(" H");
  
  Serial.print("Raw Tips: ");
  Serial.println(rainfallSensor.getRawData());
  
  Serial.println("====================");
}