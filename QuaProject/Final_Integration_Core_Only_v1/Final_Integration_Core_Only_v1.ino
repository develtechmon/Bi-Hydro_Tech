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

// ===== Rainfall Intensity Thresholds (mm per hour) =====
#define LIGHT_RAIN_MAX 10.0            // Light: < 10mm/hr
#define MODERATE_RAIN_MAX 30.0         // Moderate: 10-30mm/hr  
#define HEAVY_RAIN_MAX 60.0            // Heavy: 30-60mm/hr
                                       // Very Heavy: > 60mm/hr
                                       
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

// ===== Rainfall Intensity Classification =====
enum RainfallIntensity {
  NO_RAIN,
  LIGHT,
  MODERATE, 
  HEAVY,
  VERY_HEAVY,
  SENSOR_ERROR
};
RainfallIntensity currentRainIntensity = NO_RAIN;

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

  // Classify water level and rainfall intensity
  classifyWaterLevel();
  classifyRainfallIntensity();

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
void updateFloatSensorNonBlocking(unsigned long currentTime) {
  int newReading = digitalRead(FLOAT_PIN);

  // Check for reading change
  if (newReading != lastReading) {
    lastDebounceTime = currentTime;
    lastReading = newReading;
  }

  // Non-blocking debounce check
  if ((currentTime - lastDebounceTime) >= DEBOUNCE_DELAY) {
    if (newReading != currentReading) {
      // Reading has changed and is stable
      currentReading = newReading;
      stateStartTime = currentTime;
      stateConfirmed = false;

      Serial.print("Float state change detected: ");
      Serial.println(currentReading == LOW ? "UP (High Water)" : "DOWN (Low Water)");
    }
  }

  // Non-blocking deadband check
  if (!stateConfirmed && (currentTime - stateStartTime) >= MIN_STATE_TIME) {
    confirmedState = currentReading;
    stateConfirmed = true;

    if (confirmedState != lastConfirmedState) {
      if (confirmedState == LOW) {
        Serial.println("*** FLOAT: WATER HIGH - ALARM TRIGGERED! ***");
      } else {
        Serial.println("*** FLOAT: WATER LOW - ALARM CLEARED ***");
      }
      lastConfirmedState = confirmedState;
    }
  }
}

void updateUltrasonicSensorNonBlocking(unsigned long currentTime) {
  // Send trigger command at regular intervals
  if (!waitingForResponse && (currentTime - lastUltrasonicTrigger) >= ULTRASONIC_TRIGGER_INTERVAL) {
    ultrasonicSerial.write(COM);
    ultrasonicSentTime = currentTime;
    waitingForResponse = true;
    lastUltrasonicTrigger = currentTime;
    Serial.println("Sent ultrasonic trigger 0x55");
  }

  // Check for response (non-blocking)
  if (waitingForResponse) {
    // Wait a bit more for data to arrive (similar to your working code)
    if ((currentTime - ultrasonicSentTime) >= 50) { // Wait at least 50ms
      
      if (ultrasonicSerial.available() >= 4) {
        Serial.print("Available bytes: ");
        Serial.println(ultrasonicSerial.available());
        
        // Try to read complete frame - using your working method
        if (ultrasonicSerial.read() == 0xFF) {
          buffer_RTT[0] = 0xFF;
          Serial.println("Found frame header 0xFF");

          // Read remaining bytes with small delays (like your working code)
          bool frameComplete = true;
          for (int i = 1; i < 4; i++) {
            unsigned long byteWaitStart = millis();
            while (!ultrasonicSerial.available() && (millis() - byteWaitStart) < 10) {
              // Wait up to 10ms for each byte
            }
            
            if (ultrasonicSerial.available()) {
              buffer_RTT[i] = ultrasonicSerial.read();
            } else {
              Serial.print("Missing byte at position: ");
              Serial.println(i);
              frameComplete = false;
              break;
            }
          }

          if (frameComplete) {
            // Print raw frame for debugging
            Serial.print("Raw frame: ");
            for (int i = 0; i < 4; i++) {
              Serial.print("0x");
              if (buffer_RTT[i] < 16) Serial.print("0");
              Serial.print(buffer_RTT[i], HEX);
              Serial.print(" ");
            }
            Serial.println();
            
            // Validate checksum
            CS = buffer_RTT[0] + buffer_RTT[1] + buffer_RTT[2];
            if (buffer_RTT[3] == CS) {
              int rawDistance = (buffer_RTT[1] << 8) + buffer_RTT[2];
              Serial.print("Raw distance before validation: ");
              Serial.print(rawDistance);
              Serial.println("mm");
              
              // Validate and filter the reading
              validateUltrasonicReading(rawDistance, currentTime);
            } else {
              Serial.print("Checksum error - calculated: 0x");
              Serial.print(CS, HEX);
              Serial.print(", received: 0x");
              Serial.println(buffer_RTT[3], HEX);
              ultrasonicValid = false;
            }
          }
          waitingForResponse = false;
        } else {
          Serial.println("No frame header found, clearing buffer");
          // Clear buffer and try again
          while (ultrasonicSerial.available()) {
            ultrasonicSerial.read();
          }
        }
      } else if ((currentTime - ultrasonicSentTime) >= ULTRASONIC_TIMEOUT) {
        // Timeout - no response received
        Serial.println("Ultrasonic timeout - no response");
        ultrasonicValid = false;
        waitingForResponse = false;
      }
    }
  }

  // Check if we should use last valid reading due to timeout
  if ((currentTime - lastValidReadingTime) > INVALID_READING_TIMEOUT) {
    // Been too long since valid reading - mark as invalid
    ultrasonicValid = false;
    usingLastValidReading = false;
    Serial.println("WARNING: Ultrasonic sensor timeout - no valid readings");
  }

  // Clear any remaining data (non-blocking)
  while (ultrasonicSerial.available()) {
    ultrasonicSerial.read();
  }
}

