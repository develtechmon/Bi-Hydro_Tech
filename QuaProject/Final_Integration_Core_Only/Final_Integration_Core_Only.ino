/*
 * Non-Blocking Water Monitoring System
 * No delays - smooth operation with multiple sensors
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
// ===== Pin Definitions =====
#define FLOAT_PIN 13
#define BUZZER_PIN 15
// ===== Ultrasonic Sensor Settings =====
#define COM 0x55
// ===== Water Level Thresholds (in mm) =====
#define CRITICAL_LEVEL 1000     
#define WARNING_LEVEL_80 800       
#define WARNING_LEVEL_50 500    
#define NORMAL_LEVEL_20 200     
// ===== Timing Constants (all non-blocking) =====
#define MIN_STATE_TIME 5000         // Float deadband
#define DEBOUNCE_DELAY 100          // Float debounce
#define ULTRASONIC_TRIGGER_INTERVAL 200  // Send trigger every 200ms
#define ULTRASONIC_TIMEOUT 100      // Wait max 100ms for response
#define DISPLAY_UPDATE_INTERVAL 1000     // Update display every 1s
#define SERIAL_PRINT_INTERVAL 5000       // Print status every 5s
#define BUZZER_CRITICAL_INTERVAL 500     // Critical buzzer toggle
#define BUZZER_WARNING_INTERVAL 2000     // Warning buzzer interval
#define BUZZER_BEEP_DURATION 200         // Warning beep duration
#define FLOAT_ALARM_INTERVAL 300         // Float high alarm beep every 300ms

// ===== Ultrasonic Reliability Settings =====
#define MIN_VALID_DISTANCE 50           // Minimum valid reading (5cm)
#define MAX_VALID_DISTANCE 6000         // Maximum valid reading (6m)
#define MAX_CHANGE_THRESHOLD 1000       // Max allowed change per reading (10cm)
#define INVALID_READING_TIMEOUT 10000   // Use last valid reading for 10 seconds

// ===== Objects =====
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DFRobot_RainfallSensor_I2C rainfallSensor(&Wire);
HardwareSerial ultrasonicSerial(2);

// ===== Float Sensor Variables =====
int currentReading = HIGH;
int lastReading = HIGH;
int confirmedState = HIGH;
int lastConfirmedState = HIGH;
unsigned long stateStartTime = 0;
unsigned long lastDebounceTime = 0;
bool stateConfirmed = false;

// ===== Ultrasonic Sensor Variables =====
unsigned char buffer_RTT[4] = {0};
uint8_t CS;
int ultrasonicDistance = 0;
int lastValidDistance = 0;              // Store last valid reading
int reliableDistance = 0;               // Distance used for calculations
bool ultrasonicValid = false;
bool usingLastValidReading = false;     // Flag to show we're using stored value
unsigned long lastValidReadingTime = 0; // When we got the last valid reading
unsigned long lastUltrasonicTrigger = 0;
unsigned long ultrasonicSentTime = 0;
bool waitingForResponse = false;

// ===== Water Level Classification =====
enum WaterStatus {
  NORMAL,      
  WATCH,       
  WARNING,     
  CRITICAL     
};
WaterStatus currentWaterStatus = NORMAL;

// ===== Timing Variables =====
unsigned long lastBuzzerToggle = 0;
unsigned long lastWarningBeep = 0;
unsigned long lastFloatAlarm = 0;      // Added for float alarm
unsigned long lastDisplayUpdate = 0;
unsigned long lastSerialPrint = 0;
bool buzzerState = false;
bool warningBeepActive = false;
bool floatAlarmActive = false;          // Added for float alarm state

void setup() {
  Serial.begin(115200);

  // Initialize pins
  pinMode(FLOAT_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize I2C
  Wire.begin(CUSTOM_SDA_PIN, CUSTOM_SCL_PIN);

  // Initialize OLED Display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    while(1);
  }

  // Initialize Ultrasonic Sensor
  ultrasonicSerial.begin(115200, SERIAL_8N1, 33, 32);

  // Initialize Float Sensor
  currentReading = digitalRead(FLOAT_PIN);
  lastReading = currentReading;
  confirmedState = currentReading;
  stateStartTime = millis();
  lastDebounceTime = millis();
  stateConfirmed = true;

  // Initialize Rainfall Sensor
  while(!rainfallSensor.begin()){
    Serial.println("Rainfall sensor init error!");
    displayError("Rainfall Sensor\nInit Error!");
    delay(2000); // Only blocking delay in setup
  }

  // Show startup screen
  displayStartup();

  Serial.println("=== NON-BLOCKING WATER MONITORING SYSTEM ===");
  Serial.println("All operations are non-blocking for smooth performance");
  Serial.println("Float alarm added - triggers when water goes HIGH");
  Serial.println("Ultrasonic reliability added - prevents false 0mm readings");
  Serial.println("System ready!");

  // Initialize timing
  lastDisplayUpdate = millis();
  lastSerialPrint = millis();
  lastUltrasonicTrigger = millis();
  lastValidReadingTime = millis();
}
void loop() {
  unsigned long currentTime = millis();

  // Update all sensors (non-blocking)
  updateFloatSensorNonBlocking(currentTime);
  updateUltrasonicSensorNonBlocking(currentTime);

  // Classify water level
  classifyWaterLevel();

  // Handle alerts (non-blocking) - includes float alarm
  handleBuzzerAlertsNonBlocking(currentTime);

  // Update display (non-blocking timing)
  if (currentTime - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = currentTime;
  }

  // Print to serial (non-blocking timing)
  if (currentTime - lastSerialPrint >= SERIAL_PRINT_INTERVAL) {
    printToSerial();
    lastSerialPrint = currentTime;
  }

  // No delays - loop runs freely!
}