void validateUltrasonicReading(int rawReading, unsigned long currentTime) {
  bool isValidReading = true;
  
  // Check 1: Is reading within valid range?
  if (rawReading < MIN_VALID_DISTANCE || rawReading > MAX_VALID_DISTANCE) {
    Serial.print("Invalid range: ");
    Serial.print(rawReading);
    Serial.println("mm - using last valid reading");
    isValidReading = false;
  }
  
  // Check 2: Is change too sudden? (only if we have a previous valid reading)
  if (isValidReading && lastValidDistance > 0) {
    int change = abs(rawReading - lastValidDistance);
    if (change > MAX_CHANGE_THRESHOLD) {
      Serial.print("Sudden change detected: ");
      Serial.print(change);
      Serial.println("mm - using last valid reading");
      isValidReading = false;
    }
  }
  
  // Check 3: Special case for 0mm reading (often indicates sensor error)
  if (rawReading == 0) {
    Serial.println("Zero reading detected - using last valid reading");
    isValidReading = false;
  }
  
  if (isValidReading) {
    // Use the new valid reading
    ultrasonicDistance = rawReading;
    lastValidDistance = rawReading;
    reliableDistance = rawReading;
    lastValidReadingTime = currentTime;
    ultrasonicValid = true;
    usingLastValidReading = false;
  } else {
    // Use last valid reading if available and not too old
    if (lastValidDistance > 0 && (currentTime - lastValidReadingTime) <= INVALID_READING_TIMEOUT) {
      ultrasonicDistance = rawReading;  // Store raw reading for debugging
      reliableDistance = lastValidDistance;  // Use last valid for calculations
      ultrasonicValid = true;  // Mark as valid since we have backup
      usingLastValidReading = true;
    } else {
      // No valid backup available
      ultrasonicDistance = rawReading;
      reliableDistance = 0;
      ultrasonicValid = false;
      usingLastValidReading = false;
    }
  }
}

void classifyRainfallIntensity() {
  float rainfall1H = rainfallSensor.getRainfall(1);  // Get 1-hour rainfall
  
  // Check if sensor is working (if no data after reasonable time, might be error)
  float totalRainfall = rainfallSensor.getRainfall();
  
  // Simple sensor health check - if we never get any readings, sensor might be broken
  static bool hasEverHadReading = false;
  static unsigned long systemStartTime = millis();
  
  if (totalRainfall > 0) {
    hasEverHadReading = true;
  }
  
  // If no readings for 1 hour after system start and no rain detected, might be sensor error
  if (!hasEverHadReading && (millis() - systemStartTime) > 3600000) { // 1 hour
    currentRainIntensity = SENSOR_ERROR;
    return;
  }
  
  // Classify based on 1-hour rainfall intensity
  if (rainfall1H < 0.1) {  // Less than 0.1mm (essentially no rain)
    currentRainIntensity = NO_RAIN;
  } else if (rainfall1H < LIGHT_RAIN_MAX) {  // < 10mm
    currentRainIntensity = LIGHT;
  } else if (rainfall1H < MODERATE_RAIN_MAX) {  // 10-30mm
    currentRainIntensity = MODERATE;
  } else if (rainfall1H < HEAVY_RAIN_MAX) {  // 30-60mm
    currentRainIntensity = HEAVY;
  } else {  // > 60mm
    currentRainIntensity = VERY_HEAVY;
  }
}
void classifyWaterLevel() {
  if (!ultrasonicValid) {
    currentWaterStatus = NORMAL;
    return;
  }

  // Use reliable distance for classification (not raw reading)
  if (reliableDistance >= CRITICAL_LEVEL) {
    currentWaterStatus = CRITICAL;
  } else if (reliableDistance >= WARNING_LEVEL_80) {
    currentWaterStatus = WARNING;
  } else if (reliableDistance >= WARNING_LEVEL_50) {
    currentWaterStatus = WATCH;
  } else {
    currentWaterStatus = NORMAL;
  }
}

void handleBuzzerAlertsNonBlocking(unsigned long currentTime) {
  // PRIORITY 1: Float sensor high water alarm (overrides all other alarms)
  if (stateConfirmed && confirmedState == LOW) {
    // Float is HIGH = Water level HIGH = ALARM!
    if (currentTime - lastFloatAlarm >= FLOAT_ALARM_INTERVAL) {
      floatAlarmActive = !floatAlarmActive;
      digitalWrite(BUZZER_PIN, floatAlarmActive);
      lastFloatAlarm = currentTime;
    }
    return; // Exit early - float alarm takes priority over ultrasonic alarms
  }
  
  // PRIORITY 2: Ultrasonic sensor alarms (only when float is not high)
  switch (currentWaterStatus) {
    case CRITICAL:
      // Fast toggle for critical
      if (currentTime - lastBuzzerToggle >= BUZZER_CRITICAL_INTERVAL) {
        buzzerState = !buzzerState;
        digitalWrite(BUZZER_PIN, buzzerState);
        lastBuzzerToggle = currentTime;
      }
      break;

    case WARNING:
      // Handle warning beep timing
      if (!warningBeepActive && (currentTime - lastWarningBeep >= BUZZER_WARNING_INTERVAL)) {
        // Start new beep
        warningBeepActive = true;
        digitalWrite(BUZZER_PIN, HIGH);
        lastWarningBeep = currentTime;
      } else if (warningBeepActive && (currentTime - lastWarningBeep >= BUZZER_BEEP_DURATION)) {
        // End current beep
        warningBeepActive = false;
        digitalWrite(BUZZER_PIN, LOW);
      }
      break;

    case WATCH:
    case NORMAL:
    default:
      // Turn off buzzer for normal conditions
      digitalWrite(BUZZER_PIN, LOW);
      buzzerState = false;
      warningBeepActive = false;
      floatAlarmActive = false;
      break;
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Title
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("WATER MONITOR");
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

  // Ultrasonic Distance with reliability indicator
  display.setCursor(0, 12);
  display.print("Distance: ");
  if (ultrasonicValid) {
    display.print(reliableDistance);  // Show reliable distance
    display.print("mm");
    // Show if using backup reading
    if (usingLastValidReading) {
      display.print("*");  // Asterisk indicates backup reading
    }
  } else {
    display.print("ERROR");
  }
  display.println();

  // Water Level Status with visual indicator
  display.setCursor(0, 22);
  display.print("Level: ");

  switch (currentWaterStatus) {
    case NORMAL:
      display.print("NORMAL");
      display.drawCircle(120, 25, 3, SSD1306_WHITE);
      break;
    case WATCH:
      display.print("WATCH");
      display.fillCircle(120, 25, 3, SSD1306_WHITE);
      break;
    case WARNING:
      display.print("WARNING");
      display.fillRect(115, 22, 6, 6, SSD1306_WHITE);
      break;
    case CRITICAL:
      display.print("CRITICAL");
      display.fillTriangle(117, 19, 123, 19, 120, 28, SSD1306_WHITE);
      break;
  }

  // Float Status with alarm indicator
  display.setCursor(0, 32);
  display.print("Float: ");
  if (stateConfirmed) {
    if (confirmedState == LOW) {
      display.print("HIGH ");
      // Show blinking alarm indicator
      if (floatAlarmActive) {
        display.print("ALARM!");
      } else {
        display.print("alarm");
      }
    } else {
      display.print("LOW");
    }
  } else {
    unsigned long remaining = MIN_STATE_TIME - (millis() - stateStartTime);
    display.print("WAIT ");
    display.print(remaining / 1000);
    display.print("s");
  }

  // Rainfall Data with intensity classification
  display.setCursor(0, 42);
  display.print("Rain 1H: ");
  display.print(rainfallSensor.getRainfall(1), 1);
  display.println("mm");

  display.setCursor(0, 52);
  // Show rainfall intensity instead of total
  display.print("Rain: ");
  switch (currentRainIntensity) {
    case NO_RAIN:
      display.print("None");
      break;
    case LIGHT:
      display.print("Light");
      break;
    case MODERATE:
      display.print("Moderate");
      break;
    case HEAVY:
      display.print("Heavy");
      break;
    case VERY_HEAVY:
      display.print("V.Heavy");
      break;
    case SENSOR_ERROR:
      display.print("ERROR");
      break;
  }

  display.display();
}

void displayStartup() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 15);
  display.println("BI-HYDRO");
  display.setCursor(10, 35);
  display.println("  TECH");
  display.display();
  delay(2000); // Only blocking delay
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
  Serial.println("=== NON-BLOCKING STATUS ===");

  Serial.print("Ultrasonic: ");
  if (ultrasonicValid) {
    Serial.print("Raw=");
    Serial.print(ultrasonicDistance);
    Serial.print("mm, Reliable=");
    Serial.print(reliableDistance);
    Serial.print("mm");
    if (usingLastValidReading) {
      Serial.print(" (using backup)");
    }
    Serial.println();
  } else {
    Serial.println("INVALID");
  }

  Serial.print("Water Status: ");
  switch (currentWaterStatus) {
    case NORMAL:   Serial.println("NORMAL"); break;
    case WATCH:    Serial.println("WATCH"); break;
    case WARNING:  Serial.println("WARNING"); break;
    case CRITICAL: Serial.println("CRITICAL"); break;
  }

  Serial.print("Rainfall Intensity: ");
  switch (currentRainIntensity) {
    case NO_RAIN:      Serial.println("None"); break;
    case LIGHT:        Serial.println("Light (< 10mm/hr)"); break;
    case MODERATE:     Serial.println("Moderate (10-30mm/hr)"); break;
    case HEAVY:        Serial.println("Heavy (30-60mm/hr)"); break;
    case VERY_HEAVY:   Serial.println("Very Heavy (> 60mm/hr)"); break;
    case SENSOR_ERROR: Serial.println("SENSOR ERROR"); break;
  }

  Serial.print("Rainfall 1H: ");
  Serial.print(rainfallSensor.getRainfall(1), 1);
  Serial.println("mm");

  Serial.print("Total Rainfall: ");
  Serial.print(rainfallSensor.getRainfall(), 1);
  Serial.println("mm");

  Serial.print("Float: ");
  if (stateConfirmed) {
    if (confirmedState == LOW) {
      Serial.println("HIGH - ALARM ACTIVE!");
    } else {
      Serial.println("LOW");
    }
  } else {
    Serial.println("CONFIRMING");
  }

  Serial.print("Loop performance: Smooth (no blocking delays)");
  Serial.println();
  Serial.println("===========================");
}